// widget-helper.c
// LiVES
// (c) G. Finch 2012 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// gdk_screen_get_monitor_geometry’ -> gdk_monitor_get_geometry
// gdk_screen_get_monitor_workarea’ -> gdk_monitor_get_workarea
//  gdk_display_get_screen’ ->
// gdk_screen_get_n_monitors  -> gdk_display_get_n_monitors
//  gdk_display_get_device_manager -> gdk_display_get_default_seat
// gdk_device_manager_list_devices’ ->
//  gd_screen_get_width / height ->


// gdk_display_get_default ()

#include "main.h"

// The idea here is to replace toolkit specific functions with generic ones

// TODO - replace as much code in the other files with these functions as possible

// TODO - add for other toolkits, e.g. qt

// static defns
static void set_child_colour_internal(LiVESWidget *, livespointer set_allx);
static void set_child_alt_colour_internal(LiVESWidget *, livespointer set_allx);
static void default_changed_cb(LiVESWidgetObject *, livespointer pspec, livespointer user_data);
static boolean governor_loop(livespointer data) GNU_RETURNS_TWICE;

#define NSLEEP_TIME 5000

/// internal data keys
static cairo_user_data_key_t CAIRO_CTX_KEY;
static cairo_user_data_key_t CAIRO_WIN_KEY;
#define STD_KEY "_wh_is_standard"
#define TTIPS_KEY "_wh_lives_tooltips"
#define TTIPS_OVERRIDE_KEY "_wh_lives_tooltips_override"
#define TTIPS_HIDE_KEY "_wh_lives_tooltips_hide"
#define ROWS_KEY "_wh_rows"
#define COLS_KEY "_wh_cols"
#define CDEF_KEY "_wh_current_default"
#define DEFBUTTON_KEY "_wh_default_button"
#define EXP_LIST_KEY "_wh_expansion_list"
#define LROW_KEY "_wh_layout_row"
#define EXPANSION_KEY "_wh_expansion"
#define JUST_KEY "_wh_justification"
#define WADDED_KEY "_wh_widgets_added"
#define LAYOUT_KEY "_wh_layout"
#define NWIDTH_KEY "_wh_normal_width"
#define FBUTT_KEY "_wh_first_button"
#define HASDEF_KEY "_wh_has_default"
#define ISLOCKED_KEY "_wh_is_locked"
#define WIDTH_KEY "_wh_width"
#define HEIGHT_KEY "_wh_height"
#define CBUTTON_KEY "_wh_cbutton"
#define SPRED_KEY "_wh_sp_red"
#define SPGREEN_KEY "_wh_sp_green"
#define SPBLUE_KEY "_wh_sp_blue"
#define SPALPHA_KEY "_wh_sp_alpha"
#define WARN_IMAGE_KEY "_wh_warn_image"
#define TTIPS_IMAGE_KEY "_wh_warn_image"

static LiVESWindow *modalw = NULL;

#if 0
weed_plant_t *LiVESWidgetObject_to_weed_plant(LiVESWidgetObject *o) {
  int nprops;
  GParamSpec **pspec;
  GObjectClass oclass;
  weed_plant_t *plant;

  if (o == NULL || !G_IS_OBJECT(o)) return NULL;

  plant = weed_plant_new(WEED_PLANT_LIVES);
  weed_set_int_value(plant, WEED_LEAF_LIVES_SUBTYPE, LIVES_WEED_SUBTYPE_WIDGET);

  oclass = G_OBJECT_GET_CLASS(o);

  // get all the properties
  pspec = g_object_class_list_properties(oclass, &nprops);
  // also g_object_interface_list_properties

  if (nprops > 0) {
    GType gtype;
    weed_plant_t **params = (weed_plant_t **)lives_malloc(nprops * sizeof(weed_plant_t *));
    for (i = 0; i < nprops; i++) {
      // check pspec[i]->flags (G_PARAM_READABLE, G_PARAM_WRITABLE...)
      // if (!G_PARAM_EXPLICIT_NOTIFY), we can hook to the notify signal to shadow it
      //

      params[i] = weed_plant_new(WEED_PLANT_PARAMETER);
      weed_set_string_value(params[i], WEED_LEAF_NAME, g_param_spec_get_name(pspec[i]));
      gtype = G_PARAM_SPEC_VALUE_TYPE(pspec[i]);
      switch (gtype) {
      case G_TYPE_STRING:
      case G_TYPE_CHAR:
      case G_TYPE_UCHAR:
        // WEED_SEED_STRING
        break;
      case G_TYPE_FLOAT:
      case G_TYPE_DOUBLE:
        // WEED_SEED_DOUBLE
        break;
      case G_TYPE_INT:
      case G_TYPE_FLAGS:
      case G_TYPE_ENUM:
      case G_TYPE_UINT: {
        int ival;
        g_object_get(o, name, &ival, NULL);
        //g_object_set(o, name, ival, NULL);
        weed_set_int_value(params[i], WEED_LEAF_VALUE, ival);
        break;
      }
      case G_TYPE_BOOLEAN:
        // WEED_SEED_BOOLEAN
        break;
      case G_TYPE_INT64:
      case G_TYPE_UINT64:
      case G_TYPE_LONG:
      case G_TYPE_ULONG:
        // WEED_SEED_INT64
        break;
      case G_TYPE_POINTER:
        // WEED_SEED_VOIDPTR
        break;
      default:
        break;
      }
    }
    params[num_params] = NULL;
    weed_set_plantptr_array(plant, WEED_LEAF_IN_PARAMETERS, nprops, params);
  }
}
#endif


WIDGET_HELPER_LOCAL_INLINE boolean is_standard_widget(LiVESWidget *widget) {
  livespointer val;
  if ((val = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), STD_KEY)) == NULL) return FALSE;
  return (LIVES_POINTER_TO_INT(val));
}

WIDGET_HELPER_LOCAL_INLINE void set_standard_widget(LiVESWidget *widget, boolean is) {
  if (is) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), STD_KEY, LIVES_INT_TO_POINTER(TRUE));
  else {
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), STD_KEY) != NULL)
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), STD_KEY, LIVES_INT_TO_POINTER(FALSE));
  }
}


static void edit_state_cb(LiVESWidgetObject *object, livespointer pspec, livespointer user_data) {
  LiVESWidget *entry = LIVES_WIDGET(object);
  if (lives_entry_get_editable(LIVES_ENTRY(object))) {
    lives_widget_apply_theme3(entry, LIVES_WIDGET_STATE_NORMAL);
  } else {
    lives_widget_apply_theme2(entry, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }
}


static void widget_state_cb(LiVESWidgetObject *object, livespointer pspec, livespointer user_data) {
  // This callback is here because:
  //
  // a) cannot alter the text colour of a button after the initial draw of a button
  // this is because it doesnt have a proper label widget
  // we can only change the background colour, so here we change the border colour via updating the parent container

  // note: if we need a button with changable text colour we must use a toolbar button instead !
  //
  // b) CSS appears broken in gtk+ 3.18.9 and possibly other versions, preventing seeting of colours for
  // non-default states (e.g. insensitive)
  // thus we need to set a callback to listen to "sensitive" changes, and update the colours in response
  //
  // c) it is also easier just to set the CSS colours when the widget state changes than to figure out ahead of time
  //     what the colours should be for each state. Hopefully it doesn't add too much overhead listening for sensitivity
  //     changes and then updating the CSS manually.
  //
  LiVESWidget *widget = (LiVESWidget *)object;
  LiVESWidgetState state;
  boolean woat = widget_opts.apply_theme;

  if (LIVES_IS_PLAYING || !mainw->is_ready) return;

  state = lives_widget_get_state(widget);

#if GTK_CHECK_VERSION(3, 0, 0)
  // backdrop state removes the focus, so ignore it
  //if (state & LIVES_WIDGET_STATE_BACKDROP) return;
#endif

  if (LIVES_IS_BUTTON(widget)) {
    // this is only for non-dialog buttons, they have their own callback (button_state_changed_cb)
    if (!LIVES_IS_TOGGLE_BUTTON(widget))
      default_changed_cb(object, NULL, NULL);
    return;
  }

  widget_opts.apply_theme = TRUE;

  if (LIVES_IS_TOOL_BUTTON(widget)) {
    LiVESWidget *label;
    LiVESWidget *icon = gtk_tool_button_get_icon_widget(LIVES_TOOL_BUTTON(widget));
    if (icon != NULL) {
      // if we have an icon (no label) just update the border
      lives_tool_button_set_border_colour(widget, state, &palette->menu_and_bars);
      widget_opts.apply_theme = woat;
      return;
    }
    label = gtk_tool_button_get_label_widget(LIVES_TOOL_BUTTON(widget));
    if (label != NULL) {
      float dimval;
      LiVESWidgetColor dimmed_fg;
      LiVESList *list, *olist;
      // if we have a label we CAN set the text colours for TOOL_buttons
      // as well as the outline colour
      if (!lives_widget_is_sensitive(widget)) {
        dimval = (0.2 * 65535.);
        lives_widget_color_copy(&dimmed_fg, &palette->normal_fore);
        lives_widget_color_mix(&dimmed_fg, &palette->normal_back, (float)dimval / 65535.);
        lives_tool_button_set_border_colour(widget, state, &dimmed_fg);
        lives_widget_apply_theme_dimmed2(label, state, BUTTON_DIM_VAL);
      } else {
        dimval = (0.6 * 65535.);
        lives_widget_color_copy(&dimmed_fg, &palette->normal_fore);
        lives_widget_color_mix(&dimmed_fg, &palette->normal_back, (float)dimval / 65535.);
        lives_tool_button_set_border_colour(widget, state, &dimmed_fg);
        lives_widget_apply_theme2(label, state, TRUE);
      }
      // menutoolbuttons will also have an arrow
      // since CSS selectors are borked we have to find it by brute force
      olist = list = lives_container_get_children(LIVES_CONTAINER(widget));
      while (list != NULL) {
        widget = (LiVESWidget *)list->data;
        if (LIVES_IS_VBOX(widget)) {
          lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
        }
        list = list->next;
      }
      lives_list_free(olist);
      widget_opts.apply_theme = woat;
      return;
    }
  }
  if (LIVES_IS_LABEL(widget)) {
    // other widgets get dimmed text
    if (!lives_widget_is_sensitive(widget)) {
      set_child_dimmed_colour(widget, BUTTON_DIM_VAL); // insens, themecols 1, child only
    } else set_child_colour(widget, TRUE);
  }
  if (LIVES_IS_ENTRY(widget) || LIVES_IS_COMBO(widget)) {
    // other widgets get dimmed text
    if (!lives_widget_is_sensitive(widget)) {
      set_child_dimmed_colour2(widget, BUTTON_DIM_VAL); // insens, themecols 1, child only
      lives_widget_apply_theme_dimmed2(widget, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    } else {
      if (LIVES_IS_ENTRY(widget) && !LIVES_IS_SPIN_BUTTON(widget))
        edit_state_cb(LIVES_WIDGET_OBJECT(widget), NULL, NULL);
      else {
        if (LIVES_IS_COMBO(widget)) {
          LiVESWidget *entry = lives_combo_get_entry(LIVES_COMBO(widget));
          lives_widget_apply_theme2(entry, LIVES_WIDGET_STATE_NORMAL, TRUE);
          lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_NORMAL, TRUE);
        } else set_child_colour(widget, TRUE);
      }
    }
  }
  widget_opts.apply_theme = woat;
}


LiVESWidget *prettify_button(LiVESWidget *button) {
  if (!widget_opts.apply_theme || button == NULL) return button;

  lives_widget_apply_theme2(button, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
  set_child_colour(button, TRUE);
#else
  set_child_alt_colour(button, TRUE);
  // try to set the insensitive state colours
  lives_widget_apply_theme_dimmed2(button, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
  set_child_dimmed_colour2(button, BUTTON_DIM_VAL); // insens, themecols 2, child only
#endif
  // brute force it if we have to
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(button), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                  LIVES_GUI_CALLBACK(widget_state_cb), NULL);
  widget_state_cb(LIVES_WIDGET_OBJECT(button), NULL, NULL);
  lives_widget_apply_theme2(button, LIVES_WIDGET_STATE_PRELIGHT, TRUE);
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_auto(LiVESWidgetObject *obj, const char *key, livespointer data) {
  lives_widget_object_set_data_full(obj, key, data, lives_free);
}

/// needed because lives_list_free() is a macro
static void lives_list_free_cb(livespointer list) {lives_list_free((LiVESList *)list);}

WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_list(LiVESWidgetObject *obj, const char *key, LiVESList *list) {
  lives_widget_object_set_data_full(obj, key, list, lives_list_free_cb);
}


static void lives_widget_object_unref_cb(livespointer obj) {lives_widget_object_unref((LiVESWidgetObject *)obj);}

WIDGET_HELPER_GLOBAL_INLINE void lives_widget_object_set_data_widget_object(LiVESWidgetObject *obj, const char *key,
    livespointer other) {
  lives_widget_object_set_data_full(obj, key, other, lives_widget_object_unref_cb);
}


// basic functions

////////////////////////////////////////////////////
//lives_painter functions

WIDGET_HELPER_GLOBAL_INLINE lives_painter_t *lives_painter_create_from_surface(lives_painter_surface_t *target) {
  lives_painter_t *cr = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  cr = cairo_create(target);
#endif
#ifdef PAINTER_QPAINTER
  cr = new lives_painter_t(target);
#endif
  return cr;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_t *lives_painter_create_from_widget(LiVESWidget *widget) {
  lives_painter_t *cr = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
#ifdef GUI_GTK
  LiVESXWindow *window = lives_widget_get_xwindow(widget);
#if !GTK_CHECK_VERSION(3, 22, 0)
  cr = gdk_cairo_create(window);
#else
  cairo_region_t *reg;
  GdkDrawingContext *xctx;
  cairo_rectangle_int_t rect;

  rect.x = rect.y = 0;
  rect.width = lives_widget_get_allocation_width(widget);
  rect.height = lives_widget_get_allocation_height(widget);

  reg = cairo_region_create_rectangle(&rect);
  window = gtk_widget_get_window(widget);
  xctx = gdk_window_begin_draw_frame(window, reg);
  cr = gdk_drawing_context_get_cairo_context(xctx);
  cairo_set_user_data(cr, &CAIRO_CTX_KEY, xctx, NULL);
  cairo_set_user_data(cr, &CAIRO_WIN_KEY, window, NULL);
#endif
#endif
#endif
  return cr;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_remerge(lives_painter_t *cr) {
#if GTK_CHECK_VERSION(3, 22, 0)
  GdkDrawingContext *xctx = cairo_get_user_data(cr, &CAIRO_CTX_KEY);
  LiVESXWindow *window = cairo_get_user_data(cr, &CAIRO_WIN_KEY);
  gdk_window_end_draw_frame(window, xctx);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_pixbuf(lives_painter_t *cr, const LiVESPixbuf *pixbuf,
    double pixbuf_x,
    double pixbuf_y) {
  // blit pixbuf to cairo at x,y
#ifdef LIVES_PAINTER_IS_CAIRO
  gdk_cairo_set_source_pixbuf(cr, pixbuf, pixbuf_x, pixbuf_y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QPointF qp(pixbuf_x, pixbuf_y);
  const QImage *qi = (const QImage *)pixbuf;
  cr->drawImage(qp, *qi);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_surface(lives_painter_t *cr, lives_painter_surface_t *surface,
    double x,
    double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_source_surface(cr, surface, x, y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QPointF qp(x, y);
  cr->drawImage(qp, *surface);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_paint(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_paint(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_fill(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_fill(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->fillPath(*(cr->p), cr->pen.color());
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_stroke(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_stroke(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->strokePath(*(cr->p), cr->pen);
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_clip(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_clip(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->setClipPath(*(cr->p), Qt::IntersectClip);
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_destroy(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_destroy(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->end();
  delete cr;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_render_background(LiVESWidget *widget, lives_painter_t *cr, double x,
    double y,
    double width,
    double height) {
#ifdef LIVES_PAINTER_IS_CAIRO
#if GTK_CHECK_VERSION(3, 0, 0)
  GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
  gtk_render_background(ctx, cr, x, y, width, height);
#else
  LiVESWidgetColor color;
  lives_widget_color_copy(&color, &gtk_widget_get_style(widget)->bg[lives_widget_get_state(widget)]);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  lives_painter_set_source_rgba(cr,
                                LIVES_WIDGET_COLOR_SCALE(color.red),
                                LIVES_WIDGET_COLOR_SCALE(color.green),
                                LIVES_WIDGET_COLOR_SCALE(color.blue),
                                LIVES_WIDGET_COLOR_SCALE(color.alpha));
#else
  lives_painter_set_source_rgb(cr,
                               LIVES_WIDGET_COLOR_SCALE(color.red),
                               LIVES_WIDGET_COLOR_SCALE(color.green),
                               LIVES_WIDGET_COLOR_SCALE(color.blue));
#endif
  lives_painter_rectangle(cr, x, y, width, height);
  lives_painter_fill(cr);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_surface_destroy(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_surface_destroy(surf);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  surf->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_surface_reference(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  return cairo_surface_reference(surf);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_new_path(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_new_path(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_translate(lives_painter_t *cr, double x, double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_translate(cr, x, y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QTransform qt;
  qt.translate(x, y);
  cr->setTransform(qt, true);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_line_width(lives_painter_t *cr, double width) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_line_width(cr, width);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->pen.setWidthF(width);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_move_to(lives_painter_t *cr, double x, double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_move_to(cr, x, y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->moveTo(x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_line_to(lives_painter_t *cr, double x, double y) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_line_to(cr, x, y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->lineTo(x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_close_path(lives_painter_t *cr) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_close_path(cr);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_rectangle(lives_painter_t *cr, double x, double y, double width,
    double height) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_rectangle(cr, x, y, width, height);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->addRect(x, y, width, height);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_arc(lives_painter_t *cr, double xc, double yc, double radius, double angle1,
    double angle2) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_arc(cr, xc, yc, radius, angle1, angle2);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  double l = xc - radius;
  double t = yc - radius;
  double w = radius * 2, h = w;
  angle1 = angle1 / M_PI * 180.;
  angle2 = angle2 / M_PI * 180.;
  cr->p->arcTo(l, t, w, h, angle1, angle2 - angle1);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_operator(lives_painter_t *cr, lives_painter_operator_t op) {
  // if op was not LIVES_PAINTER_OPERATOR_DEFAULT, and FALSE is returned, then the operation failed,
  // and op was set to the default
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_operator(cr, op);
  if (op == LIVES_PAINTER_OPERATOR_UNKNOWN) return FALSE;
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->setCompositionMode(op);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_rgb(lives_painter_t *cr, double red, double green, double blue) {
  // r,g,b values 0.0 -> 1.0
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_source_rgb(cr, red, green, blue);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QColor qc(red * 255., green * 255., blue * 255.);
  cr->pen.setColor(qc);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_source_rgba(lives_painter_t *cr, double red, double green, double blue,
    double alpha) {
  // r,g,b,a values 0.0 -> 1.0
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_source_rgba(cr, red, green, blue, alpha);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QColor qc(red * 255., green * 255., blue * 255., alpha * 255.);
  cr->pen.setColor(qc);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_set_fill_rule(lives_painter_t *cr, lives_painter_fill_rule_t fill_rule) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_set_fill_rule(cr, fill_rule);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->setFillRule(fill_rule);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_painter_surface_flush(lives_painter_surface_t *surf) {
#ifdef LIVES_PAINTER_IS_CAIRO
  cairo_surface_flush(surf);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data,
    lives_painter_format_t format,
    int width, int height, int stride) {
  lives_painter_surface_t *surf = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  surf = cairo_image_surface_create_for_data(data, format, width, height, stride);
#endif
#ifdef PAINTER_QPAINTER
  surf = new lives_painter_surface_t(data, format, width, height, stride);
#endif
  return surf;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_image_surface_create(lives_painter_format_t format,
    int width,
    int height) {
  lives_painter_surface_t *surf = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  surf = cairo_image_surface_create(format, width, height);
#endif
#ifdef PAINTER_QPAINTER
  surf = new lives_painter_surface_t(width, height, format);
#endif
  return surf;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t
*lives_xwindow_create_similar_surface(LiVESXWindow *window, lives_painter_content_t cont,
                                      int width, int height) {
  return gdk_window_create_similar_surface(window, cont, width, height);
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_widget_create_painter_surface(LiVESWidget *widget) {
  if (widget)
    return lives_xwindow_create_similar_surface(lives_widget_get_xwindow(widget),
           LIVES_PAINTER_CONTENT_COLOR,
           lives_widget_get_allocation_width(widget),
           lives_widget_get_allocation_height(widget));
  return NULL;
}

////////////////////////// painter info funcs

WIDGET_HELPER_GLOBAL_INLINE lives_painter_surface_t *lives_painter_get_target(lives_painter_t *cr) {
  lives_painter_surface_t *surf = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  surf = cairo_get_target(cr);
#endif
#ifdef PAINTER_QPAINTER
  surf = cr->target;
#endif
  return surf;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_format_stride_for_width(lives_painter_format_t form, int width) {
  int stride = -1;
#ifdef LIVES_PAINTER_IS_CAIRO
  stride = cairo_format_stride_for_width(form, width);
#endif
#ifdef PAINTER_QPAINTER
  stride = width * 4; //TODO !!
#endif
  return stride;
}


WIDGET_HELPER_GLOBAL_INLINE uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *surf) {
  uint8_t *data = NULL;
#ifdef LIVES_PAINTER_IS_CAIRO
  data = cairo_image_surface_get_data(surf);
#endif
#ifdef PAINTER_QPAINTER
  data = (uint8_t *)surf->bits();
#endif
  return data;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_image_surface_get_width(lives_painter_surface_t *surf) {
  int width = 0;
#ifdef LIVES_PAINTER_IS_CAIRO
  width = cairo_image_surface_get_width(surf);
#endif
#ifdef PAINTER_QPAINTER
  width = ((QImage *)surf)->width();
#endif
  return width;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_image_surface_get_height(lives_painter_surface_t *surf) {
  int height = 0;
#ifdef LIVES_PAINTER_IS_CAIRO
  height = cairo_image_surface_get_height(surf);
#endif
#ifdef PAINTER_QPAINTER
  height = ((QImage *)surf)->height();
#endif
  return height;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_painter_image_surface_get_stride(lives_painter_surface_t *surf) {
  int stride = 0;
#ifdef LIVES_PAINTER_IS_CAIRO
  stride = cairo_image_surface_get_stride(surf);
#endif
#ifdef PAINTER_QPAINTER
  stride = ((QImage *)surf)->bytesPerLine();
#endif
  return stride;
}


WIDGET_HELPER_GLOBAL_INLINE lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *surf) {
  lives_painter_format_t format = (lives_painter_format_t)0;
#ifdef LIVES_PAINTER_IS_CAIRO
  format = cairo_image_surface_get_format(surf);
#endif
#ifdef PAINTER_QPAINTER
  format = ((QImage *)surf)->format();
#endif
  return format;
}


////////////////////////////////////////////////////////

WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref(livespointer object) {
#ifdef GUI_GTK
  if (LIVES_IS_WIDGET_OBJECT(object)) g_object_ref(object);
  else {
    LIVES_WARN("Ref of non-object");
    break_me();
    return FALSE;
  }
  return TRUE;
#endif
#ifdef GUI_QT
  static_cast<LiVESWidgetObject *>(object)->inc_refcount();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_unref(livespointer object) {
#ifdef GUI_GTK
  if (LIVES_IS_WIDGET_OBJECT(object)) g_object_unref(object);
  else {
    LIVES_WARN("Unref of non-object");
    return FALSE;
  }
  return TRUE;
#endif
#ifdef GUI_QT
  static_cast<LiVESWidgetObject *>(object)->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref_sink(livespointer object) {
  if (!LIVES_IS_WIDGET_OBJECT(object)) {
    LIVES_WARN("Ref_sink of non-object");
    break_me();
    return FALSE;
  }
  g_object_ref_sink(object);
  return TRUE;
}
#else
WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref_sink(livespointer object) {
  GtkObject *gtkobject;
  if (!LIVES_IS_WIDGET_OBJECT(object)) {
    LIVES_WARN("Ref_sink of non-object");
    return FALSE;
  }
  gtkobject = (GtkObject *)object;
  gtk_object_sink(gtkobject);
  return TRUE;
}
#endif
#endif

#ifdef GUI_QT
WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_object_ref_sink(livespointer object) {
  static_cast<LiVESWidgetObject *>(object)->ref_sink();
  return TRUE;
}
#endif

/// signal handling

typedef struct {
  livespointer instance;
  lives_funcptr_t callback;
  livespointer user_data;
  volatile boolean swapped;
  unsigned long funcid;
  char *detsig;
  boolean is_timer;
  boolean added;
  lives_proc_thread_t proc;
} lives_sigdata_t;

static LiVESList *active_sigdets = NULL;

unsigned long lives_signal_connect_sync(livespointer instance, const char *detailed_signal, LiVESGuiCallback c_handler,
                                        livespointer data,
                                        LiVESConnectFlags flags) {
  unsigned long func_id;
  if (!flags)
    func_id = g_signal_connect(instance, detailed_signal, c_handler, data);
  else if (flags & LIVES_CONNECT_AFTER)
    func_id = g_signal_connect_after(instance, detailed_signal, c_handler, data);
  else
    func_id = g_signal_connect_swapped(instance, detailed_signal, c_handler, data);
  return func_id;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handler_block(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_block(instance, handler_id);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESWidgetObject *obj = static_cast<LiVESWidgetObject *>(instance);
  obj->block_signal(handler_id);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handler_unblock(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_unblock(instance, handler_id);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESWidgetObject *obj = static_cast<LiVESWidgetObject *>(instance);
  obj->unblock_signal(handler_id);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handler_disconnect(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_disconnect(instance, handler_id);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESWidgetObject *obj = static_cast<LiVESWidgetObject *>(instance);
  obj->disconnect_signal(handler_id);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_stop_emission_by_name(livespointer instance, const char *detailed_signal) {
#ifdef GUI_GTK
  g_signal_stop_emission_by_name(instance, detailed_signal);
  return TRUE;
#endif
  return FALSE;
}


static volatile boolean gov_running = FALSE;
static volatile LiVESDialog *dlgtorun = NULL;
static volatile LiVESResponseType dlgresp = LIVES_RESPONSE_NONE;
static volatile LiVESWidgetLoop *dlgloop = NULL;
static volatile boolean was_dest = FALSE;

static void sigdata_free(livespointer data, LiVESWidgetClosure *cl) {
  lives_sigdata_t *sigdata = (lives_sigdata_t *)data;
  if (cl) active_sigdets = lives_list_remove(active_sigdets, sigdata);

  if (sigdata->detsig) {
    lives_free(sigdata->detsig);
  }
  if (sigdata) lives_free(sigdata);
}


static boolean governor_loop(livespointer data) {
  lives_sigdata_t *sigdata = (lives_sigdata_t *)data;
  /// this loop runs in the main thread while callbacks are being run in bg.

  if (mainw->is_exiting) return FALSE;
  if (!sigdata->proc) return FALSE;

  if (dlgtorun) {
    if (sigdata->is_timer) return TRUE;
    g_idle_add(governor_loop, data);
    return FALSE;
  }

  mainw->clutch = TRUE;

reloop:

  if (!lives_proc_thread_check(sigdata->proc)) {
    // signal bg that it can start now...
    gov_running = TRUE;
    lives_proc_thread_sync_ready(sigdata->proc);
    while (mainw->clutch && !mainw->is_exiting && !lives_proc_thread_check(sigdata->proc)) {
      // while any signal handler is running in the bg, we just loop here until either:
      // the task completes, the task wants to run a main loop cycle, or the app exits
      lives_nanosleep(NSLEEP_TIME);
      sched_yield();
    }
  }

  if (mainw->is_exiting) return FALSE; // app exit

  if (!mainw->clutch) {
    // run mainloop
    if (sigdata->is_timer) return TRUE;
    g_idle_add(governor_loop, data);
    return FALSE;
  }

  /// something else might have removed the clutch, so check again
  if (!lives_proc_thread_check(sigdata->proc)) goto reloop;

  // bg handler finished
  gov_running = FALSE;
  // if a timer, set sigdata->swapped
  if (sigdata->is_timer) {
    sigdata->swapped = TRUE;
  } else {
    weed_plant_free(sigdata->proc);
    sigdata->proc = NULL;
  }
  return FALSE;
}


typedef void (*bifunc)(livespointer, livespointer);
typedef boolean(*trifunc)(livespointer, livespointer, livespointer);

static void async_sig_handler(livespointer instance, livespointer data) {
  LiVESWidgetContext *ctx;
  lives_sigdata_t *sigdata = (lives_sigdata_t *)data;

  // possible values: [gtk+] gdk_frame_clock_paint_idle
  // GDK X11 Event source (:0.0)
  // [gio] complete_in_idle_cb
  // null
  // g_print("SOURCE is %s\n", g_source_get_name(g_main_current_source()));
  if (sigdata->instance != instance) return;

  ctx = lives_widget_context_get_thread_default();
  if (!gov_running && (!ctx || ctx == lives_widget_context_default())) {
    lives_thread_attr_t attr = LIVES_THRDATTR_WAIT_SYNC;
    mainw->clutch = TRUE;
    if (sigdata->swapped) {
      sigdata->proc = lives_proc_thread_create(&attr, (lives_funcptr_t)sigdata->callback, -1, "vv", sigdata->user_data, instance);
    } else {
      sigdata->proc = lives_proc_thread_create(&attr, (lives_funcptr_t)sigdata->callback, -1, "vv", instance, sigdata->user_data);
    }
    governor_loop(data);
  } else {
    (*((bifunc)sigdata->callback))(instance, sigdata->user_data);
  }
}


static void async_sig_handler3(livespointer instance, livespointer extra, livespointer data) {
  LiVESWidgetContext *ctx;
  lives_sigdata_t *sigdata = (lives_sigdata_t *)data;
  if (sigdata->instance != instance) return;
  if (!sigdata->detsig) break_me();

  ctx = lives_widget_context_get_thread_default();
  if (!gov_running && (!ctx || ctx == lives_widget_context_default())) {
    lives_thread_attr_t attr = LIVES_THRDATTR_WAIT_SYNC;
    sigdata->proc = lives_proc_thread_create(&attr, sigdata->callback, -1, "vvv", instance, extra,
                    sigdata->user_data);
    governor_loop((livespointer)sigdata);
  } else {
    (*((trifunc)sigdata->callback))(instance, extra, sigdata->user_data);
  }
}


static boolean async_timer_handler(livespointer data) {
  LiVESWidgetContext *ctx;
  lives_sigdata_t *sigdata = (lives_sigdata_t *)data;
  //g_print("SOURCE is %s\n", g_source_get_name(g_main_current_source())); // NULL for timer, GIdleSource for idle
  //g_print("hndling %p %s %p\n", sigdata, sigdata->detsig, (void *)sigdata->detsig);

  if (mainw->is_exiting) return FALSE;

  if (!sigdata->added) {
    ctx = lives_widget_context_get_thread_default();
    if (!ctx || ctx == lives_widget_context_default()) {
      lives_thread_attr_t attr = LIVES_THRDATTR_WAIT_SYNC;
      mainw->clutch = FALSE;
      sigdata->swapped = FALSE;
      sigdata->proc = lives_proc_thread_create(&attr, (lives_funcptr_t)sigdata->callback, WEED_SEED_BOOLEAN,
                      "v", sigdata->user_data);
      sigdata->added = TRUE;
      //governor_loop((livespointer)sigdata);
    } else {
      return (*((LiVESWidgetSourceFunc)sigdata->callback))(sigdata->user_data);
    }
  }

  while (1) {
    if (!governor_loop((livespointer)sigdata)) break;
    if (sigdata->swapped) {
      // get bool result and return
      boolean res = lives_proc_thread_join_boolean(sigdata->proc);
      weed_plant_free(sigdata->proc);
      sigdata->proc = NULL;
      sigdata->swapped = sigdata->added = FALSE;
      if (!res) sigdata_free(sigdata, NULL);
      return res;
    }
    while (lives_widget_context_iteration(NULL, FALSE)); // process until no more events
  }
  // should never reach here
  return FALSE;
}


unsigned long lives_signal_connect_async(livespointer instance, const char *detailed_signal, LiVESGuiCallback c_handler,
    livespointer data, LiVESConnectFlags flags) {
  static size_t notilen = -1;
  lives_sigdata_t *sigdata;
  uint32_t nvals;
  GSignalQuery sigq;

  if (notilen == -1) notilen = lives_strlen(LIVES_WIDGET_NOTIFY_SIGNAL);
  if (!lives_strncmp(detailed_signal, LIVES_WIDGET_NOTIFY_SIGNAL, notilen)) {
    return lives_signal_connect_sync(instance, detailed_signal, c_handler, data, flags);
  }

  g_signal_query(g_signal_lookup(detailed_signal, G_OBJECT_TYPE(instance)), &sigq);
  if (sigq.return_type != 4) {
    return lives_signal_connect_sync(instance, detailed_signal, c_handler, data, flags);
  }

  nvals = sigq.n_params + 2; // add instance, user_data

  if (nvals != 2 && nvals != 3) {
    return lives_signal_connect_sync(instance, detailed_signal, c_handler, data, flags);
  }

  sigdata = (lives_sigdata_t *)lives_calloc(1, sizeof(lives_sigdata_t));
  sigdata->instance = instance;
  sigdata->callback = (lives_funcptr_t)c_handler;
  sigdata->user_data = data;
  sigdata->swapped = (flags & LIVES_CONNECT_SWAPPED) ? TRUE : FALSE;
  sigdata->detsig = lives_strdup(detailed_signal);

  if (nvals == 2) {
    sigdata->funcid = g_signal_connect_data(instance, detailed_signal, LIVES_GUI_CALLBACK(async_sig_handler),
                                            sigdata, sigdata_free, (flags & LIVES_CONNECT_AFTER));
  } else {
    sigdata->funcid = g_signal_connect_data(instance, detailed_signal, LIVES_GUI_CALLBACK(async_sig_handler3),
                                            sigdata, sigdata_free, (flags & LIVES_CONNECT_AFTER));
  }
  active_sigdets = lives_list_prepend(active_sigdets, (livespointer)sigdata);
  return sigdata->funcid;
}


static lives_sigdata_t *find_sigdata(livespointer instance, LiVESGuiCallback func, livespointer data) {
  LiVESList *list = active_sigdets;
  for (; list; list = list->next) {
    lives_sigdata_t *sigdata = (lives_sigdata_t *)list->data;
    if (sigdata->instance == instance && sigdata->callback == (lives_funcptr_t)func
        && sigdata->user_data == data)
      return sigdata;
  }
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_disconnect_by_func(livespointer instance,
    LiVESGuiCallback func,
    livespointer data) {
  /// assume there is only one connection for each .inst / func / data
  lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
  if (sigdata) {
    lives_signal_handler_disconnect(instance, sigdata->funcid);
    return TRUE;
  }
#ifdef GUI_GTK
  g_signal_handlers_disconnect_by_func(instance, func, data);
  return TRUE;
#endif
  return FALSE;  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_block_by_func(livespointer instance,
    LiVESGuiCallback func,
    livespointer data) {
  /// assume there is only one connection for each .inst / func / data
  lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
  if (sigdata) {
    lives_signal_handler_block(instance, sigdata->funcid);
    return TRUE;
  }
#ifdef GUI_GTK
  g_signal_handlers_block_by_func(instance, func, data);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_signal_handlers_unblock_by_func(livespointer instance,
    LiVESGuiCallback func,
    livespointer data) {
  /// assume there is only one connection for each .inst / func / data
  lives_sigdata_t *sigdata = find_sigdata(instance, LIVES_GUI_CALLBACK(func), data);
  if (sigdata) {
    lives_signal_handler_unblock(instance, sigdata->funcid);
    return TRUE;
  }
#ifdef GUI_GTK
  g_signal_handlers_unblock_by_func(instance, func, data);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grab_add(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_grab_add(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grab_remove(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_grab_remove(widget);
  return TRUE;
#endif
  return FALSE;
}


static void _lives_widget_set_sensitive_cb(LiVESWidget *w, void *pstate) {
  boolean state = (boolean)LIVES_POINTER_TO_INT(pstate);
  lives_widget_set_sensitive(w, state);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_sensitive(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
  gtk_widget_set_sensitive(widget, state);
#ifdef GTK_SUBMENU_SENS_BUG
  if (GTK_IS_MENU_ITEM(widget)) {
    LiVESWidget *sub = lives_menu_item_get_submenu(LIVES_MENU_ITEM(widget));
    if (sub != NULL) {
      lives_container_foreach(LIVES_CONTAINER(sub), _lives_widget_set_sensitive_cb, LIVES_INT_TO_POINTER(state));
      gtk_widget_set_sensitive(sub, state);
    }
  }
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->setEnabled(state);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_sensitive(widget);
#endif
#ifdef GUI_QT
  return (static_cast<QWidget *>(widget))->isEnabled();
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_show(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(widget) && (static_cast<LiVESMainWindow *>(widget))->get_position() == LIVES_WIN_POS_CENTER_ALWAYS) {
    QRect primaryScreenGeometry(QApplication::desktop()->screenGeometry());
    widget->move(-50000, -50000);
    (static_cast<QWidget *>(widget))->setVisible(true);
    widget->move((primaryScreenGeometry.width() - widget->width()) / 2.0,
                 (primaryScreenGeometry.height() - widget->height()) / 2.0);
  } else {
    (static_cast<QWidget *>(widget))->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_hide(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_hide(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->setVisible(false);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_show_all(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show_all(widget);

  // recommended to center the window again after adding all its widgets
  if (LIVES_IS_DIALOG(widget)) lives_window_center(LIVES_WINDOW(widget));

  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->show();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_show_now(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show_now(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_destroy(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_destroy(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_realize(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_realize(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw(LiVESWidget *widget) {
#ifdef GUI_GTK
  if (!GTK_IS_WIDGET(widget)) {
    LIVES_WARN("Draw queue invalid widget");
    return FALSE;
  }
  gtk_widget_queue_draw(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_area(LiVESWidget *widget, int x, int y, int width, int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_queue_draw_area(widget, x, y, width, height);
#else
  gtk_widget_queue_draw(widget);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_resize(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_queue_resize(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_size_request(LiVESWidget *widget, int width, int height) {
#ifdef GUI_GTK
  if (LIVES_IS_WINDOW(widget)) lives_window_resize(LIVES_WINDOW(widget), width, height);
  else gtk_widget_set_size_request(widget, width, height);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_minimum_size(LiVESWidget *widget, int width, int height) {
#ifdef GUI_GTK
  GdkGeometry geom;
  GdkWindowHints mask;
  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  if (GTK_IS_WINDOW(toplevel)) {
    geom.min_width = width;
    geom.min_height = height;
    mask = GDK_HINT_MIN_SIZE;
    gtk_window_set_geometry_hints(GTK_WINDOW(toplevel), widget, &geom, mask);
    return TRUE;
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_maximum_size(LiVESWidget *widget, int width, int height) {
#ifdef GUI_GTK
  GdkGeometry geom;
  GdkWindowHints mask;
  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  if (GTK_IS_WINDOW(toplevel)) {
    geom.max_width = width;
    geom.max_height = height;
    mask = GDK_HINT_MAX_SIZE;
    gtk_window_set_geometry_hints(GTK_WINDOW(toplevel), widget, &geom, mask);
    return TRUE;
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_process_updates(LiVESWidget *widget, boolean upd_children) {
#ifdef GUI_GTK
  LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
  LiVESWindow *win, *modalold = modalw;
  boolean was_modal = TRUE;
  if (!ctx || ctx == lives_widget_context_default()) return TRUE;

  if (LIVES_IS_WINDOW(widget)) win = (LiVESWindow *)widget;
  else win = lives_widget_get_window(widget);
  if (win && LIVES_IS_WINDOW(win)) {
    was_modal = lives_window_get_modal(win);
    if (!was_modal) lives_window_set_modal(win, TRUE);
  }

  if (gov_running) {
    mainw->clutch = FALSE;
    while (!mainw->clutch) {
      lives_nanosleep(NSLEEP_TIME);
      sched_yield();
    }
  }

  if (!was_modal) {
    lives_window_set_modal(win, FALSE);
    if (modalold) lives_window_set_modal(modalold, TRUE);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_get_origin(LiVESXWindow *xwin, int *posx, int *posy) {
#ifdef GUI_GTK
  gdk_window_get_origin(xwin, posx, posy);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_get_frame_extents(LiVESXWindow *xwin, lives_rect_t *rect) {
#ifdef GUI_GTK
  gdk_window_get_frame_extents(xwin, (GdkRectangle *)rect);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_invalidate_rect(LiVESXWindow *window, lives_rect_t *rect,
    boolean inv_childs) {
#ifdef GUI_GTK
  gdk_window_invalidate_rect(window, (const GdkRectangle *)rect, inv_childs);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_reparent(LiVESWidget *widget, LiVESWidget *new_parent) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 14, 0)
  GtkWidget *parent = gtk_widget_get_parent(widget);
  g_object_ref(widget);
  if (parent != NULL) {
    gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(widget)), widget);
  }
  gtk_container_add(GTK_CONTAINER(new_parent), widget);
  g_object_unref(widget);
#else
  gtk_widget_reparent(widget, new_parent);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_ancestor(LiVESWidget *widget, LiVESWidget *ancestor) {
#ifdef GUI_GTK
  return gtk_widget_is_ancestor(widget, ancestor);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_app_paintable(LiVESWidget *widget, boolean paintable) {
#ifdef GUI_GTK
  gtk_widget_set_app_paintable(widget, paintable);
  return TRUE;
#endif
  return FALSE;
}


#ifdef GUI_GTK
WIDGET_HELPER_LOCAL_INLINE boolean lives_widget_loop_is_running(volatile LiVESWidgetLoop *loop) {
  return g_main_loop_is_running((LiVESWidgetLoop *)loop);
}
WIDGET_HELPER_LOCAL_INLINE void lives_widget_loop_quit(volatile LiVESWidgetLoop *loop) {
  g_main_loop_quit((LiVESWidgetLoop *)loop);
}
WIDGET_HELPER_LOCAL_INLINE void lives_widget_loop_run(volatile LiVESWidgetLoop *loop) {
  g_main_loop_run((LiVESWidgetLoop *)loop);
}
WIDGET_HELPER_LOCAL_INLINE void lives_widget_loop_unref(volatile LiVESWidgetLoop *loop) {
  g_main_loop_unref((LiVESWidgetLoop *)loop);
}
WIDGET_HELPER_LOCAL_INLINE volatile LiVESWidgetLoop *lives_widget_loop_new(LiVESWidgetContext *ctx,
    boolean running) {
  return g_main_loop_new(ctx, running);
}

static void dlgresponse(LiVESDialog *dialog, LiVESResponseType resp, livespointer data) {
  dlgresp = resp;
  if (dlgloop && lives_widget_loop_is_running(dlgloop))
    lives_widget_loop_quit(dlgloop);
}

static void dlgunmap(LiVESDialog *dialog, livespointer data) {
  if (dlgloop && lives_widget_loop_is_running(dlgloop))
    lives_widget_loop_quit(dlgloop);
}

static void dlgdest(LiVESDialog *dialog, livespointer data) {
  was_dest = TRUE;
  dlgtorun = NULL;
}

static boolean dlgdelete(LiVESDialog *dialog, LiVESXEvent *event, livespointer data) {
  was_dest = TRUE;
  dlgtorun = NULL;
  if (dlgloop && lives_widget_loop_is_running(dlgloop)) lives_widget_loop_quit(dlgloop);
  return TRUE;
}
#endif


WIDGET_HELPER_GLOBAL_INLINE LiVESResponseType lives_dialog_run(LiVESDialog *dialog) {
#ifdef GUI_GTK
  LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();

  lives_widget_show_all(LIVES_WIDGET(dialog));

  if (!ctx || ctx == lives_widget_context_default()) return gtk_dialog_run(dialog);

  if (gov_running) {
    boolean was_modal;

    unsigned long respfunc = lives_signal_sync_connect(LIVES_GUI_OBJECT(dialog), LIVES_WIDGET_RESPONSE_SIGNAL,
                             LIVES_GUI_CALLBACK(dlgresponse), NULL);
    unsigned long delfunc = lives_signal_sync_connect(dialog, LIVES_WIDGET_DELETE_EVENT,
                            LIVES_GUI_CALLBACK(dlgdelete), NULL);
    unsigned long destfunc = lives_signal_sync_connect(dialog, LIVES_WIDGET_DESTROY_SIGNAL,
                             LIVES_GUI_CALLBACK(dlgdest), NULL);
    unsigned long unmapfunc = lives_signal_sync_connect(dialog, LIVES_WIDGET_UNMAP_SIGNAL,
                              LIVES_GUI_CALLBACK(dlgunmap), NULL);

    dlgresp = LIVES_RESPONSE_INVALID;
    was_modal = lives_window_get_modal(LIVES_WINDOW(dialog));
    if (!was_modal) lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);
    dlgtorun = dialog;
    mainw->clutch = FALSE;

    /// needs to run in the def. ctx (?)
    was_dest = FALSE;
    dlgloop = lives_widget_loop_new(NULL, FALSE);
    lives_widget_loop_run(dlgloop);
    dlgtorun = NULL;
    while (!mainw->clutch) {
      lives_nanosleep(NSLEEP_TIME);
      sched_yield();
    }
    lives_widget_loop_unref(dlgloop);
    dlgloop = NULL;
    if (!was_dest) {
      if (!was_modal) lives_window_set_modal(LIVES_WINDOW(dialog), FALSE);
      lives_signal_handler_disconnect(LIVES_GUI_OBJECT(dialog), respfunc);
      lives_signal_handler_disconnect(LIVES_GUI_OBJECT(dialog), destfunc);
      lives_signal_handler_disconnect(LIVES_GUI_OBJECT(dialog), delfunc);
      lives_signal_handler_disconnect(LIVES_GUI_OBJECT(dialog), unmapfunc);
    }
    return dlgresp;
  }

#endif
  return LIVES_RESPONSE_INVALID;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_response(LiVESDialog *dialog, int response) {
#ifdef GUI_GTK
  gtk_dialog_response(dialog, response);
  return TRUE;
#endif
#ifdef GUI_QT
  dialog->setResult(response);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_dialog_get_response_for_widget(LiVESDialog *dialog, LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_dialog_get_response_for_widget(dialog, widget);
#endif
  return LIVES_RESPONSE_NONE;
}


#if GTK_CHECK_VERSION(3, 16, 0)

#define RND_STRLEN 12
#define RND_STR_PREFIX "XXX"

static char *make_random_string(const char *prefix) {
  // give each widget a random name so we can style it individually
  char *str;
  size_t psize = strlen(prefix);
  size_t rsize = RND_STRLEN << 1;
  register int i;

  if (psize > RND_STRLEN) return NULL;

  str = (char *)lives_malloc(rsize);
  lives_snprintf(str, psize + 1, "%s", prefix);

  rsize--;

  for (i = psize; i < rsize; i++) str[i] = ((lives_random() & 15) + 65);
  str[rsize] = 0;
  return str;
}
#endif


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)

static boolean set_css_value_for_state_flag(LiVESWidget *widget, LiVESWidgetState state, const char *selector,
    const char *detail, const char *value) {
  GtkCssProvider *provider;
  GtkStyleContext *ctx;
  char *widget_name, *wname;
  char *css_string, *tmp;
  char *state_str;

  if (widget == NULL || !LIVES_IS_WIDGET(widget)) return FALSE;

#if GTK_CHECK_VERSION(3, 24, 0)
  /* if (!(state & LIVES_WIDGET_STATE_BACKDROP)) */
  /*   set_css_value_for_state_flag(widget, state | LIVES_WIDGET_STATE_BACKDROP, */
  /* selector, detail, value); */
#endif

  ctx = gtk_widget_get_style_context(widget);
  provider = gtk_css_provider_new();
  gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER
                                 (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  widget_name = lives_strdup(gtk_widget_get_name(widget));

  if (widget_name == NULL || (strncmp(widget_name, RND_STR_PREFIX, strlen(RND_STR_PREFIX)))) {
    lives_freep((void **)&widget_name);
    widget_name = make_random_string(RND_STR_PREFIX);
    gtk_widget_set_name(widget, widget_name);
  }

#ifdef GTK_TEXT_VIEW_CSS_BUG
  if (GTK_IS_TEXT_VIEW(widget)) {
    lives_freep((void **)&widget_name);
    widget_name = lives_strdup("GtkTextView");
  } else {
#endif
    switch (state) {
    // TODO: gtk+ 3.x can set multiple states
    case GTK_STATE_FLAG_ACTIVE:
      state_str = ":active";
      break;
    case GTK_STATE_FLAG_FOCUSED:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":focus";
#endif
      break;
    case GTK_STATE_FLAG_PRELIGHT:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":hover";
#else
      state_str = ":prelight";
#endif
      break;
    case GTK_STATE_FLAG_SELECTED:
      state_str = ":selected";
      break;
    case GTK_STATE_FLAG_CHECKED:
      state_str = ":checked";
      break;
    case GTK_STATE_FLAG_INCONSISTENT:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":indeterminate";
#endif
      break;
    case GTK_STATE_FLAG_BACKDROP:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":backdrop";
#endif
      break;
    case GTK_STATE_FLAG_INSENSITIVE:
#if GTK_CHECK_VERSION(3, 18, 0)
      state_str = ":disabled";
#else
      state_str = ":insensitive";
#endif
      break;
    default:
      state_str = "";
    }

#if GTK_CHECK_VERSION(3, 24, 0)
    // special tweaks
    if (selector == NULL) {
      if (GTK_IS_FRAME(widget)) {
        selector = "label";
      }

      if (GTK_IS_TEXT_VIEW(widget)) {
        selector = "text";
        /* tmp = lives_strdup_printf("%s %s text {\n %s: %s;}\n", css_string, wname, detail, value); */
        /* lives_free(css_string); */
        /* css_string = tmp; */
      }

      if (GTK_IS_SPIN_BUTTON(widget)) {
        selector = "*";
        /* tmp = lives_strdup_printf("%s %s text {\n %s: %s;}\n", css_string, wname, detail, value); */
        /* lives_free(css_string); */
        /* css_string = tmp; */
      }
    }
#endif

    if (selector == NULL) wname = lives_strdup_printf("#%s%s", widget_name, state_str);
#if !GTK_CHECK_VERSION(3, 24, 0)
    else wname = lives_strdup_printf("#%s%s %s", widget_name, state_str, selector);
#else
    else wname = lives_strdup_printf("#%s %s%s", widget_name, selector, state_str);
#endif

#ifdef GTK_TEXT_VIEW_CSS_BUG
  }
#endif

  lives_free(widget_name);
  css_string = g_strdup_printf(" %s {\n %s: %s;}\n", wname, detail, value);

#if !GTK_CHECK_VERSION(3, 24, 0)
  // special tweaks
  if (GTK_IS_FRAME(widget)) {
    tmp = lives_strdup_printf("%s %s label {\n %s: %s;}\n", css_string, wname, detail, value);
    lives_free(css_string);
    css_string = tmp;
  }

  if (GTK_IS_TEXT_VIEW(widget)) {
    tmp = lives_strdup_printf("%s %s text {\n %s: %s;}\n", css_string, wname, detail, value);
    lives_free(css_string);
    css_string = tmp;
  }

  if (GTK_IS_SPIN_BUTTON(widget)) {
    tmp = lives_strdup_printf("%s %s * {\n %s: %s;}\n", css_string, wname, detail, value);
    lives_free(css_string);
    css_string = tmp;
  }
#endif


  if (LIVES_SHOULD_EXPAND_WIDTH) {
    if (LIVES_IS_BUTTON(widget)) {
      tmp = lives_strdup_printf("%s %s {\n padding-left: %dpx; padding-right: %dpx;\n }\n", css_string, wname,
                                widget_opts.packing_width >> 1,
                                widget_opts.packing_width >> 1);
      lives_free(css_string);
      css_string = tmp;
    }
  }

  //if (selector != NULL) g_print("running CSS %s\n", css_string);

#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                  css_string,
                                  -1);
#else
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                  css_string,
                                  -1, NULL);
#endif
  lives_free(wname);
  lives_free(css_string);
  lives_widget_object_unref(provider);
  return TRUE;
}


boolean set_css_value(LiVESWidget *widget, LiVESWidgetState state, const char *detail, const char *value) {
  if (state == GTK_STATE_FLAG_NORMAL) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_NORMAL, NULL, detail, value);
  if (state & GTK_STATE_FLAG_ACTIVE) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_ACTIVE, NULL, detail, value);
  if (state & GTK_STATE_FLAG_PRELIGHT) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_PRELIGHT, NULL, detail, value);
  if (state & GTK_STATE_FLAG_SELECTED) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_SELECTED, NULL, detail, value);
  if (state & GTK_STATE_FLAG_INSENSITIVE) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_INSENSITIVE, NULL, detail, value);
  if (state & GTK_STATE_FLAG_INCONSISTENT) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_INCONSISTENT, NULL, detail, value);
  if (state & GTK_STATE_FLAG_FOCUSED) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_FOCUSED, NULL, detail, value);
  if (state & GTK_STATE_FLAG_BACKDROP) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_BACKDROP, NULL, detail, value);
  if (state & GTK_STATE_FLAG_CHECKED) set_css_value_for_state_flag(widget, GTK_STATE_FLAG_CHECKED, NULL, detail, value);
  return TRUE;
}
#endif
#endif


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
static boolean set_css_value_direct(LiVESWidget *widget, LiVESWidgetState state, const char *selector, const char *detail,
                                    const char *value) {
  return set_css_value_for_state_flag(widget, state, selector, detail, value);
}
#endif
#endif


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_bg_color(LiVESWidget *widget, LiVESWidgetState state,
    const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "background-color", colref);
  if (retb) retb = lives_widget_set_base_color(widget, state, color);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_background_color(widget, state, color);
#endif
#else
  gtk_widget_modify_bg(widget, state, color);
  gtk_widget_modify_base(widget, state, color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_bg_color(state, color);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_fg_color(LiVESWidget *widget, LiVESWidgetState state,
    const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "color", colref);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_color(widget, state, color);
#endif
#else
  gtk_widget_modify_text(widget, state, color);
  gtk_widget_modify_fg(widget, state, color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_fg_color(state, color);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_text_color(LiVESWidget *widget, LiVESWidgetState state,
    const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "color", colref);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_color(widget, state, color);
#endif
#else
  gtk_widget_modify_text(widget, state, color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_text_color(state, color);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_base_color(LiVESWidget *widget, LiVESWidgetState state,
    const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "background", colref);
  lives_free(colref);
  return retb;
#else
  gtk_widget_override_color(widget, state, color);
#endif
#else
  gtk_widget_modify_base(widget, state, color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_base_color(state, color);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_outline_color(LiVESWidget *widget, LiVESWidgetState state,
    const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "outline-color", colref);
  lives_free(colref);
  return retb;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_border_color(LiVESWidget *widget, LiVESWidgetState state,
    const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  char *colref = gdk_rgba_to_string(color);
  boolean retb = set_css_value(widget, state, "border-color", colref);
  lives_free(colref);
  return retb;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_font_size(LiVESWidget *widget, LiVESWidgetState state,
    const char *size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  boolean retb = set_css_value(widget, state, "font-size", size);
  return retb;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_fg_state_color(LiVESWidget *widget, LiVESWidgetState state,
    LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(4, 0, 0)
  gtk_style_context_get_color(gtk_widget_get_style_context(widget), color);
#else
  gtk_style_context_get_color(gtk_widget_get_style_context(widget), lives_widget_get_state(widget), color);
#endif
#else
  lives_widget_color_copy(color, &gtk_widget_get_style(widget)->fg[LIVES_WIDGET_STATE_NORMAL]);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  lives_widget_color_copy(color, widget->get_fg_color(state));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_bg_state_color(LiVESWidget *widget, LiVESWidgetState state,
    LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color(gtk_widget_get_style_context(widget), lives_widget_get_state(widget), color);
  G_GNUC_END_IGNORE_DEPRECATIONS
#else
  lives_widget_color_copy(color, &gtk_widget_get_style(widget)->bg[LIVES_WIDGET_STATE_NORMAL]);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  lives_widget_color_copy(color, widget->get_bg_color(state));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_color_equal(LiVESWidgetColor *c1, const LiVESWidgetColor *c2) {
#ifdef GUI_GTK
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (c1->alpha != c2->alpha) return FALSE;
#endif
  if (c1->red != c2->red || c1->green != c2->green || c1->blue != c2->blue) return FALSE;
  return TRUE;
#endif
  return FALSE;
}


boolean lives_widget_color_mix(LiVESWidgetColor *c1, const LiVESWidgetColor *c2, float mixval) {
  // c1 = mixval * c1 + (1. - mixval) * c2
  if (mixval < 0. || mixval > 1. || c1 == NULL || c2 == NULL) return FALSE;
#ifdef GUI_GTK
  c1->red = (float)c1->red * mixval + (float)c2->red * (1. - mixval);
  c1->green = (float)c1->green * mixval + (float)c2->green * (1. - mixval);
  c1->blue = (float)c1->blue * mixval + (float)c2->blue * (1. - mixval);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1, const LiVESWidgetColor *c2) {
  // if c1 is NULL, create a new copy of c2, otherwise copy c2 -> c1
  LiVESWidgetColor *c0 = NULL;
#ifdef GUI_GTK
  if (c1 != NULL) {
    c1->red = c2->red;
    c1->green = c2->green;
    c1->blue = c2->blue;
#if GTK_CHECK_VERSION(3, 0, 0)
    c1->alpha = c2->alpha;
#else
    c1->pixel = c2->pixel;
#endif
  } else {
#if GTK_CHECK_VERSION(3, 0, 0)
    c0 = gdk_rgba_copy(c2);
#else
    c0 = gdk_color_copy(c2);
#endif
  }
#endif

#ifdef GUI_QT
  if (c1 == NULL) {
    c1 = new LiVESWidgetColor;
  }
  c1->red = c2->red;
  c1->green = c2->green;
  c1->blue = c2->blue;
  c1->alpha = c2->alpha;

  c0 = c1;
#endif

  return c0;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_event_box_new(void) {
  LiVESWidget *eventbox = NULL;
#ifdef GUI_GTK
  eventbox = gtk_event_box_new();
#endif
#ifdef GUI_QT
  eventbox = new LiVESEventBox;
#endif
  return eventbox;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_event_box_set_above_child(LiVESEventBox *ebox, boolean set) {
#ifdef GUI_GTK
  gtk_event_box_set_above_child(ebox, set);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new(void) {
  LiVESWidget *image = NULL;
#ifdef GUI_GTK
  image = gtk_image_new();
#endif
#ifdef GUI_QT
  image = new LiVESImage;
#endif
  return image;
}


WIDGET_HELPER_LOCAL_INLINE int get_real_size_from_icon_size(LiVESIconSize size) {
  switch (size) {
  case LIVES_ICON_SIZE_SMALL_TOOLBAR:
  case LIVES_ICON_SIZE_BUTTON:
  case LIVES_ICON_SIZE_MENU:
    return 16;
  case LIVES_ICON_SIZE_LARGE_TOOLBAR:
    return 24;
  case LIVES_ICON_SIZE_DND:
    return 32;
  case LIVES_ICON_SIZE_DIALOG:
    return 48;
  default:
    break;
  }
  return -1;
}


LiVESPixbuf *lives_pixbuf_new_from_stock_at_size(const char *stock_id, LiVESIconSize size, int x, int y) {
  LiVESPixbuf *pixbuf = NULL;
  if (strncmp(stock_id, "lives-", 6)) {
    LiVESWidget *image = NULL;
    if (size == LIVES_ICON_SIZE_CUSTOM) {
      if (x == y) {
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_MENU)) size = LIVES_ICON_SIZE_MENU;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_SMALL_TOOLBAR)) size = LIVES_ICON_SIZE_SMALL_TOOLBAR;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_LARGE_TOOLBAR)) size = LIVES_ICON_SIZE_LARGE_TOOLBAR;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_BUTTON)) size = LIVES_ICON_SIZE_BUTTON;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_DND)) size = LIVES_ICON_SIZE_DND;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_DIALOG)) size = LIVES_ICON_SIZE_DIALOG;
      }
    }
    if (size != LIVES_ICON_SIZE_CUSTOM) {
      if (lives_has_icon(stock_id, size)) {
#if GTK_CHECK_VERSION(3, 10, 0)
        image = gtk_image_new_from_icon_name(stock_id, size);
#else
        image = gtk_image_new_from_stock(stock_id, size);
#endif
      }
      if (image != NULL) return lives_image_get_pixbuf(LIVES_IMAGE(image));
    }
    // custom size, or failed at specified size
    // try all sizes to see if we get one
    if (image == NULL) {
      if (lives_has_icon(stock_id, LIVES_ICON_SIZE_DIALOG)) {
        size = LIVES_ICON_SIZE_DIALOG;
      } else if (lives_has_icon(stock_id, LIVES_ICON_SIZE_DND)) {
        size = LIVES_ICON_SIZE_DND;
      } else if (lives_has_icon(stock_id, LIVES_ICON_SIZE_LARGE_TOOLBAR)) {
        size = LIVES_ICON_SIZE_LARGE_TOOLBAR;
      } else if (lives_has_icon(stock_id, LIVES_ICON_SIZE_SMALL_TOOLBAR)) {
        size = LIVES_ICON_SIZE_SMALL_TOOLBAR;
      } else if (lives_has_icon(stock_id, LIVES_ICON_SIZE_BUTTON)) {
        size = LIVES_ICON_SIZE_BUTTON;
      } else if (lives_has_icon(stock_id, LIVES_ICON_SIZE_MENU)) {
        size = LIVES_ICON_SIZE_MENU;
      } else return NULL;

#if GTK_CHECK_VERSION(3, 10, 0)
      image = gtk_image_new_from_icon_name(stock_id, size);
#else
      image = gtk_image_new_from_stock(stock_id, size);
#endif
    }
    if (image == NULL) return NULL;
    pixbuf = lives_image_get_pixbuf(LIVES_IMAGE(image));
  } else {
    char *fname = lives_strdup_printf("%s.%s", stock_id, LIVES_FILE_EXT_PNG);
    char *fnamex = lives_build_filename(prefs->prefix_dir, ICON_DIR, fname, NULL);
    LiVESError *error = NULL;
    pixbuf = lives_pixbuf_new_from_file(fnamex, &error);
    lives_free(fnamex);
    lives_free(fname);
  }
  if (pixbuf != NULL) {
    if (size != LIVES_ICON_SIZE_CUSTOM) {
      x = y = get_real_size_from_icon_size(size);
    }
    if (x > 0 && y > 0) {
      if (x != lives_pixbuf_get_width(pixbuf) || y != lives_pixbuf_get_height(pixbuf)) {
        LiVESPixbuf *new_pixbuf = lives_pixbuf_scale_simple(pixbuf, x, y, LIVES_INTERP_BEST);
        lives_widget_object_unref(pixbuf);
        pixbuf = new_pixbuf;
      }
    }
  }
  return pixbuf;
}


LiVESWidget *lives_image_new_from_stock_at_size(const char *stock_id, LiVESIconSize size, int x, int y) {
  LiVESWidget *image = NULL;
  LiVESPixbuf *pixbuf = NULL;
#ifdef GUI_GTK
  if (strncmp(stock_id, "lives-", 6)) {
    if (size == LIVES_ICON_SIZE_CUSTOM) {
      if (x == y) {
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_MENU)) size = LIVES_ICON_SIZE_MENU;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_SMALL_TOOLBAR)) size = LIVES_ICON_SIZE_SMALL_TOOLBAR;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_LARGE_TOOLBAR)) size = LIVES_ICON_SIZE_LARGE_TOOLBAR;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_BUTTON)) size = LIVES_ICON_SIZE_BUTTON;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_DND)) size = LIVES_ICON_SIZE_DND;
        if (x == get_real_size_from_icon_size(LIVES_ICON_SIZE_DIALOG)) size = LIVES_ICON_SIZE_DIALOG;
      }
    }
    if (size != LIVES_ICON_SIZE_CUSTOM) {
      if (lives_has_icon(stock_id, size)) {
#if GTK_CHECK_VERSION(3, 10, 0)
        image = gtk_image_new_from_icon_name(stock_id, size);
#else
        image = gtk_image_new_from_stock(stock_id, size);
#endif
      }
      if (image != NULL) return image;
    }
  }
  pixbuf = lives_pixbuf_new_from_stock_at_size(stock_id, size, x, y);
  if (pixbuf == NULL) return NULL;
  return lives_image_new_from_pixbuf(pixbuf);
#endif
  return image;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new_from_stock(const char *stock_id, LiVESIconSize size) {
  return lives_image_new_from_stock_at_size(stock_id, size, get_real_size_from_icon_size(size),
         get_real_size_from_icon_size(size));
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new_from_file(const char *filename) {
  LiVESWidget *image = NULL;
#ifdef GUI_GTK
  image = gtk_image_new_from_file(filename);
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(filename); // TODO ??
  QImage qm(qs);
  uint8_t *data = qm.bits();
  int width = qm.width();
  int height = qm.height();
  int bpl = qm.bytesPerLine();
  QImage::Format fmt = qm.format();

  image = new LiVESImage(data, width, height, bpl, fmt, imclean, data);
#endif
  return image;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf *pixbuf) {
  LiVESWidget *image = NULL;
#ifdef GUI_GTK
  image = gtk_image_new_from_pixbuf(pixbuf);
#endif
#ifdef GUI_QT
  image = new LiVESImage(static_cast<QImage *>(pixbuf));
#endif
  return image;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_set_from_pixbuf(LiVESImage *image, LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  gtk_image_set_from_pixbuf(image, pixbuf);
  return TRUE;
#endif
#ifdef GUI_QT
  *(static_cast<QImage *>(image)) = pixbuf->copy(0, 0, (static_cast<QImage *>(pixbuf))->width(),
                                    (static_cast<QImage *>(pixbuf))->height());
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_image_get_pixbuf(LiVESImage *image) {
  LiVESPixbuf *pixbuf = NULL;
#ifdef GUI_GTK
  pixbuf = gtk_image_get_pixbuf(image);
#endif
#ifdef GUI_QT
  pixbuf = new LiVESPixbuf(image);
#endif
  return pixbuf;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_parse(const char *spec, LiVESWidgetColor *color) {
  boolean retval = FALSE;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  retval = gdk_rgba_parse(color, spec);
#else
  retval = gdk_color_parse(spec, color);
#endif
#endif
#ifdef GUI_QT
  QColor qc = QColor(spec);
  if (qc.isValid()) {
    color->red = qc.redF();
    color->green = qc.greenF();
    color->blue = qc.blueF();
    color->alpha = qc.alphaF();
    retval = TRUE;
  }
#endif
  return retval;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2, 14, 0)
  return gtk_dialog_get_content_area(LIVES_DIALOG(dialog));
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif
#endif
#ifdef GUI_QT
  return dialog->get_content_area();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_dialog_get_action_area(LiVESDialog *dialog) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
#ifdef G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#endif
  return gtk_dialog_get_action_area(LIVES_DIALOG(dialog));
#ifdef G_GNUC_END_IGNORE_DEPRECATIONS
  G_GNUC_END_IGNORE_DEPRECATIONS
#endif
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif
#endif
#ifdef GUI_QT
  return dialog->get_action_area();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_left(LiVESWidget *widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 12, 0)
  gtk_widget_set_margin_start(widget, margin);
#else
  gtk_widget_set_margin_left(widget, margin);
#endif
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_right(LiVESWidget *widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#if GTK_CHECK_VERSION(3, 12, 0)
  gtk_widget_set_margin_end(widget, margin);
#else
  gtk_widget_set_margin_right(widget, margin);
#endif
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_top(LiVESWidget *widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_margin_top(widget, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_margin_bottom(LiVESWidget *widget, int margin) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_margin_bottom(widget, margin);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_add_action_widget(LiVESDialog *dialog, LiVESWidget *widget, int response) {
  // TODO: use lives_dialog_add_button, lives_dialog_add_button_from_stock
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_margin_left(widget, widget_opts.packing_width / 2);
  lives_widget_set_margin_right(widget, widget_opts.packing_width / 2);
#endif
  gtk_dialog_add_action_widget(dialog, widget, response);
  gtk_box_set_spacing(LIVES_BOX(lives_widget_get_parent(widget)), widget_opts.packing_width * 4);
  return TRUE;
#endif
#ifdef GUI_QT
  QDialogButtonBox *qbb = dynamic_cast<QDialogButtonBox *>(dialog->get_action_area());

  qbb->addButton(dynamic_cast<QPushButton *>(widget), static_cast<QDialogButtonBox::ButtonRole>(response));
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_window_new(LiVESWindowType wintype) {
  LiVESWidget *window = NULL;
#ifdef GUI_GTK
  window = gtk_window_new(wintype);
#endif

#ifdef GUI_QT
  if (wintype == LIVES_WINDOW_TOPLEVEL) {
    window = new LiVESMainWindow;
  } else {
    window = new LiVESDialog;
  }
#endif
  return window;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_title(LiVESWindow *window, const char *title) {
#ifdef GUI_GTK
  char *ntitle;
  if (strlen(widget_opts.title_prefix) > 0) {
    ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  } else ntitle = lives_strdup(title);
  gtk_window_set_title(window, ntitle);
  lives_free(ntitle);
  return TRUE;
#endif
#ifdef GUI_QT
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  QString qs = QString::fromUtf8(ntitle);
  window->setWindowTitle(qs);
  lives_free(ntitle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_transient_for(LiVESWindow *window, LiVESWindow *parent) {
#ifdef GUI_GTK
  gtk_window_set_transient_for(window, parent);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    parent->winId();
    QWindow *qpwindow = parent->windowHandle();
    qwindow->setTransientParent(qpwindow);
  }
  return TRUE;
#endif
  return FALSE;
}


static void modunmap(LiVESWindow *win, livespointer data) {if (win == modalw) modalw = NULL;}
static void moddest(LiVESWindow *win, livespointer data) {if (win == modalw) modalw = NULL;}
static boolean moddelete(LiVESWindow *win, LiVESXEvent *event, livespointer data) {
  if (win == modalw) modalw = NULL;
  return TRUE;
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_modal(LiVESWindow *window, boolean modal) {
  if (window == modalw) {
    lives_signal_sync_handler_disconnect_by_func(LIVES_GUI_OBJECT(modalw), moddest, NULL);
    lives_signal_sync_handler_disconnect_by_func(LIVES_GUI_OBJECT(modalw), moddelete, NULL);
    lives_signal_sync_handler_disconnect_by_func(LIVES_GUI_OBJECT(modalw), modunmap, NULL);
    modalw = NULL;
  }
  if (modal) {
    lives_signal_sync_connect(window, LIVES_WIDGET_DELETE_EVENT,
                              LIVES_GUI_CALLBACK(moddelete), NULL);
    lives_signal_sync_connect(window, LIVES_WIDGET_DESTROY_SIGNAL,
                              LIVES_GUI_CALLBACK(moddest), NULL);
    lives_signal_sync_connect(window, LIVES_WIDGET_UNMAP_SIGNAL,
                              LIVES_GUI_CALLBACK(modunmap), NULL);
    modalw = window;
  }
#ifdef GUI_GTK
  gtk_window_set_modal(window, modal);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_get_modal(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_get_modal(window);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_deletable(LiVESWindow *window, boolean deletable) {
#ifdef GUI_GTK
  gtk_window_set_deletable(window, deletable);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    //QDialog *qd = (QDialog *)window;
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags;
    if (!deletable) {
      newflags = flags;
      if (flags & Qt::WindowCloseButtonHint) newflags ^= Qt::WindowCloseButtonHint;
      newflags &= (flags | Qt::CustomizeWindowHint | Qt::WindowMinMaxButtonsHint);
    } else {
      newflags = flags | Qt::WindowCloseButtonHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_resizable(LiVESWindow *window, boolean resizable) {
#ifdef GUI_GTK
  gtk_window_set_resizable(window, resizable);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags = flags;
    if (!resizable) {
      if (flags & Qt::WindowMinMaxButtonsHint) newflags ^= Qt::WindowMinMaxButtonsHint;
      newflags |= Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint;
    } else {
      newflags = flags | Qt::WindowMinMaxButtonsHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_keep_below(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_keep_below(window, set);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags = flags;
    if (set) {
      newflags = flags | Qt::WindowStaysOnBottomHint;
    } else {
      if (flags & Qt::WindowStaysOnBottomHint) newflags ^= Qt::WindowStaysOnBottomHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_keep_above(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_keep_above(window, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_decorated(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_decorated(window, set);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags = flags;
    if (!set) {
      newflags = flags | Qt::FramelessWindowHint;
    } else {
      if (flags & Qt::FramelessWindowHint) newflags ^= Qt::FramelessWindowHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_auto_startup_notification(boolean set) {
#ifdef GUI_GTK
  gtk_window_set_auto_startup_notification(set);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
#endif

  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_screen(LiVESWindow *window, LiVESXScreen *screen) {
  if (LIVES_IS_WINDOW(window)) {
#ifdef GUI_GTK
    gtk_window_set_screen(window, screen);
    return TRUE;
#endif
#ifdef GUI_QT
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setScreen(screen);
    return TRUE;
#endif
  }
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_default_size(LiVESWindow *window, int width, int height) {
#ifdef GUI_GTK
  gtk_window_set_default_size(window, width, height);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->resize(width, height);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_window_get_title(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_get_title(window);
#endif
#ifdef GUI_QT
  return (const char *)window->windowTitle().toUtf8().constData();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_move(LiVESWindow *window, int x, int y) {
#ifdef GUI_GTK
  gtk_window_move(window, x, y);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qw = window->windowHandle();
    qw->setX(x);
    qw->setY(y);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_position(LiVESWidget *widget, int *x, int *y) {
#ifdef GUI_GTK
  GdkWindow *window = lives_widget_get_xwindow(widget);
  if (x != NULL) *x = 0;
  if (y != NULL) *y = 0;
  if (GDK_IS_WINDOW(window))
    gdk_window_get_position(window, x, y);
  return TRUE;
#endif
#ifdef GUI_QT
  QPoint p(0, 0);
  p = widget->mapToGlobal(p);
  *x = p.x();
  *y = p.y();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_get_position(LiVESWindow *window, int *x, int *y) {
#ifdef GUI_GTK
  gtk_window_get_position(window, x, y);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qw = window->windowHandle();
    *x = qw->x();
    *y = qw->y();
    return TRUE;
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_position(LiVESWindow *window, LiVESWindowPosition pos) {
#ifdef GUI_GTK
  gtk_window_set_position(window, pos);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window))
    (static_cast<LiVESMainWindow *>(window))->set_position(pos);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_set_hide_titlebar_when_maximized(LiVESWindow *window, boolean setting) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_window_set_hide_titlebar_when_maximized(window, setting);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_resize(LiVESWindow *window, int width, int height) {
#ifdef GUI_GTK
  gtk_window_resize(window, width, height);
  gtk_widget_set_size_request(GTK_WIDGET(window), width, height);
  return TRUE;
#endif
  // TODO
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_present(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_present(window);
  return TRUE;
#endif
#ifdef GUI_QT
  window->raise();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_fullscreen(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_fullscreen(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowFullScreen);
    qwindow->setFlags(Qt::Widget | Qt::Window | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_unfullscreen(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_unfullscreen(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowNoState);
    qwindow->setFlags(Qt::Widget | Qt::Window);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_maximize(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_maximize(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowMaximized);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_unmaximize(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_unmaximize(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowNoState);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_window_get_focus(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_get_focus(window);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAccelGroup *lives_accel_group_new(void) {
  LiVESAccelGroup *group = NULL;
#ifdef GUI_GTK
  group = gtk_accel_group_new();
#endif
#ifdef GUI_QT
  group = new LiVESAccelGroup;
#endif
  return group;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_group_connect(LiVESAccelGroup *group, uint32_t key, LiVESXModifierType mod,
    LiVESAccelFlags flags, LiVESWidgetClosure *closure) {
#ifdef GUI_GTK
  gtk_accel_group_connect(group, key, mod, flags, closure);
  return TRUE;
#endif
#ifdef GUI_QT
  group->connect(key, mod, flags, closure);
  return FALSE;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_group_disconnect(LiVESAccelGroup *group, LiVESWidgetClosure *closure) {
#ifdef GUI_GTK
  gtk_accel_group_disconnect(group, closure);
  return TRUE;
#endif
#ifdef GUI_QT
  group->disconnect(closure);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_add_accelerator(LiVESWidget *widget, const char *accel_signal,
    LiVESAccelGroup *accel_group,
    uint32_t accel_key, LiVESXModifierType accel_mods, LiVESAccelFlags accel_flags) {
#ifdef GUI_GTK
  gtk_widget_add_accelerator(widget, accel_signal, accel_group, accel_key, accel_mods, accel_flags);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->add_accel(accel_signal, accel_group, accel_key, accel_mods, accel_flags);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_add_accel_group(LiVESWindow *window, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_window_add_accel_group(window, group);
  return TRUE;
#endif
#ifdef GUI_QT
  window->add_accel_group(group);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_has_focus(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_has_focus(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_has_default(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_has_default(widget);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_remove_accel_group(LiVESWindow *window, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_window_remove_accel_group(window, group);
  return TRUE;
#endif
#ifdef GUI_QT
  window->remove_accel_group(group);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_set_accel_group(LiVESMenu *menu, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_menu_set_accel_group(menu, group);
  return TRUE;
#endif
#ifdef GUI_QT
  menu->add_accel_group(group);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_groups_activate(LiVESWidgetObject *object, uint32_t key,
    LiVESXModifierType mod) {
#ifdef GUI_GTK
  gtk_accel_groups_activate(object, key, mod);
  return TRUE;
#endif
#ifdef GUI_QT
  object->activate_accel(key, mod);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height) {
#ifdef GUI_GTK
  // alpha fmt is RGBA post mult
  return gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha, 8, width, height);
#endif

#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  QImage::Format fmt;
  if (!has_alpha) fmt = QImage::Format_RGB888;
  else {
    fmt = QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  // on destruct, we need to call lives_free_buffer_fn(uchar *pixels, livespointer destroy_fn_data)
  return new LiVESPixbuf(width, height, fmt);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_new_from_data(const unsigned char *buf, boolean has_alpha, int width,
    int height,
    int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn,
    livespointer destroy_fn_data) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_data((const guchar *)buf, GDK_COLORSPACE_RGB, has_alpha, 8, width, height, rowstride,
                                  lives_free_buffer_fn,
                                  destroy_fn_data);
#endif

#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  QImage::Format fmt;
  if (!has_alpha) fmt = QImage::Format_RGB888;
  else {
    fmt = QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  return new LiVESPixbuf((uchar *)buf, width, height, rowstride, fmt, imclean, (void *)buf);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file(filename, error);
#endif

#ifdef GUI_QT
  LiVESPixbuf *image = new LiVESPixbuf;
  QString qs = QString::fromUtf8(filename); // TODO ??
  if (!(static_cast<QImage *>(image))->load(qs)) {
    // do something with error
    LIVES_WARN("QImage not loaded");
    delete image;
    return NULL;
  }
  return image;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height,
    boolean preserve_aspect_ratio,
    LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file_at_scale(filename, width, height, preserve_aspect_ratio, error);
#endif

#ifdef GUI_QT
  QImage image;
  QImage image2;
  Qt::AspectRatioMode asp;
#ifdef IS_MINGW
  QString qs = QString::fromUtf8(filename);
#else
  QString qs = QString::fromLocal8Bit(filename);
#endif
  QImageReader qir;
  qir.setFileName(qs);
  if (!preserve_aspect_ratio) qir.setScaledSize(QSize(width, height));
  if (!qir.read(&image)) {
    if (error != NULL) {
      *error = (LiVESError *)malloc(sizeof(LiVESError));
      (*error)->code = qir.error();
      (*error)->message = strdup(qir.errorString().toUtf8().constData());
    }
    return NULL;
  }
  if (preserve_aspect_ratio) asp = Qt::KeepAspectRatio;
  else asp = Qt::IgnoreAspectRatio;
  image2 = image.scaled(width, height, asp,  Qt::SmoothTransformation);
  if (image2.isNull()) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return new LiVESPixbuf(&image2);
#endif

  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_rowstride(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_rowstride(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->bytesPerLine();
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_width(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_width(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->width();
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_height(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_height(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->height();
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_pixbuf_get_n_channels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_n_channels(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->depth() >> 3;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return (uchar *)(dynamic_cast<const QImage *>(pixbuf))->bits();
#endif
}


WIDGET_HELPER_GLOBAL_INLINE const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return (const guchar *)gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return (const uchar *)(dynamic_cast<const QImage *>(pixbuf))->bits();
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_has_alpha(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->hasAlphaChannel();
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height,
    LiVESInterpType interp_type) {
#ifdef GUI_GTK
  return gdk_pixbuf_scale_simple(src, dest_width, dest_height, interp_type);
#endif

#ifdef GUI_QT
  QImage image = (dynamic_cast<const QImage *>(src))->scaled(dest_width, dest_height, Qt::IgnoreAspectRatio, interp_type);
  if (image.isNull()) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return new LiVESPixbuf(&image);

#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf *src, LiVESPixbuf *dest,
    float saturation,
    boolean pixilate) {
#ifdef GUI_GTK
  gdk_pixbuf_saturate_and_pixelate(src, dest, saturation, pixilate);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper,
    double step_increment, double page_increment, double page_size) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  adj = gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size);
#else
  adj = GTK_ADJUSTMENT(gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size));
#endif
#endif
#ifdef GUI_QT
  adj = new LiVESAdjustment(value, lower, upper, step_increment, page_increment, page_size);
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_set_homogeneous(LiVESBox *box, boolean homogenous) {
#ifdef GUI_GTK
  gtk_box_set_homogeneous(box, homogenous);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_reorder_child(LiVESBox *box, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_box_reorder_child(box, child, pos);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    qbox->removeWidget(child);
    qbox->insertWidget(pos, child);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    qbox->removeWidget(child);
    qbox->insertWidget(pos, child);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_set_spacing(LiVESBox *box, int spacing) {
#ifdef GUI_GTK
  gtk_box_set_spacing(box, spacing);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    qbox->setSpacing(spacing);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    qbox->setSpacing(spacing);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hbox_new(boolean homogeneous, int spacing) {
  LiVESWidget *hbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hbox = gtk_box_new(LIVES_ORIENTATION_HORIZONTAL, spacing);
  lives_box_set_homogeneous(LIVES_BOX(hbox), homogeneous);
#else
  hbox = gtk_hbox_new(homogeneous, spacing);
#endif
#endif
#ifdef GUI_QT
  LiVESHBox *hxbox = new LiVESHBox;
  hxbox->setSpacing(spacing);
  hbox = static_cast<LiVESWidget *>(hxbox);
#endif
  return hbox;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vbox_new(boolean homogeneous, int spacing) {
  LiVESWidget *vbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vbox = gtk_box_new(LIVES_ORIENTATION_VERTICAL, spacing);
  lives_box_set_homogeneous(LIVES_BOX(vbox), homogeneous);
#else
  vbox = gtk_vbox_new(homogeneous, spacing);
#endif
#endif
#ifdef GUI_QT
  LiVESVBox *vxbox = new LiVESVBox;
  vxbox->setSpacing(spacing);
  vbox = static_cast<LiVESWidget *>(vxbox);
#endif
  return vbox;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_pack_start(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill,
    uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_start(box, child, expand, fill, padding);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_pack_end(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill,
    uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_end(box, child, expand, fill, padding);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    if (fill) child->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    qbox->insertWidget(0, child, expand ? 100 : 0, fill ? (Qt::Alignment)0 : Qt::AlignHCenter);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    if (fill) child->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    qbox->insertWidget(0, child, expand ? 100 : 0, fill ? (Qt::Alignment)0 : Qt::AlignVCenter);
  }
  box->add_child(child);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hseparator_new(void) {
  LiVESWidget *hsep = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hsep = gtk_separator_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  hsep = gtk_hseparator_new();
#endif
#endif
#ifdef GUI_QT
  hsep = new LiVESHSeparator;
#endif
  return hsep;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vseparator_new(void) {
  LiVESWidget *vsep = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vsep = gtk_separator_new(LIVES_ORIENTATION_VERTICAL);
#else
  vsep = gtk_vseparator_new();
#endif
#endif
#ifdef GUI_QT
  vsep = new LiVESVSeparator;
#endif
  return vsep;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hbutton_box_new(void) {
  LiVESWidget *bbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  bbox = gtk_button_box_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  bbox = gtk_hbutton_box_new();
#endif
#ifdef GUI_QT
  bbox = new LiVESButtonBox(LIVES_ORIENTATION_HORIZONTAL);
#endif
#endif
  return bbox;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vbutton_box_new(void) {
  LiVESWidget *bbox = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  bbox = gtk_button_box_new(LIVES_ORIENTATION_VERTICAL);
#else
  bbox = gtk_vbutton_box_new();
#endif
#ifdef GUI_QT
  bbox = new LiVESButtonBox(LIVES_ORIENTATION_VERTICAL);
#endif
#endif
  return bbox;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_box_set_layout(LiVESButtonBox *bbox, LiVESButtonBoxStyle bstyle) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  return FALSE;
#endif
  gtk_button_box_set_layout(bbox, bstyle);
  return TRUE;
#ifdef GUI_QT
  if (bstyle == LIVES_BUTTONBOX_CENTER) {
    bbox->setCenterButtons(true);
  } else {
    bbox->setCenterButtons(false);
  }
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vscale_new(LiVESAdjustment *adj) {
  LiVESWidget *vscale = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vscale = gtk_scale_new(LIVES_ORIENTATION_VERTICAL, adj);
#else
  vscale = gtk_vscale_new(adj);
#endif
#endif
#ifdef GUI_QT
  vscale = new LiVESScale(LIVES_ORIENTATION_VERTICAL, adj);
#endif
  return vscale;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hpaned_new(void) {
  LiVESWidget *hpaned = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hpaned = gtk_paned_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  hpaned = gtk_hpaned_new();
#endif
#endif
#ifdef GUI_QT
  LiVESPaned *qs = new LiVESPaned;
  qs->setOrientation(LIVES_ORIENTATION_HORIZONTAL);
  hpaned = static_cast<LiVESWidget *>(qs);
#endif
  return hpaned;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vpaned_new(void) {
  LiVESWidget *vpaned = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vpaned = gtk_paned_new(LIVES_ORIENTATION_VERTICAL);
#else
  vpaned = gtk_vpaned_new();
#endif
#endif
#ifdef GUI_QT
  LiVESPaned *qs = new LiVESPaned;
  qs->setOrientation(LIVES_ORIENTATION_VERTICAL);
  vpaned = static_cast<LiVESWidget *>(qs);
#endif
  return vpaned;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_hscrollbar_new(LiVESAdjustment *adj) {
  LiVESWidget *hscrollbar = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hscrollbar = gtk_scrollbar_new(LIVES_ORIENTATION_HORIZONTAL, adj);
#else
  hscrollbar = gtk_hscrollbar_new(adj);
#endif
#endif
#ifdef GUI_QT
  hscrollbar = new LiVESScrollbar(LIVES_ORIENTATION_HORIZONTAL, adj);
#endif
  return hscrollbar;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_vscrollbar_new(LiVESAdjustment *adj) {
  LiVESWidget *vscrollbar = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  vscrollbar = gtk_scrollbar_new(LIVES_ORIENTATION_VERTICAL, adj);
#else
  vscrollbar = gtk_vscrollbar_new(adj);
#endif
#endif
#ifdef GUI_QT
  vscrollbar = new LiVESScrollbar(LIVES_ORIENTATION_VERTICAL, adj);
#endif
  return vscrollbar;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_label_new(const char *text) {
  LiVESWidget *label = NULL;
#ifdef GUI_GTK
  label = gtk_label_new(text);
  gtk_label_set_use_underline(LIVES_LABEL(label), widget_opts.mnemonic_label);
  gtk_label_set_justify(LIVES_LABEL(label), widget_opts.justify);
  gtk_label_set_line_wrap(LIVES_LABEL(label), widget_opts.line_wrap);
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(text);
  LiVESLabel *ql = new LiVESLabel(qs);
  ql->setAlignment(widget_opts.justify);
  ql->setWordWrap(widget_opts.line_wrap);
  ql->setTextFormat(Qt::PlainText);
  label = static_cast<LiVESWidget *>(ql);
#endif
  return label;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_arrow_new(LiVESArrowType arrow_type, LiVESShadowType shadow_type) {
  LiVESWidget *arrow = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 12, 0)
  const char *format = "<b>%s</b>";
  char *markup;
  char *str;

  switch (arrow_type) {
  case LIVES_ARROW_DOWN:
    str = "v";
    break;
  case LIVES_ARROW_LEFT:
    str = "<";
    break;
  case LIVES_ARROW_RIGHT:
    str = ">";
    break;
  default:
    return arrow;
  }

  arrow = gtk_label_new("");
  markup = g_markup_printf_escaped(format, str);
  gtk_label_set_markup(GTK_LABEL(arrow), markup);
  lives_free(markup);

#else
  arrow = gtk_arrow_new(arrow_type, shadow_type);
#endif
#endif
#ifdef GUI_QT
  QStyleOption *qstyleopt = 0;
  QIcon qi = QApplication::style()->standardIcon(arrow_type, qstyleopt);
  QPixmap qp = qi.pixmap(LIVES_ICON_SIZE_DIALOG);
  QImage qm = qp.toImage();
  arrow = new LiVESArrow(&qm);
#endif
  return arrow;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_halign(LiVESWidget *widget, LiVESAlign align) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (LIVES_IS_LABEL(widget)) {
    if (align == LIVES_ALIGN_START) gtk_label_set_xalign(LIVES_LABEL(widget), 0.);
    if (align == LIVES_ALIGN_CENTER) gtk_label_set_xalign(LIVES_LABEL(widget), 0.5);
    if (align == LIVES_ALIGN_END) gtk_label_set_xalign(LIVES_LABEL(widget), 1.);
  } else gtk_widget_set_halign(widget, align);
#else
  if (LIVES_IS_LABEL(widget)) {
    float xalign, yalign;
    gtk_misc_get_alignment(GTK_MISC(widget), &xalign, &yalign);
    switch (align) {
    case LIVES_ALIGN_START:
      gtk_misc_set_alignment(GTK_MISC(widget), 0., yalign);
      break;
    case LIVES_ALIGN_END:
      gtk_misc_set_alignment(GTK_MISC(widget), 1., yalign);
      break;
    case LIVES_ALIGN_CENTER:
      gtk_misc_set_alignment(GTK_MISC(widget), 0.5, yalign);
      break;
    default:
      return FALSE;
    }
    return TRUE;
  }
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_valign(LiVESWidget *widget, LiVESAlign align) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_valign(widget, align);
#else
  if (!LIVES_IS_LABEL(widget)) return FALSE;
  else {
    float xalign, yalign;
    gtk_misc_get_alignment(GTK_MISC(widget), &xalign, &yalign);
    switch (align) {
    case LIVES_ALIGN_START:
      gtk_misc_set_alignment(GTK_MISC(widget), xalign, 0.);
      break;
    case LIVES_ALIGN_END:
      gtk_misc_set_alignment(GTK_MISC(widget), xalign, 1.);
      break;
    case LIVES_ALIGN_CENTER:
      gtk_misc_set_alignment(GTK_MISC(widget), xalign, 0.5);
      break;
    default:
      return FALSE;
    }
  }
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale) {
  LiVESWidget *alignment = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  alignment = gtk_aspect_frame_new(NULL, xalign, yalign, xscale / yscale, TRUE);
  lives_frame_set_shadow_type(LIVES_FRAME(alignment), LIVES_SHADOW_NONE);
#else
  alignment = gtk_alignment_new(xalign, yalign, xscale, yscale);
#endif
#endif
#ifdef GUI_QT
  alignment = new LiVESAlignment(xalign, yalign, xscale, yscale);
#endif
  return alignment;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_alignment_set(LiVESWidget *alignment, float xalign, float yalign, float xscale,
    float yscale) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_aspect_frame_set(GTK_ASPECT_FRAME(alignment), xalign, yalign, xscale / yscale, TRUE);
#else
  gtk_alignment_set(LIVES_ALIGNMENT(alignment), xalign, yalign, xscale, yscale);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  alignment->set_alignment(xalign, yalign, xscale, yscale);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_expander_new(const char *label) {
  LiVESWidget *expander = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) expander = gtk_expander_new(label);
  else expander = gtk_expander_new_with_mnemonic(label);
#if GTK_CHECK_VERSION(3, 2, 0)
  gtk_expander_set_resize_toplevel(GTK_EXPANDER(expander), TRUE);
#endif
#endif
  return expander;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_expander_get_label_widget(LiVESExpander *expander) {
  LiVESWidget *widget = NULL;
#ifdef GUI_GTK
  widget = gtk_expander_get_label_widget(expander);
#endif
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_expander_set_use_markup(LiVESExpander *expander, boolean val) {
#ifdef GUI_GTK
  gtk_expander_set_use_markup(expander, val);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_width_chars(LiVESLabel *label, int nchars) {
#ifdef GUI_GTK
  gtk_label_set_max_width_chars(label, nchars);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_halignment(LiVESLabel *label, float xalign) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 16, 0)
  gtk_label_set_xalign(label, xalign);
#else
  if (xalign == 0.)
    lives_widget_set_halign(LIVES_WIDGET(label), LIVES_ALIGN_START);
  else if (xalign == 1.)
    lives_widget_set_halign(LIVES_WIDGET(label), LIVES_ALIGN_END);
  else
    lives_widget_set_halign(LIVES_WIDGET(label), LIVES_ALIGN_CENTER);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QRect qr = (static_cast<QFrame *>(label))->contentsRect();
  int pixels = (float)qr.width() * xalign;
  label->setIndent(pixels);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_combo_new(void) {
  LiVESWidget *combo = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  combo = gtk_combo_box_text_new_with_entry();
#else
  combo = gtk_combo_box_entry_new_text();
#endif
#endif
#ifdef GUI_QT
  combo = new LiVESCombo;
#endif
  return combo;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_combo_new_with_model(LiVESTreeModel *model) {
  LiVESWidget *combo = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  combo = gtk_combo_box_new_with_model_and_entry(model);
#else
  combo = gtk_combo_box_entry_new();
  gtk_combo_box_set_model(GTK_COMBO_BOX(combo), model);
#endif
#endif
#ifdef GUI_QT
  combo = new LiVESCombo;
  QComboBox *qcombo = dynamic_cast<QComboBox *>(combo);
  qcombo->setModel(model->to_qsimodel());
  if (model->get_qtree_widget() != NULL) qcombo->setView(model->get_qtree_widget());
#endif
  return combo;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeModel *lives_combo_get_model(LiVESCombo *combo) {
  LiVESTreeModel *model = NULL;
#ifdef GUI_GTK
  model = gtk_combo_box_get_model(combo);
#endif
#ifdef GUI_QT
  QAbstractItemModel *qqmodel = (static_cast<QComboBox *>(combo))->model();

  QVariant qv = (qqmodel)->property("LiVESWidgetObject");

  if (qv.isValid()) {
    model = static_cast<LiVESTreeModel *>(qv.value<LiVESWidgetObject *>());
  }

#endif
  return model;
}


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)

#define COMBO_CELL_HEIGHT 24

static boolean setcellbg(LiVESCellRenderer *r, void *p) {
  // set the pop-up colours
  //lives_widget_object_set(r, "border-width", 0);
  lives_widget_object_set(r, "cell-background-rgba", &palette->info_base);
  lives_widget_object_set(r, "foreground-rgba", &palette->info_text);
  gtk_cell_renderer_set_fixed_size(r, -1, COMBO_CELL_HEIGHT);
  return FALSE;
}
#endif
#endif


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_model(LiVESCombo *combo, LiVESTreeModel *model) {
#ifdef GUI_GTK
  gtk_combo_box_set_model(combo, model);
  return TRUE;
#endif
  return FALSE;
}


void lives_combo_popup(LiVESCombo *combo) {
  // used in callback, so no inline
#ifdef GUI_GTK
  gtk_combo_box_popup(combo);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_focus_on_click(LiVESCombo *combo, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 20, 0)
  gtk_widget_set_focus_on_click(GTK_WIDGET(combo), state);
#else
  gtk_combo_box_set_focus_on_click(combo, state);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_append_text(LiVESCombo *combo, const char *text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);
#else
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo), text);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(text);
  (static_cast<QComboBox *>(combo))->addItem(qs);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_prepend_text(LiVESCombo *combo, const char *text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(combo), text);
#else
  gtk_combo_box_prepend_text(GTK_COMBO_BOX(combo), text);
#endif
  return TRUE;
#endif
  return FALSE;
}


boolean lives_combo_remove_all_text(LiVESCombo *combo) {
  /* // for tstore, need lives_tree_store_find_iter(), gtk_tree_model_iter_has_child () */
  /* // or maybe just free the treestore and add a new list store */
  /* //LiVESTreeStore *tstore = lives_tree_store_new(1, LIVES_COL_TYPE_STRING); */
  /* LiVESListStore *lstore = lives_list_store_new(1, LIVES_COL_TYPE_STRING); */
  /* //lives_combo_set_model(combo, NULL); */
  /* GtkCellArea *celly; */
  /* lives_widget_object_get(LIVES_WIDGET_OBJECT(combo), "cell-area", &celly); */
  /* gtk_cell_layout_clear(celly); */
  /* lives_combo_set_model(LIVES_COMBO(combo), LIVES_TREE_MODEL(lstore)); */
  /* return TRUE; */
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  // TODO *** - only works with list model
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
#else
  register int count = lives_tree_model_iter_n_children(lives_combo_get_model(combo), NULL);
  while (count-- > 0) gtk_combo_box_remove_text(combo, 0);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  static_cast<QComboBox *>(combo)->clear();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_entry_text_column(LiVESCombo *combo, int column) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo), column);
#else
  gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(combo), column);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QComboBox *>(combo))->setModelColumn(column);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_combo_get_active_text(LiVESCombo *combo) {
  // return value should be freed
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 24, 0)
  return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
#else
  return gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
#endif
#endif
#ifdef GUI_QT
  return strdup((static_cast<QComboBox *>(combo))->currentText().toUtf8().constData());
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_active_index(LiVESCombo *combo, int index) {
#ifdef GUI_GTK
  gtk_combo_box_set_active(combo, index);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QComboBox *>(combo))->setCurrentIndex(index);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_active_iter(LiVESCombo *combo, LiVESTreeIter *iter) {
#ifdef GUI_GTK
  gtk_combo_box_set_active_iter(combo, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESTreeModel *model = lives_combo_get_model(combo);
  if (model != NULL) {
    QTreeWidget *widget = model->get_qtree_widget();
    if (widget != NULL) widget->setCurrentItem(iter);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_get_active_iter(LiVESCombo *combo, LiVESTreeIter *iter) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active_iter(combo, iter);
#endif
#ifdef GUI_QT
  LiVESTreeModel *model = lives_combo_get_model(combo);
  if (model != NULL) {
    QTreeWidget *widget = model->get_qtree_widget();
    if (widget != NULL) {
      QTreeWidgetItem *qtwi = widget->currentItem();
      *iter = *qtwi;
      return TRUE;
    }
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_combo_get_active(LiVESCombo *combo) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active(combo);
#endif
#ifdef GUI_QT
  return (static_cast<QComboBox *>(combo))->currentIndex();
#endif
  return -1;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_text_view_new(void) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_text_view_new();
#endif
#ifdef GUI_QT
  tview = new LiVESTextView;
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_text_view_new_with_buffer(LiVESTextBuffer *tbuff) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_text_view_new_with_buffer(tbuff);
#endif
#ifdef GUI_QT
  tview = new LiVESTextView(tbuff);
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTextBuffer *lives_text_view_get_buffer(LiVESTextView *tview) {
  LiVESTextBuffer *tbuff = NULL;
#ifdef GUI_GTK
  tbuff = gtk_text_view_get_buffer(tview);
#endif
#ifdef GUI_QT
  tbuff = tview->get_buffer();
#endif
  return tbuff;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_editable(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_editable(tview, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setReadOnly(!setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_accepts_tab(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_accepts_tab(tview, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setTabChangesFocus(!setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_cursor_visible(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_cursor_visible(tview, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  if (setting) tview->setCursorWidth(1);
  else tview->setCursorWidth(0);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_wrap_mode(LiVESTextView *tview, LiVESWrapMode wrapmode) {
#ifdef GUI_GTK
  gtk_text_view_set_wrap_mode(tview, wrapmode);
  return TRUE;
#endif
#ifdef GUI_QT
  if (wrapmode == LIVES_WRAP_NONE) tview->setLineWrapMode(QTextEdit::NoWrap);
  if (wrapmode == LIVES_WRAP_WORD) tview->setLineWrapMode(QTextEdit::WidgetWidth);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_view_set_justification(LiVESTextView *tview, LiVESJustification justify) {
#ifdef GUI_GTK
  gtk_text_view_set_justification(tview, justify);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setAlignment(justify);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTextBuffer *lives_text_buffer_new(void) {
  LiVESTextBuffer *tbuff = NULL;
#ifdef GUI_GTK
  tbuff = gtk_text_buffer_new(NULL);
#endif
#ifdef GUI_QT
  tbuff = new LiVESTextBuffer;
#endif
  return tbuff;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert(LiVESTextBuffer *tbuff, LiVESTextIter *iter, const char *text,
    int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert(tbuff, iter, text, len);
  return TRUE;
#endif
#ifdef GUI_QT
  QTextCursor qtc = QTextCursor(tbuff);
  qtc.setPosition(*iter);
  QString qs = QString::fromUtf8(text);
  qtc.insertText(qs);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer *tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert_at_cursor(tbuff, text, len);
  return TRUE;
#endif
#ifdef GUI_QT
  QTextCursor qtc = tbuff->get_cursor();
  QString qs = QString::fromUtf8(text, len);
  qtc.insertText(qs);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_set_text(LiVESTextBuffer *tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_set_text(tbuff, text, len);
  return TRUE;
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(text, len);
  tbuff->setPlainText(qs);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_text_buffer_get_text(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end,
    boolean inc_hidden_chars) {
#ifdef GUI_GTK
  return gtk_text_buffer_get_text(tbuff, start, end, inc_hidden_chars);
#endif
#ifdef GUI_QT
  QTextCursor qtc = QTextCursor(tbuff);
  qtc.setPosition(*start);
  qtc.setPosition(*end, QTextCursor::KeepAnchor);
  return strdup(qtc.selection().toPlainText().toUtf8().constData());
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_get_start_iter(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_start_iter(tbuff, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  *iter = 0;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_get_end_iter(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_end_iter(tbuff, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  *iter = tbuff->characterCount();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_place_cursor(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_place_cursor(tbuff, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  tbuff->get_cursor().setPosition(*iter);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTextMark *lives_text_buffer_create_mark(LiVESTextBuffer *tbuff, const char *mark_name,
    const LiVESTextIter *where, boolean left_gravity) {
  LiVESTextMark *tmark;
#ifdef GUI_GTK
  tmark = gtk_text_buffer_create_mark(tbuff, mark_name, where, left_gravity);
#endif
#ifdef GUI_QT
  tmark = new LiVESTextMark(tbuff, mark_name, *where, left_gravity);
#endif
  return tmark;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_delete_mark(LiVESTextBuffer *tbuff, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_buffer_delete_mark(tbuff, mark);
  return TRUE;
#endif
#ifdef GUI_QT
  mark->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_delete(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end) {
#ifdef GUI_GTK
  gtk_text_buffer_delete(tbuff, start, end);
  return TRUE;
#endif
#ifdef GUI_QT
  QTextCursor qtc = QTextCursor(tbuff);
  qtc.setPosition(*start);
  qtc.setPosition(*end, QTextCursor::KeepAnchor);
  qtc.removeSelectedText();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_text_buffer_get_iter_at_mark(LiVESTextBuffer *tbuff, LiVESTextIter *iter,
    LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_buffer_get_iter_at_mark(tbuff, iter, mark);
  return TRUE;
#endif
#ifdef GUI_QT
  *iter = mark->position();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_dialog_new(void) {
  LiVESWidget *dialog = NULL;
#ifdef GUI_GTK
  dialog = gtk_dialog_new();
#endif
#ifdef GUI_QT
  dialog = new LiVESDialog();
#endif
  return dialog;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_button_new(void) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_button_new();
  gtk_button_set_use_underline(GTK_BUTTON(button), widget_opts.mnemonic_label);
#endif
#ifdef GUI_QT
  button = new LiVESButton;
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_button_new_with_label(const char *label) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = lives_button_new();
  lives_button_set_label(LIVES_BUTTON(button), label);
#endif
  return button;
}


LiVESWidget *lives_button_new_from_stock(const char *stock_id, const char *label) {
  LiVESWidget *button = NULL;

#if GTK_CHECK_VERSION(3, 10, 0) || defined GUI_QT
  do {
    if (stock_id == NULL) {
      button = lives_button_new();
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_LABEL_CANCEL)) stock_id = LIVES_STOCK_CANCEL;
    if (!strcmp(stock_id, LIVES_STOCK_LABEL_OK)) stock_id = LIVES_STOCK_OK;
    // gtk 3.10 + -> we need to set the text ourselves
    if (!strcmp(stock_id, LIVES_STOCK_NO)) stock_id = "gtk-no";
    if (!strcmp(stock_id, LIVES_STOCK_APPLY)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_APPLY);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_OK)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_OK);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_CANCEL)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_CANCEL);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_YES)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_YES);
      break;
    }
    if (!strcmp(stock_id, "gtk-no")) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_NO);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_CLOSE)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_CLOSE);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_REVERT_TO_SAVED)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_REVERT);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_REFRESH)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_REFRESH);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_DELETE)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_DELETE);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_SAVE)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_SAVE);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_SAVE_AS)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_SAVE_AS);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_OPEN)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_OPEN);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_SELECT_ALL)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_SELECT_ALL);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_QUIT)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_QUIT);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_GO_FORWARD)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_GO_FORWARD);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_FORWARD)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_FORWARD);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_REWIND)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_REWIND);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_STOP)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_STOP);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_PLAY)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_PLAY);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_PAUSE)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_PAUSE);
      break;
    }
    if (!strcmp(stock_id, LIVES_STOCK_MEDIA_RECORD)) {
      button = lives_button_new_with_label(LIVES_STOCK_LABEL_MEDIA_RECORD);
      break;
    }
    // text not known
    button = lives_button_new();
  } while (FALSE);
#ifdef GUI_GTK
  if (stock_id != NULL && (widget_opts.show_button_images
                           || !strcmp(stock_id, LIVES_STOCK_ADD)
                           || !strcmp(stock_id, LIVES_STOCK_REMOVE)
                          )) {
    LiVESWidget *image = gtk_image_new_from_icon_name(stock_id, GTK_ICON_SIZE_BUTTON);
    if (LIVES_IS_IMAGE(image))
      gtk_button_set_image(GTK_BUTTON(button), image);
#endif

#ifdef GUI_QT
    if (!QIcon::hasThemeIcon(stock_id)) lives_printerr("Missing icon %s\n", stock_id);
    else {
      QAbstractButton *qbutton = dynamic_cast<QAbstractButton *>(button);
      if (qbutton != NULL) {
        qbutton->setIconSize(LIVES_ICON_SIZE_BUTTON);
        QIcon qi = QIcon::fromTheme(stock_id);
        QPixmap qp = qi.pixmap(qbutton->iconSize());
        qbutton->setIcon(qp);
      }
#endif
    }

#else
  // < 3.10
  button = gtk_button_new_from_stock(stock_id);
#endif

    if (!LIVES_IS_BUTTON(button)) {
      LIVES_WARN("Unable to find button with stock_id:");
      LIVES_WARN(stock_id);
      button = lives_button_new();
    }

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 6, 0)
    gtk_button_set_always_show_image(GTK_BUTTON(button), widget_opts.show_button_images);
#endif
    if (label != NULL)
      lives_button_set_label(LIVES_BUTTON(button), label);
#endif
    lives_widget_set_can_focus_and_default(button);
    return button;
  }

#ifdef BALANCE
} // to fix indentation
#endif


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_label(LiVESButton *button, const char *label) {
  char *labeltext = lives_strdup_printf("%s%s%s%s%s", LIVES_SHOULD_EXPAND_HEIGHT ? "\n" : "",
                                        LIVES_SHOULD_EXPAND_WIDTH ? "    " : "",
                                        label, LIVES_SHOULD_EXPAND_WIDTH ? "    " : "", LIVES_SHOULD_EXPAND_HEIGHT ? "\n" : "");

#ifdef GUI_GTK
  gtk_button_set_label(button, labeltext);
  gtk_button_set_use_underline(button, widget_opts.mnemonic_label);
  lives_free(labeltext);
#if !GTK_CHECK_VERSION(3, 0, 0)
  if (is_standard_widget(button))
    default_changed_cb(LIVES_WIDGET_OBJECT(button), NULL, NULL);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QString qlabel = QString::fromUtf8(label);
  if (button->get_use_underline()) {
    qlabel = qlabel.replace('&', "&&");
    qlabel = qlabel.replace('_', '&');
  }
  (dynamic_cast<QAbstractButton *>(button))->setText(qlabel);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_button_get_label(LiVESButton *button) {
#ifdef GUI_GTK
  return gtk_button_get_label(button);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_clicked(LiVESButton *button) {
#ifdef GUI_GTK
  gtk_button_clicked(button);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_relief(LiVESButton *button, LiVESReliefStyle rstyle) {
#ifdef GUI_GTK
  gtk_button_set_relief(button, rstyle);
  return TRUE;
#endif
#ifdef GUI_QT
  if (rstyle == LIVES_RELIEF_NONE) button->setFlat(true);
  else button->setFlat(false);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_image(LiVESButton *button, LiVESWidget *image) {
#ifdef GUI_GTK
  gtk_button_set_image(button, image);
  return TRUE;
#endif
#ifdef GUI_QT
  QAbstractButton *qbutton = dynamic_cast<QAbstractButton *>(button);
  if (image != NULL && image->get_type() == LIVES_WIDGET_TYPE_IMAGE) {
    QImage *qim = dynamic_cast<QImage *>(image);
    if (qim != NULL) {
      QPixmap qpx;
      qpx.convertFromImage(*qim);
      QIcon *qi = new QIcon(qpx);
      qbutton->setIcon(*qi);
    }
  } else {
    //qbutton->setIcon(NULL);
  }
  qbutton->setIconSize(LIVES_ICON_SIZE_BUTTON);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_focus_on_click(LiVESWidget *widget, boolean focus) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 20, 0)
  gtk_widget_set_focus_on_click(widget, focus);
#else
  if (!LIVES_IS_BUTTON(widget)) return FALSE;
  gtk_button_set_focus_on_click(LIVES_BUTTON(widget), focus);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QWidget *qw = (static_cast<QWidget *>(static_cast<QPushButton *>(button)));
  if (qw->focusPolicy() == Qt::NoFocus) return TRUE;
  if (focus) {
    qw->setFocusPolicy(Qt::StrongFocus);
  } else qw->setFocusPolicy(Qt::TabFocus);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_set_focus_on_click(LiVESButton *button, boolean focus) {
  return lives_widget_set_focus_on_click(LIVES_WIDGET(button), focus);
}


WIDGET_HELPER_GLOBAL_INLINE int lives_paned_get_position(LiVESPaned *paned) {
#ifdef GUI_GTK
  return gtk_paned_get_position(paned);
#endif
  return -1;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_paned_set_position(LiVESPaned *paned, int pos) {
  // call this only after adding widgets
#ifdef GUI_GTK
  gtk_paned_set_position(paned, pos);
  return TRUE;
#endif
#ifdef GUI_QT
  int size = 0;
  QList<int> qli = paned->sizes();
  for (int i = 0; i < qli.size(); i++) {
    size += qli.at(i);
  }
  qli.clear();
  qli.append(pos);
  qli.append(size - pos);
  paned->setSizes(qli);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_paned_pack(int where, LiVESPaned *paned, LiVESWidget *child, boolean resize,
    boolean shrink) {
#ifdef GUI_GTK
  if (where == 1) gtk_paned_pack1(paned, child, resize, shrink);
  else gtk_paned_pack2(paned, child, resize, shrink);
  return TRUE;
#endif
#ifdef GUI_QT
  paned->insertWidget(where - 1, child);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_drawing_area_new(void) {
  LiVESWidget *darea = NULL;
#ifdef GUI_GTK
  darea = gtk_drawing_area_new();
#endif
#ifdef GUI_QT
  darea = new LiVESDrawingArea;
#endif
  return darea;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_event_get_time(LiVESXEvent *event) {
#ifdef GUI_GTK
  return gdk_event_get_time(event);
#endif
#ifdef GUI_QT
  // TODO
  LiVESXEventButton *xevent = (LiVESXEventButton *)event;
  return xevent->time;
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_get_active(LiVESToggleButton *button) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_SWITCH(button)) return gtk_switch_get_active(LIVES_SWITCH(button));
#endif
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_set_active(LiVESToggleButton *button, boolean active) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (LIVES_IS_SWITCH(button)) gtk_switch_set_active(LIVES_SWITCH(button), active);
  else
#endif
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_set_mode(LiVESToggleButton *button, boolean drawind) {
#ifdef GUI_GTK
#if LIVES_HAS_SWITCH_WIDGET
  if (!LIVES_IS_SWITCH(button))
#endif
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), drawind);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toggle_tool_button_new(void) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = LIVES_WIDGET(gtk_toggle_tool_button_new());
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_tool_button_get_active(LiVESToggleToolButton *button) {
#ifdef GUI_GTK
  return gtk_toggle_tool_button_get_active(button);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_tool_button_set_active(LiVESToggleToolButton *button, boolean active) {
#ifdef GUI_GTK
  gtk_toggle_tool_button_set_active(button, active);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_radio_button_new(LiVESSList *group) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_radio_button_new(group);
#endif
#ifdef GUI_QT
  QButtonGroup *qbg;
  button = new LiVESRadioButton;
  if (group == NULL) {
    qbg = new QButtonGroup;
    group = lives_slist_append(group, (void *)qbg);
    qbg->setExclusive(true);
  } else {
    qbg = const_cast<QButtonGroup *>(static_cast<const QButtonGroup *>(lives_slist_nth_data(group, 0)));
  }

  (static_cast<LiVESRadioButton *>(button))->set_group(group);
  qbg->addButton(dynamic_cast<QPushButton *>(button));

#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_check_button_new(void) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_check_button_new();
#endif
#ifdef GUI_QT
  button = new LiVESCheckButton;
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_check_button_new_with_label(const char *label) {
  LiVESWidget *button = NULL;
#ifdef GUI_GTK
  button = gtk_check_button_new_with_label(label);
#endif
#ifdef GUI_QT
  button = new LiVESCheckButton;
  QString qlabel = QString::fromUtf8(label);
  if ((static_cast<LiVESButtonBase *>(button))->get_use_underline()) {
    qlabel = qlabel.replace('&', "&&");
    qlabel = qlabel.replace('_', '&');
  }
  (dynamic_cast<QAbstractButton *>(button))->setText(qlabel);
#endif
  return button;
}


static LiVESWidget *make_ttips_image(const char *text) {
  LiVESWidget *ttips_image = lives_image_new_from_stock(LIVES_STOCK_DIALOG_QUESTION,
                             LIVES_ICON_SIZE_SMALL_TOOLBAR);
  /// dont work...
  //lives_widget_set_bg_color(ttips_image, LIVES_WIDGET_STATE_NORMAL, &palette->dark_orange);
  lives_widget_set_no_show_all(ttips_image, TRUE);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ttips_image), TTIPS_IMAGE_KEY, ttips_image);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ttips_image), TTIPS_HIDE_KEY, ttips_image);
  lives_widget_set_tooltip_text(ttips_image, text);
  return ttips_image;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_widget_set_tooltip_text(LiVESWidget *widget, const char *tip_text) {
  LiVESWidget *img_tips = NULL;
  boolean ttips_override = FALSE;

  if (tip_text && *tip_text == '#' && !lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_IMAGE_KEY)) {
    widget = img_tips = make_ttips_image(tip_text + 1);
  } else {
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_OVERRIDE_KEY)) ttips_override = TRUE;
    if (!prefs->show_tooltips && !ttips_override) {
      if (tip_text != NULL)
        lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, (livespointer)(lives_strdup(tip_text)));
      else
        lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, (livespointer)(lives_strdup(tip_text)));
      return NULL;
    }
  }
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text(widget, tip_text);
#else
  GtkTooltips *tips;
  tips = gtk_tooltips_new();
  gtk_tooltips_set_tip(tips, widget, tip_text, NULL);
#endif
#endif
  return img_tips;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_grab_focus(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_set_can_focus(widget, TRUE);
  gtk_widget_grab_focus(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->setFocus(Qt::OtherFocusReason);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_grab_default(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_grab_default(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_PUSH_BUTTON(widget)) {
    QPushButton *button = dynamic_cast<QPushButton *>(widget);
    if (button != NULL)
      button->setDefault(true);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESSList *lives_radio_button_get_group(LiVESRadioButton *rbutton) {
#ifdef GUI_GTK
  return gtk_radio_button_get_group(rbutton);
#endif
#ifdef GUI_QT
  return rbutton->get_list();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_widget_get_parent(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_parent(widget);
#endif
#ifdef GUI_QT
  return widget->get_parent();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_widget_get_toplevel(LiVESWidget *widget) {
#ifdef GUI_GTK
  if (!GTK_IS_WIDGET(widget)) return NULL;
  return gtk_widget_get_toplevel(widget);
#endif
#ifdef GUI_QT
  QWidget *qwidget = widget->window();
  QVariant qv = qwidget->property("LiVESWidgetObject");
  if (qv.isValid()) {
    return static_cast<LiVESWidget *>(qv.value<LiVESWidgetObject *>());
  }
  return widget;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  return gtk_widget_get_window(widget);
#else
  return GDK_WINDOW(widget->window);
#endif
#endif
#ifdef GUI_QT
  QWidget *qwidget = static_cast<QWidget *>(widget);
  qwidget->winId();
  return qwidget->windowHandle();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWindow *lives_widget_get_window(LiVESWidget *widget) {
#ifdef GUI_GTK
  LiVESWidget *window = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
  if (GTK_IS_WINDOW(window)) return (LiVESWindow *)window;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_set_keep_above(LiVESXWindow *xwin, boolean setting) {
#ifdef GUI_GTK
  gdk_window_set_keep_above(xwin, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  bool vis = xwin->isVisible();
  const Qt::WindowFlags flags = xwin->flags();
  Qt::WindowFlags newflags = flags;
  if (setting) {
    newflags = flags | Qt::WindowStaysOnTopHint;
  } else {
    if (flags & Qt::WindowStaysOnBottomHint) newflags ^= Qt::WindowStaysOnTopHint;
  }
  xwin->setFlags(newflags);
  if (vis) xwin->setVisible(true);

  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_can_focus(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_widget_set_can_focus(widget, state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_FOCUS);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  if (state) {
    widget->setFocusPolicy(Qt::StrongFocus);
  } else widget->setFocusPolicy(Qt::NoFocus);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_can_default(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_widget_set_can_default(widget, state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_DEFAULT);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_DEFAULT);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_PUSH_BUTTON(widget)) {
    QPushButton *button = dynamic_cast<QPushButton *>(widget);
    if (button != NULL)
      button->setAutoDefault(state);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_add_events(LiVESWidget *widget, int events) {
#ifdef GUI_GTK
  gtk_widget_add_events(widget, events);
  return TRUE;
#endif
#ifdef GUI_QT
  events |= widget->get_events();
  widget->set_events(events);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_events(LiVESWidget *widget, int events) {
#ifdef GUI_GTK
  gtk_widget_set_events(widget, events);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_events(events);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_remove_accelerator(LiVESWidget *widget, LiVESAccelGroup *acgroup,
    uint32_t accel_key, LiVESXModifierType accel_mods) {
#ifdef GUI_GTK
  return gtk_widget_remove_accelerator(widget, acgroup, accel_key, accel_mods);
#endif
#ifdef GUI_QT
  return (static_cast<LiVESWidgetObject *>(widget))->remove_accels(acgroup, accel_key, accel_mods);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_preferred_size(LiVESWidget *widget, LiVESRequisition *min_size,
    LiVESRequisition *nat_size) {
  // for GTK 4.x we will use widget::measure()
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_get_preferred_size(widget, min_size, nat_size);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  *min_size = widget->minimumSize();
  *nat_size = widget->size();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_no_show_all(LiVESWidget *widget, boolean set) {
#ifdef GUI_GTK
  gtk_widget_set_no_show_all(widget, set);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_sensitive(LiVESWidget *widget) {
  // return TRUE is widget + parent is sensitive
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_is_sensitive(widget);
#else
  return GTK_WIDGET_IS_SENSITIVE(widget);
#endif
#endif
#ifdef GUI_QT
  return widget->isEnabled();
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_visible(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_get_visible(widget);
#else
  return GTK_WIDGET_VISIBLE(widget);
#endif
#endif
#ifdef GUI_QT
  return widget->isVisible();
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_is_realized(LiVESWidget *widget) {
  // used for giw widgets
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_get_realized(widget);
#else
  return GTK_WIDGET_REALIZED(widget);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_add(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_add(container, widget);
  return TRUE;
#endif
#ifdef GUI_QT
  if (container == NULL || widget == NULL) return TRUE;
  // types that cannot be cast to qlayout: paned, menushell, notebook, table, textview, toolbar, treeview

  if (LIVES_IS_PANED(container)) {
    QSplitter *ql = dynamic_cast<QSplitter *>(container);
    ql->addWidget(widget);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_SCROLLED_WINDOW(container)) {
    QScrollArea *ql = dynamic_cast<QScrollArea *>(container);
    ql->setWidget(widget);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU(container)) {
    QMenu *ql = dynamic_cast<QMenu *>(container);
    ql->addAction(dynamic_cast<QAction *>(widget));
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU_BAR(container)) {
    QMenuBar *ql = dynamic_cast<QMenuBar *>(container);
    ql->addAction(dynamic_cast<QAction *>(widget));
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_NOTEBOOK(container)) {
    LiVESNotebook *ql = static_cast<LiVESNotebook *>(container);
    ql->addTab(widget, NULL);
    ql->append_page();
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_TABLE(container)) {
    QGridLayout *ql = dynamic_cast<QGridLayout *>(container);
    ql->addWidget(widget, ql->rowCount(), 0);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_TOOLBAR(container)) {
    QToolBar *ql = dynamic_cast<QToolBar *>(container);
    ql->addWidget(widget);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_FRAME(container)) {
    LiVESFrame *ql = static_cast<LiVESFrame *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_TOOL_ITEM(container)) {
    LiVESToolItem *ql = static_cast<LiVESToolItem *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_WINDOW(container)) {
    LiVESMainWindow *ql = static_cast<LiVESMainWindow *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_DIALOG(container)) {
    LiVESDialog *ql = static_cast<LiVESDialog *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_BUTTON(container)) {
    LiVESButton *ql = static_cast<LiVESButton *>(container);
    container = ql->get_layout();
  }

  if (container != NULL) {
    QLayout *ql = dynamic_cast<QLayout *>(container);
    if (ql == NULL) {
      LIVES_WARN("Attempt to add widget to non-layout class");
      return FALSE;
    }
    ql->addWidget(widget);
    container->add_child(widget);
  }

  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_remove(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_remove(container, widget);
  return TRUE;
#endif
#ifdef GUI_QT
  // types that cannot be cast to qlayout: paned, menushell, notebook, table, textview, toolbar, treeview
  if (LIVES_IS_PANED(container)) {
    return FALSE; // cannot remove
  } else if (LIVES_IS_SCROLLED_WINDOW(container)) {
    QScrollArea *ql = dynamic_cast<QScrollArea *>(container);
    ql->takeWidget();
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU(container)) {
    QMenu *ql = dynamic_cast<QMenu *>(container);
    ql->removeAction(dynamic_cast<QAction *>(widget));
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU_BAR(container)) {
    QMenuBar *ql = dynamic_cast<QMenuBar *>(container);
    ql->removeAction(dynamic_cast<QAction *>(widget));
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_NOTEBOOK(container)) {
    QTabWidget *ql = dynamic_cast<QTabWidget *>(container);
    int tabno = container->get_child_index(widget);
    ql->removeTab(tabno);
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_TOOLBAR(container)) {
    QWidget *ql = static_cast<QWidget *>(container);
    ql->removeAction(dynamic_cast<QAction *>(widget));
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_FRAME(container)) {
    LiVESFrame *ql = static_cast<LiVESFrame *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_TOOL_ITEM(container)) {
    LiVESToolItem *ql = static_cast<LiVESToolItem *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_WINDOW(container)) {
    LiVESMainWindow *ql = static_cast<LiVESMainWindow *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_DIALOG(container)) {
    LiVESDialog *ql = static_cast<LiVESDialog *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_BUTTON(container)) {
    LiVESButton *ql = static_cast<LiVESButton *>(container);
    container = ql->get_layout();
  }

  if (container != NULL) {
    QLayout *ql = dynamic_cast<QLayout *>(container);
    if (ql == NULL) {
      LIVES_WARN("Attempt to remove widget to non-layout class");
      return FALSE;
    }
    ql->removeWidget(widget);
    container->remove_child(widget);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_set_border_width(LiVESContainer *container, uint32_t width) {
  // sets border OUTSIDE container
#ifdef GUI_GTK
  gtk_container_set_border_width(container, width);
  return TRUE;
#endif
#ifdef GUI_QT
  QLayout *ql = dynamic_cast<QLayout *>(container);
  if (ql == NULL) {
    LIVES_WARN("Attempt to add widget to non-layout class");
    return FALSE;
  }
  ql->setContentsMargins(width, width, width, width);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_foreach(LiVESContainer *cont, LiVESWidgetCallback callback,
    livespointer cb_data) {
  // excludes internal children
#ifdef GUI_GTK
  gtk_container_foreach(cont, callback, cb_data);
  return TRUE;
#endif
#ifdef GUI_QT
  for (int i = 0; i < cont->count_children(); ++i) {
    (*callback)(cont->get_child(i), cb_data);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE boolean lives_container_forall(LiVESContainer *cont, LiVESWidgetCallback callback,
    livespointer cb_data) {
  // includes internal children
#ifdef GUI_GTK
  gtk_container_forall(cont, callback, cb_data);
  return TRUE;
#endif
#ifdef GUI_QT
  /*  QLayout *ql = static_cast<QLayout *>(container);
      if (ql == NULL) {
      LIVES_WARN("Attempt to add widget to non-layout class");
      return FALSE;
      }
      for (int i = 0; i < ql->count(); ++i) {
      QWidget *widget = ql->itemAt(i)->widget();
      if (widget) {
      (*callback)(widget, cb_data);
      }
      }
      return TRUE;*/
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESList *lives_container_get_children(LiVESContainer *cont) {
  LiVESList *children = NULL;
#ifdef GUI_GTK
  children = gtk_container_get_children(cont);
#endif
#ifdef GUI_QT
  LiVESList *list = new LiVESList(*cont->get_children());
  return list;
#endif
  return children;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_set_focus_child(LiVESContainer *cont, LiVESWidget *child) {
#ifdef GUI_GTK
  gtk_container_set_focus_child(cont, child);
  return TRUE;
#endif
#ifdef GUI_QT
  if (child == NULL) {
    for (int i = 0; i < cont->count_children(); ++i) {
      if (cont->get_child(i)->hasFocus()) {
        cont->get_child(i)->clearFocus();
        break;
      }
    }
  } else {
    if (cont->hasFocus()) {
      child->setFocus();
    }
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_container_get_focus_child(LiVESContainer *cont) {
#ifdef GUI_GTK
  return gtk_container_get_focus_child(cont);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_progress_bar_new(void) {
  LiVESWidget *pbar = NULL;
#ifdef GUI_GTK
  pbar = gtk_progress_bar_new();
#endif
#ifdef GUI_QT
  pbar = new LiVESProgressBar;
#endif
  return pbar;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_progress_bar_set_fraction(LiVESProgressBar *pbar, double fraction) {
#ifdef GUI_GTK
  gtk_progress_bar_set_fraction(pbar, fraction);
  return TRUE;
#endif
#ifdef GUI_QT
  pbar->setMinimum(0);
  pbar->setMaximum(1000000);
  pbar->setValue(fraction * 1000000.);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_progress_bar_set_pulse_step(LiVESProgressBar *pbar, double fraction) {
#ifdef GUI_GTK
  gtk_progress_bar_set_pulse_step(pbar, fraction);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_progress_bar_pulse(LiVESProgressBar *pbar) {
#ifdef GUI_GTK
  gtk_progress_bar_pulse(pbar);
  return TRUE;
#endif
#ifdef GUI_QT
  pbar->setMinimum(0);
  pbar->setMaximum(0);
  pbar->setValue(qrand() % 99 + 1);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_spin_button_new(LiVESAdjustment *adj, double climb_rate, uint32_t digits) {
  LiVESWidget *sbutton = NULL;
#ifdef GUI_GTK
  sbutton = gtk_spin_button_new(adj, climb_rate, digits);
#endif
#ifdef GUI_QT
  sbutton = new LiVESSpinButton(adj, climb_rate, digits);
#endif
  return sbutton;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_spin_button_get_value(LiVESSpinButton *button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value(button);
#endif
#ifdef GUI_QT
  return button->value();
#endif
  return 0.;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_spin_button_get_value_as_int(LiVESSpinButton *button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value_as_int(button);
#endif
#ifdef GUI_QT
  return (int)button->value();
#endif
  return 0.;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_spin_button_get_adjustment(LiVESSpinButton *button) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_spin_button_get_adjustment(button);
#endif
#ifdef GUI_QT
  adj = button->get_adj();
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_spin_button_set_adjustment(LiVESSpinButton *button, LiVESAdjustment *adj) {
#ifdef GUI_GTK
  gtk_spin_button_set_adjustment(button, adj);
#endif
#ifdef GUI_QT
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_value(LiVESSpinButton *button, double value) {
  if (is_standard_widget(LIVES_WIDGET(button))) value = lives_spin_button_get_snapval(button, value);
#ifdef GUI_GTK
  gtk_spin_button_set_value(button, value);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setValue(value);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_range(LiVESSpinButton *button, double min, double max) {
#ifdef GUI_GTK
  gtk_spin_button_set_range(button, min, max);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESAdjustment *adj = button->get_adj();
  button->setRange(min, max);
  double value = button->value();
  if (value < min) value = min;
  if (value > max) value = max;
  button->setValue(value);
  adj->freeze();
  adj->set_lower(min);
  adj->set_upper(max);
  adj->thaw();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_wrap(LiVESSpinButton *button, boolean wrap) {
#ifdef GUI_GTK
  gtk_spin_button_set_wrap(button, wrap);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setWrapping(wrap);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_snap_to_ticks(LiVESSpinButton *button, boolean snap) {
#ifdef GUI_GTK
  gtk_spin_button_set_snap_to_ticks(button, snap);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_digits(LiVESSpinButton *button, uint32_t digits) {
#ifdef GUI_GTK
  gtk_spin_button_set_digits(button, digits);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setDecimals(digits);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_update(LiVESSpinButton *button) {
#ifdef GUI_GTK
  gtk_spin_button_update(button);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE; // not needed ?
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_tool_button_new(LiVESWidget *icon_widget, const char *label) {
  LiVESToolItem *button = NULL;
#ifdef GUI_GTK
  button = gtk_tool_button_new(icon_widget, label);
#endif
#ifdef GUI_QT
  button = new LiVESToolButton(icon_widget, label);
#endif
  return button;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_tool_item_new(void) {
  LiVESToolItem *item = NULL;
#ifdef GUI_GTK
  item = gtk_tool_item_new();
#endif
#ifdef GUI_QT
  item = new LiVESToolItem;
#endif
  return item;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_separator_tool_item_new(void) {
  LiVESToolItem *item = NULL;
#ifdef GUI_GTK
  item = gtk_separator_tool_item_new();
#endif
#ifdef GUI_QT
  item = new LiVESToolItem;
#endif
  return item;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tool_button_set_icon_widget(LiVESToolButton *button, LiVESWidget *icon) {
#ifdef GUI_GTK
  gtk_tool_button_set_icon_widget(button, icon);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_icon_widget(icon);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tool_button_get_icon_widget(LiVESToolButton *button) {
#ifdef GUI_GTK
  return gtk_tool_button_get_icon_widget(button);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tool_button_set_label_widget(LiVESToolButton *button, LiVESWidget *label) {
#ifdef GUI_GTK
  gtk_tool_button_set_label_widget(button, label);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_label_widget(label);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tool_button_get_label_widget(LiVESToolButton *button) {
#ifdef GUI_GTK
  return gtk_tool_button_get_label_widget(button);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tool_button_set_use_underline(LiVESToolButton *button, boolean use_underline) {
#ifdef GUI_GTK
  gtk_tool_button_set_use_underline(button, use_underline);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_use_underline(use_underline);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_ruler_set_range(LiVESRuler *ruler, double lower, double upper, double position,
    double max_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_range_set_range(GTK_RANGE(ruler), lower, upper);
  gtk_range_set_value(GTK_RANGE(ruler), position);
#else
  gtk_ruler_set_range(ruler, lower, upper, position, max_size);
  return TRUE;
#endif
#ifdef GUI_QT
  ruler->setMinimum(lower);
  ruler->setMaximum(upper);
  ruler->setValue(position);
  return TRUE;
#endif
  return FALSE;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_message_dialog_new(LiVESWindow *parent, LiVESDialogFlags flags,
    LiVESMessageType type,
    LiVESButtonsType buttons, const char *msg_fmt, ...) {
  LiVESWidget *mdial = NULL;
#ifdef GUI_GTK
  mdial = gtk_message_dialog_new(parent, flags | GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, msg_fmt, NULL);
#endif
  if (mdial != NULL && widget_opts.screen != NULL) lives_window_set_screen(parent, widget_opts.screen);
  return mdial;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_get_value(LiVESRuler *ruler) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  return gtk_range_get_value(GTK_RANGE(ruler));
#else
  return ruler->position;
#endif
#endif
#ifdef GUI_QT
  return ruler->value();
#endif
  return 0.;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_set_value(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_range_set_value(GTK_RANGE(ruler), value);
#else
  ruler->position = value;
#endif
#endif
#ifdef GUI_QT
  ruler->setValue(value);
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_set_upper(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
#ifdef ENABLE_GIW_3
  if (GIW_IS_TIMELINE(ruler))
    giw_timeline_set_range(GIW_TIMELINE(ruler), 0., value, giw_timeline_get_max_size(GIW_TIMELINE(ruler)));
  else
#endif
    gtk_adjustment_set_upper(gtk_range_get_adjustment(GTK_RANGE(ruler)), value);
#else
  ruler->upper = value;
#endif
#endif
#ifdef GUI_QT
  ruler->setMaximum(value);
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_ruler_set_lower(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_adjustment_set_lower(gtk_range_get_adjustment(GTK_RANGE(ruler)), value);
#else
  ruler->lower = value;
#endif
#endif
#ifdef GUI_QT
  ruler->setMinimum(value);
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_text_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
  renderer = gtk_cell_renderer_text_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_TEXT);
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_spin_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 10, 0)
  renderer = gtk_cell_renderer_spin_new();
#endif
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_SPIN);
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_toggle_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
  renderer = gtk_cell_renderer_toggle_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_TOGGLE);
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESCellRenderer *lives_cell_renderer_pixbuf_new(void) {
  LiVESCellRenderer *renderer = NULL;
#ifdef GUI_GTK
  renderer = gtk_cell_renderer_pixbuf_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_PIXBUF);
#endif
  return renderer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toolbar_new(void) {
  LiVESWidget *toolbar = NULL;
#ifdef GUI_GTK
  toolbar = gtk_toolbar_new();
#endif
#ifdef GUI_QT
  toolbar = new LiVESToolbar;
#endif
  return toolbar;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_insert(LiVESToolbar *toolbar, LiVESToolItem *item, int pos) {
#ifdef GUI_GTK
  gtk_toolbar_insert(toolbar, item, pos);
  return TRUE;
#endif
#ifdef GUI_QT
  QAction *act;
  int naction = toolbar->num_actions();
  if (pos >= naction) {
    act = toolbar->addWidget(static_cast<QWidget *>(item));
  } else {
    QAction *bef = toolbar->get_action(pos);
    act = toolbar->insertWidget(bef, static_cast<QWidget *>(item));
  }
  toolbar->add_action(act, pos);
  toolbar->add_child(item);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_set_show_arrow(LiVESToolbar *toolbar, boolean show) {
#ifdef GUI_GTK
  gtk_toolbar_set_show_arrow(toolbar, show);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESIconSize lives_toolbar_get_icon_size(LiVESToolbar *toolbar) {
#ifdef GUI_GTK
  return gtk_toolbar_get_icon_size(toolbar);
#endif
#ifdef GUI_QT
  return toolbar->iconSize();
#endif
  return LIVES_ICON_SIZE_INVALID;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_set_icon_size(LiVESToolbar *toolbar, LiVESIconSize icon_size) {
#ifdef GUI_GTK
  gtk_toolbar_set_icon_size(toolbar, icon_size);
  return TRUE;
#endif
#ifdef GUI_QT
  toolbar->setIconSize(icon_size);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toolbar_set_style(LiVESToolbar *toolbar, LiVESToolbarStyle style) {
#ifdef GUI_GTK
  gtk_toolbar_set_style(toolbar, style);
  return TRUE;
#endif
#ifdef GUI_QT
  toolbar->setToolButtonStyle(style);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_x(LiVESWidget *widget) {
  int x = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  x = alloc.x;
#else
  x = widget->allocation.x;
#endif
#endif
#ifdef GUI_QT
  QPoint p(0, 0);
  p = widget->mapToGlobal(p);
  x = p.x();
#endif
  return x;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_y(LiVESWidget *widget) {
  int y = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  y = alloc.y;
#else
  y = widget->allocation.y;
#endif
#endif
#ifdef GUI_QT
  QPoint p(0, 0);
  p = widget->mapToGlobal(p);
  y = p.y();
#endif
  return y;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_width(LiVESWidget *widget) {
  int width = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  width = alloc.width;
#else
  width = widget->allocation.width;
#endif
#endif
#ifdef GUI_QT
  width = widget->size().width();
#endif
  return width;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_widget_get_allocation_height(LiVESWidget *widget) {
  int height = 0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 18, 0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  height = alloc.height;
#else
  height = widget->allocation.height;
#endif
#endif
#ifdef GUI_QT
  height = widget->size().height();
#endif
  return height;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_state(LiVESWidget *widget, LiVESWidgetState state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_state_flags(widget, state, TRUE);
#else
  gtk_widget_set_state(widget, state);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_state(state);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetState lives_widget_get_state(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  return gtk_widget_get_state_flags(widget);
#else
#if GTK_CHECK_VERSION(2, 18, 0)
  return gtk_widget_get_state(widget);
#else
  return GTK_WIDGET_STATE(widget);
#endif
#endif
#endif

#ifdef GUI_QT
  return widget->get_state();
#else
  return (LiVESWidgetState)0;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_bin_get_child(LiVESBin *bin) {
  LiVESWidget *child = NULL;
#ifdef GUI_GTK
  child = gtk_bin_get_child(bin);
#endif
#ifdef GUI_QT
  return bin->get_child(0);
#endif
#ifdef GUI_QT
  if (LIVES_IS_SCROLLED_WINDOW(bin)) return bin;
  return bin->get_child(0);
#endif
  return child;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_upper(LiVESAdjustment *adj) {
  double upper = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  upper = gtk_adjustment_get_upper(adj);
#else
  upper = adj->upper;
#endif
#endif
#ifdef GUI_QT
  upper = adj->get_upper();
#endif
  return upper;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_lower(LiVESAdjustment *adj) {
  double lower = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  lower = gtk_adjustment_get_lower(adj);
#else
  lower = adj->lower;
#endif
#endif
#ifdef GUI_QT
  lower = adj->get_lower();
#endif
  return lower;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_page_size(LiVESAdjustment *adj) {
  double page_size = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  page_size = gtk_adjustment_get_page_size(adj);
#else
  page_size = adj->page_size;
#endif
#endif
#ifdef GUI_QT
  page_size = adj->get_page_size();
#endif
  return page_size;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_step_increment(LiVESAdjustment *adj) {
  double step_increment = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  step_increment = gtk_adjustment_get_step_increment(adj);
#else
  step_increment = adj->step_increment;
#endif
#endif
  return step_increment;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_adjustment_get_value(LiVESAdjustment *adj) {
  double value = 0.;
#ifdef GUI_GTK
  value = gtk_adjustment_get_value(adj);
#endif
#ifdef GUI_QT
  value = adj->get_value();
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_upper(LiVESAdjustment *adj, double upper) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_upper(adj, upper);
#else
  adj->upper = upper;
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_upper(upper);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_lower(LiVESAdjustment *adj, double lower) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_lower(adj, lower);
#else
  adj->lower = lower;
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_lower(lower);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_page_size(LiVESAdjustment *adj, double page_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_page_size(adj, page_size);
#else
  adj->page_size = page_size;
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_page_size(page_size);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_step_increment(LiVESAdjustment *adj, double step_increment) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_set_step_increment(adj, step_increment);
#else
  adj->step_increment = step_increment;
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_set_value(LiVESAdjustment *adj, double value) {
#ifdef GUI_GTK
  gtk_adjustment_set_value(adj, value);
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_value(value);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_adjustment_clamp_page(LiVESAdjustment *adj, double lower, double upper) {
#ifdef GUI_GTK
  gtk_adjustment_clamp_page(adj, lower, upper);
  return TRUE;
#endif
#ifdef GUI_QT
  if (adj->get_upper() > adj->get_value() + adj->get_page_size())
    adj->set_value(adj->get_upper() - adj->get_page_size());
  if (adj->get_lower() < adj->get_value()) adj->set_value(adj->get_lower());

  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_range_get_adjustment(LiVESRange *range) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_range_get_adjustment(range);
#endif
#ifdef GUI_QT
  adj = range->get_adj();
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_value(LiVESRange *range, double value) {
#ifdef GUI_GTK
  gtk_range_set_value(range, value);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_range(LiVESRange *range, double min, double max) {
#ifdef GUI_GTK
  gtk_range_set_range(range, min, max);
  return TRUE;
#endif
#ifdef GUI_QT
  double newval;
  LiVESAdjustment *adj = range->get_adj();
  if (adj->get_value() < min) newval = min;
  if (adj->get_value() > max) newval = max;
  adj->set_lower(min);
  adj->set_upper(max);
  adj->set_value(newval);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_increments(LiVESRange *range, double step, double page) {
#ifdef GUI_GTK
  gtk_range_set_increments(range, step, page);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESAdjustment *adj = range->get_adj();
  adj->set_step_increment(step);
  adj->set_page_increment(page);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_range_set_inverted(LiVESRange *range, boolean invert) {
#ifdef GUI_GTK
  gtk_range_set_inverted(range, invert);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_SCALE(range)) {
    (static_cast<QAbstractSlider *>(static_cast<LiVESScale *>(range)))->setInvertedAppearance(invert);
  } else {
    (static_cast<QAbstractSlider *>(static_cast<LiVESScrollbar *>(range)))->setInvertedAppearance(invert);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_range_get_value(LiVESRange *range) {
  double value = 0.;
#ifdef GUI_GTK
  value = gtk_range_get_value(range);
#endif
#ifdef GUI_QT
  value = range->get_adj()->get_value();
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_get(LiVESTreeModel *tmod, LiVESTreeIter *titer, ...) {
  boolean res = FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_model_get_valist(tmod, titer, argList);
  res = TRUE;
#endif
#ifdef GUI_QT
  // get 1 or more cells in row refd by titer
  // we have col number, locn to store value in
  char *attribute = va_arg(argList, char *);
  QVariant qv;

  while (attribute != NULL) {
    int colnum = va_arg(argList, int);
    int coltype = tmod->get_coltype(colnum);

    // types may be STRING, INT, (BOOLEAN, UINT, PIXBUF)
    if (coltype == LIVES_COL_TYPE_INT) {
      int *iattr = va_arg(argList, int *);
      qv = titer->data(colnum, Qt::DisplayRole);
      *iattr = qv.value<int>();
    }
    if (coltype == LIVES_COL_TYPE_STRING) {
      char **cattr = va_arg(argList, char **);
      qv = titer->data(colnum, Qt::DisplayRole);
      *cattr = strdup(qv.value<QString>().toUtf8().constData());
    }
    attribute = va_arg(argList, char *);
  }

#endif
  va_end(argList);
  return res;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_get_iter(LiVESTreeModel *tmod, LiVESTreeIter *titer,
    LiVESTreePath *tpath) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter(tmod, titer, tpath);
#endif
#ifdef GUI_QT
  int *indices = tpath->get_indices();
  int cnt = tpath->get_depth();

  QTreeWidgetItem *qtwi = tmod->get_qtree_widget()->invisibleRootItem();

  for (int i = 0; i < cnt; i++) {
    qtwi = qtwi->child(indices[i]);
  }

  *titer = *qtwi;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_get_iter_first(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter_first(tmod, titer);
#endif
#ifdef GUI_QT
  QTreeWidgetItem *qtwi = tmod->get_qtree_widget()->invisibleRootItem();
  qtwi = qtwi->child(0);
  *titer = *qtwi;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreePath *lives_tree_model_get_path(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
  LiVESTreePath *tpath = NULL;
#ifdef GUI_GTK
  tpath = gtk_tree_model_get_path(tmod, titer);
#endif
#ifdef GUI_QT
  QList<int> qli;
  QTreeWidgetItem *qtw = titer;
  int idx;
  QTreeWidget *twidget = tmod->get_qtree_widget();

  while (1) {
    if (qtw->parent() == 0) {
      idx = twidget->indexOfTopLevelItem(qtw);
      qli.append(idx);
      break;
    }
    idx = qtw->parent()->indexOfChild(qtw);
    qli.append(idx);
  }

  tpath = new LiVESTreePath(qli);

#endif
  return tpath;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_iter_children(LiVESTreeModel *tmod, LiVESTreeIter *titer,
    LiVESTreeIter *parent) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_children(tmod, titer, parent);
#endif
#ifdef GUI_QT
  if (parent == NULL || parent == tmod->get_qtree_widget()->invisibleRootItem()) {
    QTreeWidget *tw = tmod->get_qtree_widget();
    if (tw->topLevelItemCount() == 0) return FALSE;
    *titer = *tw->topLevelItem(0);
    return TRUE;
  }
  if (parent->childCount() == 0) return FALSE;
  *titer = *parent->child(0);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_tree_model_iter_n_children(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_n_children(tmod, titer);
#endif
#ifdef GUI_QT
  if (titer == NULL || titer == tmod->get_qtree_widget()->invisibleRootItem()) {
    // return num toplevel
    return tmod->get_qtree_widget()->topLevelItemCount();
  }
  return titer->childCount();
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_model_iter_next(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_next(tmod, titer);
#endif
#ifdef GUI_QT
  QTreeWidgetItem *parent = titer->parent();
  if (parent == NULL || parent == tmod->get_qtree_widget()->invisibleRootItem()) {
    QTreeWidget *tw = tmod->get_qtree_widget();
    int idx = tw->indexOfTopLevelItem(titer) + 1;
    if (idx >= tw->topLevelItemCount()) {
      return FALSE;
    }
    *titer = *(tw->topLevelItem(idx));
    return TRUE;
  }
  int idx = parent->indexOfChild(titer) + 1;
  if (idx >= parent->childCount()) {
    return FALSE;
  }
  *titer = *(parent->child(idx));
  return TRUE;

#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_path_free(LiVESTreePath *tpath) {
#ifdef GUI_GTK
  gtk_tree_path_free(tpath);
  return TRUE;
#endif
#ifdef GUI_QT
  delete tpath;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreePath *lives_tree_path_new_from_string(const char *path) {
  LiVESTreePath *tpath = NULL;
#ifdef GUI_GTK
  tpath = gtk_tree_path_new_from_string(path);
#endif
#ifdef GUI_QT
  tpath = new LiVESTreePath(path);
#endif
  return tpath;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_tree_path_get_depth(LiVESTreePath *tpath) {
  int depth = -1;
#ifdef GUI_GTK
  depth = gtk_tree_path_get_depth(tpath);
#endif
#ifdef GUI_QT
  return tpath->get_depth();
#endif
  return depth;
}


WIDGET_HELPER_GLOBAL_INLINE int *lives_tree_path_get_indices(LiVESTreePath *tpath) {
  int *indices = NULL;
#ifdef GUI_GTK
  indices = gtk_tree_path_get_indices(tpath);
#endif
#ifdef GUI_QT
  indices = tpath->get_indices();
#endif
  return indices;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeStore *lives_tree_store_new(int ncols, ...) {
  LiVESTreeStore *tstore = NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols > 0) {
    GType types[ncols];
    register int i;
    for (i = 0; i < ncols; i++) {
      types[i] = va_arg(argList, long unsigned int);
    }
    tstore = gtk_tree_store_newv(ncols, types);
  }
#ifdef GUI_GTK
  // supposedly speeds things up a bit...
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tstore),
                                       GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
                                       GTK_SORT_ASCENDING);
#endif
#endif

#ifdef GUI_QT
  if (ncols > 0) {
    QModelIndex qmi = QModelIndex();
    int types[ncols];

    for (int i = 0; i < ncols; i++) {
      types[i] = va_arg(argList, long unsigned int);
    }

    tstore = new LiVESTreeStore(ncols, types);
    tstore->insertColumns(0, ncols, qmi);
  }
#endif
  va_end(argList);
  return tstore;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_store_append(LiVESTreeStore *tstore, LiVESTreeIter *titer,
    LiVESTreeIter *parent) {
#ifdef GUI_GTK
  gtk_tree_store_append(tstore, titer, parent);
  return TRUE;
#endif
#ifdef GUI_QT
  QVariant qv = (static_cast<QAbstractItemModel *>(tstore))->property("LiVESWidgetObject");
  if (qv.isValid()) {
    LiVESTreeModel *ltm = static_cast<LiVESTreeModel *>(qv.value<LiVESWidgetObject *>());
    QTreeWidget *qtw = ltm->get_qtree_widget();

    if (parent != NULL) {
      QTreeWidgetItem *qtwi = new QTreeWidgetItem(parent);
      *titer = *qtwi;
      return TRUE;
    } else {
      QTreeWidgetItem *qtwi = new QTreeWidgetItem(qtw);
      *titer = *qtwi;
      return TRUE;
    }
  }
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_store_prepend(LiVESTreeStore *tstore, LiVESTreeIter *titer,
    LiVESTreeIter *parent) {
#ifdef GUI_GTK
  gtk_tree_store_prepend(tstore, titer, parent);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_store_set(LiVESTreeStore *tstore, LiVESTreeIter *titer, ...) {
  boolean res = FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_store_set_valist(tstore, titer, argList);
  res = TRUE;
#endif
#ifdef GUI_QT
  // set a row in the treestore
  QVariant qv;

  while (1) {
    int colnum = va_arg(argList, long unsigned int);
    if (colnum == -1) break;

    int coltype = tstore->get_coltype(colnum);

    // types may be STRING, INT, (BOOLEAN, UINT, PIXBUF)
    if (coltype == LIVES_COL_TYPE_INT) {
      int iattr = va_arg(argList, int);
      qv = QVariant::fromValue(iattr);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
    if (coltype == LIVES_COL_TYPE_STRING) {
      char *cattr = va_arg(argList, char *);
      QString qs = QString::fromUtf8(cattr);
      qv = QVariant::fromValue(qs);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
  }

#endif
  va_end(argList);
  return res;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tree_view_new_with_model(LiVESTreeModel *tmod) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_tree_view_new_with_model(tmod);
#endif
#ifdef GUI_QT
  LiVESTreeView *trview = new LiVESTreeView;
  trview->set_model(tmod);
  tview = static_cast<LiVESWidget *>(trview);
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_tree_view_new(void) {
  LiVESWidget *tview = NULL;
#ifdef GUI_GTK
  tview = gtk_tree_view_new();
#endif
#ifdef GUI_QT
  tview = new LiVESTreeView;
#endif
  return tview;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_set_model(LiVESTreeView *tview, LiVESTreeModel *tmod) {
#ifdef GUI_GTK
  gtk_tree_view_set_model(tview, tmod);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->set_model(tmod);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeModel *lives_tree_view_get_model(LiVESTreeView *tview) {
  LiVESTreeModel *tmod = NULL;
#ifdef GUI_GTK
  tmod = gtk_tree_view_get_model(tview);
#endif
#ifdef GUI_QT
  tmod = tview->get_model();
#endif
  return tmod;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeSelection *lives_tree_view_get_selection(LiVESTreeView *tview) {
  LiVESTreeSelection *tsel = NULL;
#ifdef GUI_GTK
  tsel = gtk_tree_view_get_selection(tview);
#endif
#ifdef GUI_QT
  tsel = tview;
#endif
  return tsel;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_tree_view_append_column(LiVESTreeView *tview, LiVESTreeViewColumn *tvcol) {
#ifdef GUI_GTK
  gtk_tree_view_append_column(tview, tvcol);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->append_column(tvcol);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_set_headers_visible(LiVESTreeView *tview, boolean vis) {
#ifdef GUI_GTK
  gtk_tree_view_set_headers_visible(tview, vis);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setHeaderHidden(!vis);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_tree_view_get_hadjustment(LiVESTreeView *tview) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  adj = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(tview));
#else
  adj = gtk_tree_view_get_hadjustment(tview);
#endif
#endif
#ifdef GUI_QT
  adj = tview->get_hadj();
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESTreeViewColumn *lives_tree_view_column_new_with_attributes(const char *title,
    LiVESCellRenderer *crend,
    ...) {
  LiVESTreeViewColumn *tvcol = NULL;
  va_list args;
  va_start(args, crend);
  int column;
  char *attribute;
  boolean expand = FALSE;
#ifdef GUI_GTK

  tvcol = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(tvcol, title);
  gtk_tree_view_column_pack_start(tvcol, crend, expand);

  attribute = va_arg(args, char *);

  while (attribute != NULL) {
    column = va_arg(args, int);
    gtk_tree_view_column_add_attribute(tvcol, crend, attribute, column);
    attribute = va_arg(args, char *);
  }

#endif
#ifdef GUI_QT
  tvcol = crend;
  tvcol->set_title(title);
  tvcol->set_expand(expand);
  while (attribute != NULL) {
    column = va_arg(args, int);
    tvcol->add_attribute(attribute, column);
    attribute = va_arg(args, char *);
  }
#endif
  va_end(args);
  return tvcol;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_column_set_sizing(LiVESTreeViewColumn *tvcol,
    LiVESTreeViewColumnSizing type) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_sizing(tvcol, type);
  return TRUE;
#endif
#ifdef GUI_QT
  tvcol->set_sizing(type);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_view_column_set_fixed_width(LiVESTreeViewColumn *tvcol, int fwidth) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_fixed_width(tvcol, fwidth);
  return TRUE;
#endif
#ifdef GUI_QT
  tvcol->set_fixed_width(fwidth);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_selection_get_selected(LiVESTreeSelection *tsel, LiVESTreeModel **tmod,
    LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_selection_get_selected(tsel, tmod, titer);
#endif
#ifdef GUI_QT
  int mode = tsel->selectionMode();
  if (mode == LIVES_SELECTION_MULTIPLE) return TRUE;
  // tsel should have single seln.
  QList<QTreeWidgetItem *> qtwi = tsel->selectedItems();
  *titer = *(qtwi.at(0));
  *tmod = tsel->get_model();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_selection_set_mode(LiVESTreeSelection *tsel, LiVESSelectionMode tselmod) {
#ifdef GUI_GTK
  gtk_tree_selection_set_mode(tsel, tselmod);
  return TRUE;
#endif
#ifdef GUI_QT
  tsel->setSelectionMode(tselmod);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_tree_selection_select_iter(LiVESTreeSelection *tsel, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  gtk_tree_selection_select_iter(tsel, titer);
  return TRUE;
#endif
#ifdef GUI_QT
  titer->setSelected(true);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESListStore *lives_list_store_new(int ncols, ...) {
  LiVESListStore *lstore = NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols > 0) {
    GType types[ncols];
    register int i;
    for (i = 0; i < ncols; i++) {
      types[i] = va_arg(argList, long unsigned int);
    }
    lstore = gtk_list_store_newv(ncols, types);
  }
#endif

#ifdef GUI_QT
  if (ncols > 0) {
    QModelIndex qmi = QModelIndex();
    int types[ncols];

    for (int i = 0; i < ncols; i++) {
      types[i] = va_arg(argList, long unsigned int);
    }

    lstore = new LiVESListStore(ncols, types);
    lstore->insertColumns(0, ncols, qmi);
  }
#endif
  va_end(argList);
  return lstore;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_list_store_set(LiVESListStore *lstore, LiVESTreeIter *titer, ...) {
  boolean res = FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_list_store_set_valist(lstore, titer, argList);
  res = TRUE;
#endif
#ifdef GUI_QT
  // set a row in the liststore
  QVariant qv;

  while (1) {
    int colnum = va_arg(argList, int);
    if (colnum == -1) break;

    int coltype = lstore->get_coltype(colnum);

    // types may be STRING, INT, (BOOLEAN, UINT, PIXBUF)
    if (coltype == LIVES_COL_TYPE_INT) {
      int iattr = va_arg(argList, long unsigned int);
      qv = QVariant::fromValue(iattr);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
    if (coltype == LIVES_COL_TYPE_STRING) {
      char *cattr = va_arg(argList, char *);
      QString qs = QString::fromUtf8(cattr);
      qv = QVariant::fromValue(qs);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
  }

#endif
  va_end(argList);
  return res;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_list_store_insert(LiVESListStore *lstore, LiVESTreeIter *titer, int position) {
#ifdef GUI_GTK
  gtk_list_store_insert(lstore, titer, position);
  return TRUE;
#endif
#ifdef GUI_QT
  lstore->insertRow(position);
  QTreeWidgetItem *qtwi = qtwi->child(position);
  *titer = *qtwi;
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_label_get_text(LiVESLabel *label) {
#ifdef GUI_GTK
  return gtk_label_get_text(label);
#endif
#ifdef GUI_QT
  return label->text().toUtf8().constData();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_text(LiVESLabel *label, const char *text) {
#ifdef GUI_GTK
  if (widget_opts.mnemonic_label) gtk_label_set_text_with_mnemonic(label, text);
  else gtk_label_set_text(label, text);
  return TRUE;
#endif
#ifdef GUI_QT
  label->setTextFormat(Qt::PlainText);
  label->setText(QString::fromUtf8(text));
  label->set_text(QString::fromUtf8(text));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_markup(LiVESLabel *label, const char *markup) {
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) gtk_label_set_markup(label, markup);
  else gtk_label_set_markup_with_mnemonic(label, markup);
  return TRUE;
#endif
#ifdef GUI_QT
  label->setTextFormat(Qt::RichText);
  label->setText(QString::fromUtf8(markup));
  label->set_text(QString::fromUtf8(markup));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_mnemonic_widget(LiVESLabel *label, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_label_set_mnemonic_widget(label, widget);
  return TRUE;
#endif
#ifdef GUI_QT
  label->set_mnemonic_widget(widget);
  (static_cast<QLabel *>(label))->setBuddy(static_cast<QWidget *>(widget));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_label_get_mnemonic_widget(LiVESLabel *label) {
  LiVESWidget *widget = NULL;
#ifdef GUI_GTK
  widget = gtk_label_get_mnemonic_widget(label);
#endif
#ifdef GUI_QT
  widget = label->get_mnemonic_widget();
#endif
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_label_set_selectable(LiVESLabel *label, boolean setting) {
#ifdef GUI_GTK
  gtk_label_set_selectable(label, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  QLabel *qlabel = static_cast<QLabel *>(label);
  Qt::TextInteractionFlags flags = qlabel->textInteractionFlags();
  if (setting) {
    flags |= Qt::TextSelectableByMouse;
    flags |= Qt::TextSelectableByKeyboard;
  } else {
    if (flags & Qt::TextSelectableByMouse) flags ^= Qt::TextSelectableByMouse;
    if (flags & Qt::TextSelectableByKeyboard) flags ^= Qt::TextSelectableByKeyboard;
  }
  qlabel->setTextInteractionFlags(flags);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_editable_get_editable(LiVESEditable *editable) {
#ifdef GUI_GTK
  return gtk_editable_get_editable(editable);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_editable_set_editable(LiVESEditable *editable, boolean is_editable) {
#ifdef GUI_GTK
  gtk_editable_set_editable(editable, is_editable);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_LABEL(editable)) {
    QLabel *qlabel = dynamic_cast<QLabel *>(editable);
    Qt::TextInteractionFlags flags = qlabel->textInteractionFlags();
    if (is_editable) {
      flags |= Qt::TextEditable;
    } else {
      if (flags & Qt::TextEditable) flags ^= Qt::TextEditable;
    }
    qlabel->setTextInteractionFlags(flags);
  } else {
    QAbstractSpinBox *qsb = dynamic_cast<QAbstractSpinBox *>(editable);
    qsb->setReadOnly(!is_editable);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_editable_select_region(LiVESEditable *editable, int start_pos, int end_pos) {
#ifdef GUI_GTK
  gtk_editable_select_region(editable, start_pos, end_pos);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_LABEL(editable)) {
    QLabel *qlabel = dynamic_cast<QLabel *>(editable);
    qlabel->setSelection(start_pos, end_pos);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_entry_new(void) {
  LiVESWidget *entry = NULL;
#ifdef GUI_GTK
  entry = gtk_entry_new();
#endif
#ifdef GUI_QT
  entry = new LiVESEntry();
#endif
  return entry;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_max_length(LiVESEntry *entry, int len) {
  // entry length (not display length)
#ifdef GUI_GTK
  gtk_entry_set_max_length(entry, len);
  return TRUE;
#endif
#ifdef GUI_QT
  entry->setMaxLength(len);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_activates_default(LiVESEntry *entry, boolean act) {
#ifdef GUI_GTK
  gtk_entry_set_activates_default(entry, act);
  return TRUE;
#endif
#ifdef GUI_QT
  // do nothing, enter is picked up by dialog
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_get_activates_default(LiVESEntry *entry) {
#ifdef GUI_GTK
  return gtk_entry_get_activates_default(entry);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_visibility(LiVESEntry *entry, boolean vis) {
#ifdef GUI_GTK
  gtk_entry_set_visibility(entry, vis);
  return TRUE;
#endif
#ifdef GUI_QT
  if (!vis) {
    entry->setEchoMode(QLineEdit::Password);
  } else {
    entry->setEchoMode(QLineEdit::Normal);
  }
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_has_frame(LiVESEntry *entry, boolean has) {
#ifdef GUI_GTK
  gtk_entry_set_has_frame(entry, has);
  return TRUE;
#endif
#ifdef GUI_QT
  entry->setFrame(has);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_alignment(LiVESEntry *entry, float align) {
#ifdef GUI_GTK
  gtk_entry_set_alignment(entry, align);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_entry_get_text(LiVESEntry *entry) {
#ifdef GUI_GTK
  return gtk_entry_get_text(entry);
#endif
#ifdef GUI_QT
  return entry->text().toUtf8().constData();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_text(LiVESEntry *entry, const char *text) {
#ifdef GUI_GTK
  gtk_entry_set_text(entry, text);
  return TRUE;
#endif
#ifdef GUI_QT
  entry->setText(QString::fromUtf8(text));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_width_chars(LiVESEntry *entry, int nchars) {
  // display length
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 12, 0)
  gtk_entry_set_max_width_chars(entry, nchars);
#else
  gtk_entry_set_width_chars(entry, nchars);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QFontMetrics metrics(QApplication::font());
  (static_cast<QLineEdit *>(entry))->setFixedSize(metrics.width('A')*nchars,
      (static_cast<QWidget *>(static_cast<QLineEdit *>(entry)))->height());
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_scrolled_window_new(LiVESAdjustment *hadj, LiVESAdjustment *vadj) {
  LiVESWidget *swindow = NULL;
#ifdef GUI_GTK
  swindow = gtk_scrolled_window_new(hadj, vadj);
#endif
#ifdef GUI_QT
  swindow = new LiVESScrolledWindow(hadj, vadj);
#endif
  return swindow;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_scrolled_window_get_hadjustment(LiVESScrolledWindow *swindow) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_scrolled_window_get_hadjustment(swindow);
#endif
#ifdef GUI_QT
  adj = swindow->get_hadj();
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAdjustment *lives_scrolled_window_get_vadjustment(LiVESScrolledWindow *swindow) {
  LiVESAdjustment *adj = NULL;
#ifdef GUI_GTK
  adj = gtk_scrolled_window_get_vadjustment(swindow);
#endif
#ifdef GUI_QT
  adj = swindow->get_vadj();
#endif
  return adj;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_set_policy(LiVESScrolledWindow *scrolledwindow,
    LiVESPolicyType hpolicy,
    LiVESPolicyType vpolicy) {
#ifdef GUI_GTK
  gtk_scrolled_window_set_policy(scrolledwindow, hpolicy, vpolicy);
  return TRUE;
#endif
#ifdef GUI_QT
  scrolledwindow->setHorizontalScrollBarPolicy(hpolicy);
  scrolledwindow->setVerticalScrollBarPolicy(hpolicy);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_add_with_viewport(LiVESScrolledWindow *scrolledwindow,
    LiVESWidget *child) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 8, 0)
  gtk_scrolled_window_add_with_viewport(scrolledwindow, child);
#else
  lives_container_add(LIVES_CONTAINER(scrolledwindow), child);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  scrolledwindow->setWidget(static_cast<QWidget *>(child));
  scrolledwindow->add_child(child);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_set_min_content_height(LiVESScrolledWindow *scrolledwindow,
    int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_scrolled_window_set_min_content_height(scrolledwindow, height);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scrolled_window_set_min_content_width(LiVESScrolledWindow *scrolledwindow,
    int width) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_scrolled_window_set_min_content_width(scrolledwindow, width);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_raise(LiVESXWindow *xwin) {
#ifdef GUI_GTK
  gdk_window_raise(xwin);
  return TRUE;
#endif
#ifdef GUI_QT
  xwin->raise();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_xwindow_set_cursor(LiVESXWindow *xwin, LiVESXCursor *cursor) {
#ifdef GUI_GTK
  if (GDK_IS_WINDOW(xwin)) {
    if (cursor == NULL || gdk_window_get_display(xwin) == gdk_cursor_get_display(cursor)) {
      gdk_window_set_cursor(xwin, cursor);
      return TRUE;
    }
  }
#endif
#ifdef GUI_QT
  if (cursor != NULL)
    xwin->setCursor(*cursor);
  else
    xwin->unsetCursor();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_dialog_set_has_separator(LiVESDialog *dialog, boolean has) {
  // return TRUE if implemented

#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator(dialog, has);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_hexpand(LiVESWidget *widget, boolean state) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand(widget, state);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_vexpand(LiVESWidget *widget, boolean state) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_vexpand(widget, state);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_new(void) {
  LiVESWidget *menu = NULL;
#ifdef GUI_GTK
  menu = gtk_menu_new();
#endif
#ifdef GUI_QT
  menu = new LiVESMenu;
#endif
  return menu;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_bar_new(void) {
  LiVESWidget *menubar = NULL;
#ifdef GUI_GTK
  menubar = gtk_menu_bar_new();
#endif
#ifdef GUI_QT
  menubar = new LiVESMenuBar;
#endif
  return menubar;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_item_new(void) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  menuitem = gtk_menu_item_new();
#endif
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(LIVES_MAIN_WINDOW_WIDGET);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) menuitem = gtk_menu_item_new_with_label(label);
  else menuitem = gtk_menu_item_new_with_mnemonic(label);
#endif
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(QString::fromUtf8(label), LIVES_MAIN_WINDOW_WIDGET);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_item_set_accel_path(LiVESMenuItem *menuitem, const char *path) {
#ifdef GUI_GTK
  gtk_menu_item_set_accel_path(menuitem, path);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_menu_item_get_submenu(LiVESMenuItem *menuitem) {
#ifdef GUI_GTK
  return gtk_menu_item_get_submenu(menuitem);
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  if (!widget_opts.mnemonic_label) menuitem = gtk_menu_item_new_with_label(label);
  else menuitem = gtk_menu_item_new_with_mnemonic(label);
#else
  if (!widget_opts.mnemonic_label) menuitem = gtk_image_menu_item_new_with_label(label);
  else menuitem = gtk_image_menu_item_new_with_mnemonic(label);
#endif
#endif
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(QString::fromUtf8(label), LIVES_MAIN_WINDOW_WIDGET);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_radio_menu_item_new_with_label(LiVESSList *group, const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) menuitem = gtk_radio_menu_item_new_with_label(group, label);
  else menuitem = gtk_radio_menu_item_new_with_mnemonic(group, label);
#endif
#ifdef GUI_QT
  QActionGroup *qag;
  LiVESRadioMenuItem *xmenuitem = new LiVESRadioMenuItem(QString::fromUtf8(label), LIVES_MAIN_WINDOW_WIDGET);
  if (group == NULL) {
    qag = new QActionGroup(NULL);
    group = lives_slist_append(group, (void *)qag);
    qag->setExclusive(true);
  } else {
    qag = const_cast<QActionGroup *>(static_cast<const QActionGroup *>(lives_slist_nth_data(group, 0)));
  }

  xmenuitem->set_group(group);
  qag->addAction(static_cast<QAction *>(xmenuitem));
  menuitem = static_cast<LiVESWidget *>(xmenuitem);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESSList *lives_radio_menu_item_get_group(LiVESRadioMenuItem *rmenuitem) {
#ifdef GUI_GTK
  return gtk_radio_menu_item_get_group(rmenuitem);
#endif
#ifdef GUI_QT
  return rmenuitem->get_list();
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_check_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
  if (!widget_opts.mnemonic_label) menuitem = gtk_check_menu_item_new_with_label(label);
  else menuitem = gtk_check_menu_item_new_with_mnemonic(label);   // TODO - deprecated
#endif
#ifdef GUI_QT
  menuitem = new LiVESCheckMenuItem(QString::fromUtf8(label), LIVES_MAIN_WINDOW_WIDGET);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_check_menu_item_set_draw_as_radio(LiVESCheckMenuItem *item, boolean setting) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_draw_as_radio(item, setting);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id,
    LiVESAccelGroup *accel_group) {
  LiVESWidget *menuitem = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  char *xstock_id = lives_strdup(stock_id); // need to back this up as we will use translation functions
  menuitem = gtk_menu_item_new_with_mnemonic(xstock_id);

  if (!strcmp(xstock_id, LIVES_STOCK_LABEL_SAVE)) {
    lives_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem), LIVES_ACCEL_PATH_SAVE);
  }

  if (!strcmp(xstock_id, LIVES_STOCK_LABEL_QUIT)) {
    lives_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem), LIVES_ACCEL_PATH_QUIT);
  }
  lives_free(xstock_id);
#else
  menuitem = gtk_image_menu_item_new_from_stock(stock_id, accel_group);
#endif
#endif
#ifdef GUI_QT
  char *xstock_id = lives_strdup(stock_id); // need to back this up as we will use translation functions
  LiVESMenuItem *xmenuitem = new LiVESMenuItem(qmake_mnemonic(QString::fromUtf8(xstock_id)), LIVES_MAIN_WINDOW_WIDGET);

  if (!strcmp(xstock_id, LIVES_STOCK_LABEL_SAVE)) {
    xmenuitem->setShortcut(make_qkey_sequence(LIVES_KEY_s, LIVES_CONTROL_MASK));
  }

  if (!strcmp(xstock_id, LIVES_STOCK_LABEL_QUIT)) {
    xmenuitem->setMenuRole(QAction::QuitRole);
    xmenuitem->setShortcut(make_qkey_sequence(LIVES_KEY_q, LIVES_CONTROL_MASK));
  }
  lives_free(xstock_id);

  menuitem = static_cast<LiVESWidget *>(xmenuitem);
#endif
  return menuitem;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESToolItem *lives_menu_tool_button_new(LiVESWidget *icon, const char *label) {
  LiVESToolItem *toolitem = NULL;
#ifdef GUI_GTK
  toolitem = gtk_menu_tool_button_new(icon, label);
#endif
#ifdef GUI_QT
  toolitem = new LiVESMenuToolButton(QString::fromUtf8(label), LIVES_MAIN_WINDOW_WIDGET, icon);
#endif
  return toolitem;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_tool_button_set_menu(LiVESMenuToolButton *toolbutton, LiVESWidget *menu) {
#ifdef GUI_GTK
  gtk_menu_tool_button_set_menu(toolbutton, menu);
  return TRUE;
#endif
#ifdef GUI_QT
  (dynamic_cast<QMenu *>(menu))->addAction(static_cast<QAction *>(toolbutton));
  menu->add_child(toolbutton);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_item_set_submenu(LiVESMenuItem *menuitem, LiVESWidget *menu) {
#ifdef GUI_GTK
  gtk_menu_item_set_submenu(menuitem, menu);

#ifdef GTK_SUBMENU_SENS_BUG
  if (!lives_widget_is_sensitive(LIVES_WIDGET(menuitem))) {
    //g_print("Warning, adding submenu when insens!");
    //assert(FALSE);
  }
#endif

  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QAction *>(menuitem))->setMenu(dynamic_cast<QMenu *>(menu));
  menuitem->add_child(menu);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_item_activate(LiVESMenuItem *menuitem) {
#ifdef GUI_GTK
  gtk_menu_item_activate(menuitem);
  return TRUE;
#endif
#ifdef GUI_QT
  menuitem->activate(QAction::Trigger);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_check_menu_item_set_active(LiVESCheckMenuItem *item, boolean state) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_active(item, state);
  return TRUE;
#endif
#ifdef GUI_QT
  item->setChecked(state);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_check_menu_item_get_active(LiVESCheckMenuItem *item) {
#ifdef GUI_GTK
  return gtk_check_menu_item_get_active(item);
#endif
#ifdef GUI_QT
  return item->isChecked();
#endif
  return FALSE;
}


#if !GTK_CHECK_VERSION(3, 10, 0)

WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_menu_item_set_image(LiVESImageMenuItem *item, LiVESWidget *image) {
#ifdef GUI_GTK
  gtk_image_menu_item_set_image(item, image);
  return TRUE;
#endif
#ifdef GUI_QT
  QImage *qim = dynamic_cast<QImage *>(image);
  if (qim != NULL) {
    QPixmap qpx;
    qpx.convertFromImage(*qim);
    QIcon *qi = new QIcon(qpx);
    item->setIcon(*qi);
  }
  return TRUE;
#endif
  return FALSE;
}

#endif

WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_set_title(LiVESMenu *menu, const char *title) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 10, 0)
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  gtk_menu_set_title(menu, ntitle);
  lives_free(ntitle);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  menu->setTitle(QString::fromUtf8(title));
  lives_free(ntitle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_popup(LiVESMenu *menu, LiVESXEventButton *event) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 22, 0)
  gtk_menu_popup_at_pointer(menu, NULL);
#else
  gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  menu->popup((static_cast<QWidget *>(static_cast<LiVESWidget *>(menu)))->pos());
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_reorder_child(LiVESMenu *menu, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_menu_reorder_child(menu, child, pos);
  return TRUE;
#endif
#ifdef GUI_QT
  menu->reorder_child(child, pos);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_detach(LiVESMenu *menu) {
  // NB also calls detacher callback
#ifdef GUI_GTK
  gtk_menu_detach(menu);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESTornOffMenu *ltom = new LiVESTornOffMenu(menu);
  ltom->no_refcount();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_shell_append(LiVESMenuShell *menushell, LiVESWidget *child) {
#ifdef GUI_GTK
  gtk_menu_shell_append(menushell, child);
  return TRUE;
#endif
#ifdef GUI_QT
  QAction *action = dynamic_cast<QAction *>(child);
  if (LIVES_IS_MENU(menushell)) {
    QMenu *qmenu = dynamic_cast<QMenu *>(menushell);
    qmenu->addAction(action);
  } else {
    QMenuBar *qmenu = dynamic_cast<QMenuBar *>(menushell);
    qmenu->addAction(action);
  }
  menushell->add_child(child);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_shell_insert(LiVESMenuShell *menushell, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_menu_shell_insert(menushell, child, pos);
  return TRUE;
#endif
#ifdef GUI_QT
  lives_menu_shell_append(menushell, child);
  if (LIVES_IS_MENU(menushell))(static_cast<LiVESMenu *>(menushell))->reorder_child(child, pos);
  else (dynamic_cast<LiVESMenuBar *>(menushell))->reorder_child(child, pos);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_menu_shell_prepend(LiVESMenuShell *menushell, LiVESWidget *child) {
#ifdef GUI_GTK
  gtk_menu_shell_prepend(menushell, child);
  return TRUE;
#endif
#ifdef GUI_QT
  return lives_menu_shell_insert(menushell, child, 0);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem *item, boolean show) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 16, 0)
#if !GTK_CHECK_VERSION(3, 10, 0)
  gtk_image_menu_item_set_always_show_image(item, show);
#endif
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  item->setIconVisibleInMenu(show);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_set_draw_value(LiVESScale *scale, boolean draw_value) {
#ifdef GUI_GTK
  return TRUE;
#endif
#ifdef GUI_QT
  return !draw_value;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_set_value_pos(LiVESScale *scale, LiVESPositionType ptype) {
#ifdef GUI_GTK
  gtk_scale_set_value_pos(scale, ptype);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
  return FALSE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_set_digits(LiVESScale *scale, int digits) {
#ifdef GUI_GTK
  gtk_scale_set_digits(scale, digits);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
  return FALSE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_button_set_orientation(LiVESScaleButton *scale, LiVESOrientation orientation) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_orientable_set_orientation(GTK_ORIENTABLE(scale), orientation);
  return TRUE;
#else
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_scale_button_set_orientation(scale, orientation);
  return TRUE;
#endif
#endif
#endif
#ifdef GUI_QT
  scale->setOrientation(orientation);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_scale_button_get_value(LiVESScaleButton *scale) {
  double value = 0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  value = gtk_scale_button_get_value(scale);
#else
  value = gtk_adjustment_get_value(gtk_range_get_adjustment(scale));
#endif
#endif
#ifdef GUI_QT
  value = scale->value();
#endif
  return value;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_scale_button_set_value(LiVESScaleButton *scale, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_scale_button_set_value(scale, value);
#else
  gtk_adjustment_set_value(gtk_range_get_adjustment(scale), value);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  value = scale->value();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE char *lives_file_chooser_get_filename(LiVESFileChooser *chooser) {
  char *fname = NULL;
#ifdef GUI_GTK
  fname = gtk_file_chooser_get_filename(chooser);
#endif
#ifdef GUI_QT
  QStringList qsl = chooser->selectedFiles();
  fname = strdup(qsl.at(0).toUtf8().constData());
#endif
  return fname;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser *chooser) {
  LiVESSList *fnlist = NULL;
#ifdef GUI_GTK
  fnlist = gtk_file_chooser_get_filenames(chooser);
#endif
#ifdef GUI_QT
  QStringList qsl = chooser->selectedFiles();
  for (int i = 0; i < qsl.size(); i++) {
    fnlist = lives_slist_append(fnlist, ((livesconstpointer)(strdup(qsl.at(0).toUtf8().constData()))));
  }
#endif
  return fnlist;
}


#if GTK_CHECK_VERSION(3,2,0)
WIDGET_HELPER_GLOBAL_INLINE char *lives_font_chooser_get_font(LiVESFontChooser *fc) {
  return gtk_font_chooser_get_font(fc);
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_font_chooser_set_font(LiVESFontChooser *fc,
    const char *fontname) {
  gtk_font_chooser_set_font(fc, fontname);
  return TRUE;
}

WIDGET_HELPER_GLOBAL_INLINE LingoFontDescription *lives_font_chooser_get_font_desc(LiVESFontChooser *fc) {
  return gtk_font_chooser_get_font_desc(fc);
}

WIDGET_HELPER_GLOBAL_INLINE boolean lives_font_chooser_set_font_desc(LiVESFontChooser *fc,
    LingoFontDescription *lfd) {
  gtk_font_chooser_set_font_desc(fc, lfd);
  return TRUE;
}
#endif


#ifdef GUI_GTK
WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_grid_new(void) {
  LiVESWidget *grid = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  grid = gtk_grid_new();
#endif
#endif
  return grid;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_set_row_spacing(LiVESGrid *grid, uint32_t spacing) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_set_row_spacing(grid, spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_set_column_spacing(LiVESGrid *grid, uint32_t spacing) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_set_column_spacing(grid, spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_remove_row(LiVESGrid *grid, int posn) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  gtk_grid_remove_row(grid, posn);
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_insert_row(LiVESGrid *grid, int posn) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  gtk_grid_insert_row(grid, posn);
  return TRUE;
#endif

  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_grid_attach_next_to(LiVESGrid *grid, LiVESWidget *child, LiVESWidget *sibling,
    LiVESPositionType side, int width, int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_attach_next_to(grid, child, sibling, side, width, height);
  return TRUE;
#endif
#endif
  return FALSE;
}
#endif
#endif

WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_frame_new(const char *label) {
  LiVESWidget *frame = NULL;
#ifdef GUI_GTK
  frame = gtk_frame_new(label);
#endif
#ifdef GUI_QT
  frame = new LiVESFrame(label);
#endif
  return frame;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_label(LiVESFrame *frame, const char *label) {
#ifdef GUI_GTK
  gtk_frame_set_label(frame, label);
  return TRUE;
#endif
#ifdef GUI_QT
  frame->set_label(QString::fromUtf8(label));
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_label_align(LiVESFrame *frame, float xalign, float yalign) {
#ifdef GUI_GTK
  gtk_frame_set_label_align(frame, xalign, yalign);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_label_widget(LiVESFrame *frame, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_frame_set_label_widget(frame, widget);
  return TRUE;
#endif
#ifdef GUI_QT
  frame->set_label_widget(widget);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_frame_get_label_widget(LiVESFrame *frame) {
  LiVESWidget *widget = NULL;
#ifdef GUI_GTK
  widget = gtk_frame_get_label_widget(frame);
#endif
#ifdef GUI_QT
  widget = frame->get_label_widget();
#endif
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_frame_set_shadow_type(LiVESFrame *frame, LiVESShadowType stype) {
#ifdef GUI_GTK
  gtk_frame_set_shadow_type(frame, stype);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_notebook_new(void) {
  LiVESWidget *nbook = NULL;
#ifdef GUI_GTK
  nbook = gtk_notebook_new();
#endif
#ifdef GUI_QT
  nbook = new LiVESNotebook;
#endif
  return nbook;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_notebook_get_nth_page(LiVESNotebook *nbook, int pagenum) {
  LiVESWidget *page = NULL;
#ifdef GUI_GTK
  page = gtk_notebook_get_nth_page(nbook, pagenum);
#endif
#ifdef GUI_QT
  QWidget *qwidget = nbook->widget(pagenum);
  QVariant qv = qwidget->property("LiVESWidgetObject");
  if (qv.isValid()) {
    page = static_cast<LiVESWidget *>(qv.value<LiVESWidgetObject *>());
  }
#endif
  return page;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_notebook_get_current_page(LiVESNotebook *nbook) {
  int pagenum = -1;
#ifdef GUI_GTK
  pagenum = gtk_notebook_get_current_page(nbook);
#endif
#ifdef GUI_QT
  pagenum = nbook->currentIndex();
#endif
  return pagenum;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_notebook_set_current_page(LiVESNotebook *nbook, int pagenum) {
#ifdef GUI_GTK
  gtk_notebook_set_current_page(nbook, pagenum);
  return TRUE;
#endif
#ifdef GUI_QT
  nbook->setCurrentIndex(pagenum);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_notebook_set_tab_label(LiVESNotebook *nbook, LiVESWidget *child,
    LiVESWidget *tablabel) {
#ifdef GUI_GTK
  gtk_notebook_set_tab_label(nbook, child, tablabel);
  return TRUE;
#endif
#ifdef GUI_QT
  nbook->set_tab_label(child, tablabel);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_table_new(uint32_t rows, uint32_t cols, boolean homogeneous) {
  LiVESWidget *table = NULL;
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  register int i;
  GtkGrid *grid = (GtkGrid *)lives_grid_new();
  gtk_grid_set_row_homogeneous(grid, homogeneous);
  gtk_grid_set_column_homogeneous(grid, homogeneous);

  for (i = 0; i < rows; i++) {
    lives_grid_insert_row(grid, 0);
  }

  for (i = 0; i < cols; i++) {
    gtk_grid_insert_column(grid, 0);
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(grid), ROWS_KEY, LIVES_INT_TO_POINTER(rows));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(grid), COLS_KEY, LIVES_INT_TO_POINTER(cols));
  table = (LiVESWidget *)grid;
#else
  table = gtk_table_new(rows, cols, homogeneous);
#endif
#endif
#ifdef GUI_QT
  table = new LiVESTable;
#endif
  return table;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_row_spacings(LiVESTable *table, uint32_t spacing) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  lives_grid_set_row_spacing(table, spacing);
#else
  gtk_table_set_row_spacings(table, spacing);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  table->setHorizontalSpacing(spacing);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_col_spacings(LiVESTable *table, uint32_t spacing) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  lives_grid_set_column_spacing(table, spacing);
#else
  gtk_table_set_col_spacings(table, spacing);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  table->setVerticalSpacing(spacing);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_row_homogeneous(LiVESTable *table, boolean homogeneous) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID
  gtk_grid_set_row_homogeneous(table, homogeneous);
  return TRUE;
#else
  gtk_table_set_homogeneous(table, homogeneous);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_set_column_homogeneous(LiVESTable *table, boolean homogeneous) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID
  gtk_grid_set_column_homogeneous(table, homogeneous);
  return TRUE;
#else
  gtk_table_set_homogeneous(table, homogeneous);
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_resize(LiVESTable *table, uint32_t rows, uint32_t cols) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  register int i;

  for (i = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(table), ROWS_KEY)); i < rows; i++) {
    lives_grid_insert_row(table, i);
  }

  for (i = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(table), COLS_KEY)); i < cols; i++) {
    gtk_grid_insert_column(table, i);
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(table), ROWS_KEY, LIVES_INT_TO_POINTER(rows));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(table), COLS_KEY, LIVES_INT_TO_POINTER(cols));
#else
  gtk_table_resize(table, rows, cols);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_table_attach(LiVESTable *table, LiVESWidget *child, uint32_t left, uint32_t right,
    uint32_t top, uint32_t bottom, LiVESAttachOptions xoptions, LiVESAttachOptions yoptions,
    uint32_t xpad, uint32_t ypad) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  gtk_grid_attach(table, child, left, top, right - left, bottom - top);
  if (xoptions & LIVES_EXPAND)
    lives_widget_set_hexpand(child, TRUE);
  else
    lives_widget_set_hexpand(child, FALSE);
  if (yoptions & LIVES_EXPAND)
    lives_widget_set_vexpand(child, TRUE);
  else
    lives_widget_set_vexpand(child, FALSE);

  lives_widget_set_margin_left(child, xpad);
  lives_widget_set_margin_right(child, xpad);

  lives_widget_set_margin_top(child, ypad);
  lives_widget_set_margin_bottom(child, ypad);
#else
  gtk_table_attach(table, child, left, right, top, bottom, xoptions, yoptions, xpad, ypad);
#endif
  return TRUE;
#endif
#ifdef GUI_QT

  table->addWidget(static_cast<QWidget *>(child), top, left, bottom - top, right - left);

  QSizePolicy policy;
  if (xoptions & LIVES_EXPAND) {
    policy.setHorizontalPolicy(QSizePolicy::Expanding);
  } else {
    policy.setHorizontalPolicy(QSizePolicy::Preferred);
  }
  if (yoptions & LIVES_EXPAND) {
    policy.setVerticalPolicy(QSizePolicy::Expanding);
  } else {
    policy.setVerticalPolicy(QSizePolicy::Preferred);
  }
  child->setSizePolicy(policy);

  QRect qr = child->geometry();
  qr.setWidth(qr.width() + xpad);
  qr.setHeight(qr.height() + ypad);
  child->setGeometry(qr);

  return TRUE;
#endif

  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_color_button_new_with_color(const LiVESWidgetColor *color) {
  LiVESWidget *cbutton = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  cbutton = gtk_color_button_new_with_rgba(color);
#else
  cbutton = gtk_color_button_new_with_color(color);
#endif
#endif
#ifdef GUI_QT
  cbutton = new LiVESColorButton(color);
#endif
  return cbutton;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_color_button_get_color(LiVESColorButton *button, LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_chooser_get_rgba((GtkColorChooser *)button, color);
#else
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_color_button_get_rgba((GtkColorChooser *)button, color);
#else
  gtk_color_button_get_color(button, color);
#endif
#endif
  return color;
#endif
#ifdef GUI_QT
  button->get_colour(color);
  return color;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_alpha(LiVESColorButton *button, int16_t alpha) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  LiVESWidgetColor color;
  gtk_color_chooser_get_rgba((GtkColorChooser *)button, &color);
  color.alpha = LIVES_WIDGET_COLOR_SCALE(alpha);
  gtk_color_chooser_set_rgba((GtkColorChooser *)button, &color);
#else
  gtk_color_button_set_alpha(button, alpha);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE int16_t lives_color_button_get_alpha(LiVESColorButton *button) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  LiVESWidgetColor color;
  gtk_color_chooser_get_rgba((GtkColorChooser *)button, &color);
  return LIVES_WIDGET_COLOR_STRETCH(color.alpha);
#else
  return gtk_color_button_get_alpha(button);
#endif
#endif
  return -1;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_color(LiVESColorButton *button, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_chooser_set_rgba((GtkColorChooser *)button, color);
#else
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_color_button_set_rgba((GtkColorChooser *)button, color);
#else
  gtk_color_button_set_color(button, color);
#endif
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_colour(color);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_title(LiVESColorButton *button, const char *title) {
#ifdef GUI_GTK
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  gtk_color_button_set_title(button, title);
  lives_free(ntitle);
  return TRUE;
#endif
#ifdef GUI_QT
  char *ntitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);
  button->set_title(title);
  lives_free(ntitle);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_color_button_set_use_alpha(LiVESColorButton *button, boolean use_alpha) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_color_chooser_set_use_alpha((GtkColorChooser *)button, use_alpha);
#else
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_color_button_set_use_alpha((GtkColorChooser *)button, use_alpha);
#else
  gtk_color_button_set_use_alpha(button, use_alpha);
#endif
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_use_alpha(use_alpha);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_LOCAL_INLINE boolean lives_widget_get_mods(LiVESXDevice *device, LiVESWidget *widget, int *x, int *y,
    LiVESXModifierType *modmask) {
#ifdef GUI_GTK
  LiVESXWindow *xwin;
  if (widget == NULL) xwin = gdk_get_default_root_window();
  else xwin = lives_widget_get_xwindow(widget);
  if (xwin == NULL) {
    LIVES_ERROR("Tried to get pointer for windowless widget");
    return TRUE;
  }
#if GTK_CHECK_VERSION(3, 0, 0)
  gdk_window_get_device_position(xwin, device, x, y, modmask);
#else
  gdk_window_get_pointer(xwin, x, y, modmask);
#endif
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_pointer(LiVESXDevice *device, LiVESWidget *widget, int *x, int *y) {
  return lives_widget_get_mods(device, widget, x, y, NULL);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_modmask(LiVESXDevice *device, LiVESWidget *widget,
    LiVESXModifierType *modmask) {
  return lives_widget_get_mods(device, widget, NULL, NULL, modmask);
}


static boolean lives_widget_destroyed(LiVESWidget *widget, void **ptr) {
  if (ptr) *ptr = NULL;
  return FALSE;
}


static boolean lives_widget_timetodie(LiVESWidget *widget, LiVESWidget *getoverhere) {
  if (LIVES_IS_WIDGET(getoverhere)) lives_widget_destroy(getoverhere);
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_nullify_with(LiVESWidget *widget, void **ptr) {
  lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_DESTROY_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_widget_destroyed),
                            ptr);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_destroy_with(LiVESWidget *widget, LiVESWidget *dieplease) {
  lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_DESTROY_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_widget_timetodie),
                            dieplease);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXDisplay *lives_widget_get_display(LiVESWidget *widget) {
  LiVESXDisplay *disp = NULL;
#ifdef GUI_GTK
  disp = gtk_widget_get_display(widget);
#endif
#ifdef GUI_QT
  QWidget *window = widget->window();
  window->winId();
  QWindow *qwindow = window->windowHandle();
  disp = qwindow->screen();
#endif
  return disp;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXWindow *lives_display_get_window_at_pointer
(LiVESXDevice *device, LiVESXDisplay *display, int *win_x, int *win_y) {
  LiVESXWindow *xwindow = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (device == NULL) return NULL;
  xwindow = gdk_device_get_window_at_position(device, win_x, win_y);
#else
  xwindow = gdk_display_get_window_at_pointer(display, win_x, win_y);
#endif
#endif
#ifdef GUI_QT
  QWidget *widget = QApplication::widgetAt(QCursor::pos(display));
  widget->winId();
  xwindow = widget->windowHandle();
#endif
  return xwindow;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_display_get_pointer
(LiVESXDevice *device, LiVESXDisplay *display, LiVESXScreen **screen, int *x, int *y, LiVESXModifierType *mask) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (device == NULL) return TRUE;
  gdk_device_get_position(device, screen, x, y);
#else
  gdk_display_get_pointer(display, screen, x, y, mask);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QPoint p = QCursor::pos(device);
  *x = p.x();
  *y = p.y();
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_display_warp_pointer
(LiVESXDevice *device, LiVESXDisplay *display, LiVESXScreen *screen, int x, int y) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (device == NULL) return TRUE;
  gdk_device_warp(device, screen, x, y);
#else
#if GLIB_CHECK_VERSION(2, 8, 0)
  gdk_display_warp_pointer(display, screen, x, y);
#endif
#endif
  return TRUE;
#endif
#ifdef GTK_QT
  QCursor::setPos(display, x, y);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_display_t lives_widget_get_display_type(LiVESWidget *widget) {
  lives_display_t dtype = LIVES_DISPLAY_TYPE_UNKNOWN;
#ifdef GUI_GTK
  if (GDK_IS_X11_DISPLAY(gtk_widget_get_display(widget))) dtype = LIVES_DISPLAY_TYPE_X11;
#ifdef GDK_WINDOWING_WAYLAND
  else if (GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(widget))) dtype = LIVES_DISPLAY_TYPE_WAYLAND;
#endif
  else if (GDK_IS_WIN32_DISPLAY(gtk_widget_get_display(widget))) dtype = LIVES_DISPLAY_TYPE_WIN32;
#endif
#ifdef GUI_QT
#ifdef Q_WS_X11
  dtype = LIVES_DISPLAY_TYPE_X11;
#endif
#ifdef Q_WS_WIN32
  dtype = LIVES_DISPLAY_TYPE_WIN32;
#endif
#endif
  return dtype;
}


WIDGET_HELPER_GLOBAL_INLINE uint64_t lives_widget_get_xwinid(LiVESWidget *widget, const char *msg) {
  uint64_t xwin = -1;
#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
  if (lives_widget_get_display_type(widget) == LIVES_DISPLAY_TYPE_X11)
    xwin = (uint64_t)GDK_WINDOW_XID(lives_widget_get_xwindow(widget));
  else
#endif
#ifdef GDK_WINDOWING_WIN32
    if (lives_widget_get_display_type(widget) == LIVES_DISPLAY_TYPE_WIN32)
      xwin = (uint64_t)gdk_win32_window_get_handle(lives_widget_get_xwindow(widget));
    else
#endif
#endif
#ifdef GUI_QT
      if (LIVES_IS_WINDOW(widget)) xwin = (uint64_t)widget->effectiveWinId();
      else
#endif
        if (msg != NULL) LIVES_WARN(msg);

  return xwin;
}


WIDGET_HELPER_GLOBAL_INLINE uint32_t lives_timer_add(uint32_t interval, LiVESWidgetSourceFunc function, livespointer data) {
  // interval in milliseconds
  lives_sigdata_t *sigdata = (lives_sigdata_t *)lives_calloc(1, sizeof(lives_sigdata_t));
  sigdata->callback = (lives_funcptr_t)function;
  sigdata->user_data = data;
  sigdata->is_timer = TRUE;

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  if (interval > 1000) {
    sigdata->funcid = g_timeout_add_seconds(interval / 1000., async_timer_handler, sigdata);
  } else {
    sigdata->funcid = g_timeout_add(interval, async_timer_handler, sigdata);
  }
#else
  sigdata->funcid = gtk_timeout_add(interval, async_timer_handler, sigdata);
#endif
#endif

  return sigdata->funcid;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_timer_remove(uint32_t timer) {
#ifdef GUI_GTK
  g_source_remove(timer);
  return TRUE;
#endif
#ifdef GUI_QT
  remove_static_timer(timer);
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE uint32_t lives_idle_add(LiVESWidgetSourceFunc function, livespointer data) {
  lives_sigdata_t *sigdata = (lives_sigdata_t *)lives_calloc(1, sizeof(lives_sigdata_t));
  sigdata->callback = (lives_funcptr_t)function;
  sigdata->user_data = data;
  sigdata->is_timer = TRUE;

  sigdata->funcid = g_idle_add(async_timer_handler, sigdata);
  return sigdata->funcid;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_source_remove(uint32_t handle) {
  return lives_timer_remove(handle);
}


WIDGET_HELPER_GLOBAL_INLINE uint32_t lives_accelerator_get_default_mod_mask(void) {
#ifdef GUI_GTK
  return gtk_accelerator_get_default_mod_mask();
#endif
#ifdef GUI_QT
  return 0;
#endif
}


WIDGET_HELPER_GLOBAL_INLINE int lives_screen_get_width(LiVESXScreen *screen) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 22, 0)
  return gdk_screen_get_width(screen);
#endif
#endif
#ifdef GUI_QT
  return screen->size().width();
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_screen_get_height(LiVESXScreen *screen) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 22, 0)
  return gdk_screen_get_height(screen);
#endif
#endif
#ifdef GUI_QT
  return screen->size().height();
#endif
  return 0;
}


WIDGET_HELPER_GLOBAL_INLINE boolean global_recent_manager_add(const char *full_file_name) {
#ifdef GUI_GTK
  char *tmp = g_filename_to_uri(full_file_name, NULL, NULL);
  gtk_recent_manager_add_item(gtk_recent_manager_get_default(), tmp);
  g_free(tmp);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESXCursor *lives_cursor_new_from_pixbuf(LiVESXDisplay *disp, LiVESPixbuf *pixbuf, int x, int y) {
  LiVESXCursor *cursor = NULL;
#ifdef GUI_GTK
  cursor = gdk_cursor_new_from_pixbuf(disp, pixbuf, x, y);
#endif
#ifdef GUI_QT
  QPixmap qpx;
  qpx.convertFromImage(*pixbuf);
  cursor = new QCursor(qpx, x, y);
#endif
  return cursor;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_has_toplevel_focus(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_window_has_toplevel_focus(LIVES_WINDOW(widget));
#endif
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_set_editable(LiVESEntry *entry, boolean editable) {
  return lives_editable_set_editable(LIVES_EDITABLE(entry), editable);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_entry_get_editable(LiVESEntry *entry) {
  return lives_editable_get_editable(LIVES_EDITABLE(entry));
}


// compound functions

WIDGET_HELPER_GLOBAL_INLINE boolean lives_image_scale(LiVESImage *image, int width, int height, LiVESInterpType interp_type) {
  LiVESPixbuf *pixbuf;
  if (!LIVES_IS_IMAGE(image)) return FALSE;
  pixbuf = lives_image_get_pixbuf(image);
  if (pixbuf != NULL) {
    LiVESPixbuf *new_pixbuf = lives_pixbuf_scale_simple(pixbuf, width, height, interp_type);
    lives_image_set_from_pixbuf(image, new_pixbuf);
    //if (LIVES_IS_WIDGET_OBJECT(pixbuf)) lives_widget_object_unref(pixbuf);
    if (new_pixbuf != NULL && LIVES_IS_WIDGET_OBJECT(new_pixbuf)) lives_widget_object_unref(new_pixbuf);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_label_set_hpadding(LiVESLabel *label, int pad) {
  const char *text = lives_label_get_text(label);
  lives_label_set_width_chars(label, strlen(text) + pad);
}


#define H_ALIGN_ADJ (22. * widget_opts.scale) // why 22 ? no idea

WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *align_horizontal_with(LiVESWidget *thingtoadd, LiVESWidget *thingtoalignwith) {
#ifdef GUI_GTK
  GtkWidget *fixed = gtk_fixed_new();
  int x = lives_widget_get_allocation_x(thingtoalignwith);
  // allow for 1 packing_width before adding the real widget
  gtk_fixed_put(GTK_FIXED(fixed), thingtoadd, x - H_ALIGN_ADJ - widget_opts.packing_width, 0);
  lives_widget_set_show_hide_parent(thingtoadd);
  return fixed;
#endif
  return NULL;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_pack_first(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill,
    uint32_t padding) {
  if (lives_box_pack_start(box, child, expand, fill, padding))
    return lives_box_reorder_child(box, child, 0);
  return FALSE;
}


void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  boolean mustfree = TRUE;
  char *text = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(source), TTIPS_KEY);
  if (text == NULL) text = gtk_widget_get_tooltip_text(source);
  else mustfree = FALSE;
  lives_widget_set_tooltip_text(dest, text);
  if (mustfree && text != NULL) lives_free(text);
#else
  GtkTooltipsData *td = gtk_tooltips_data_get(source);
  if (td == NULL) return;
  gtk_tooltips_set_tip(td->tooltips, dest, td->tip_text, td->tip_private);
#endif
#endif
#ifdef GUI_QT
  dest->setToolTip(source->toolTip());
#endif
}


boolean lives_combo_populate(LiVESCombo *combo, LiVESList *list) {
  LiVESList *revlist;

  // remove any current list
  if (!lives_combo_set_active_index(combo, -1)) return FALSE;
  if (!lives_combo_remove_all_text(combo)) return FALSE;

  if (lives_list_length(list) > COMBO_LIST_LIMIT) {
    // use a treestore
    LiVESTreeIter iter1, iter2;
    LiVESTreeStore *tstore = lives_tree_store_new(1, LIVES_COL_TYPE_STRING);
    char *cat;
    for (revlist = list; revlist != NULL; revlist = revlist->next) {
      cat = lives_strndup((const char *)revlist->data, 1);
      lives_tree_store_find_iter(tstore, 0, cat, NULL, &iter1);
      lives_tree_store_append(tstore, &iter2, &iter1);   /* Acquire an iterator */
      lives_tree_store_set(tstore, &iter2, 0, revlist->data, -1);
      lives_free(cat);
    }
    lives_combo_set_model(LIVES_COMBO(combo), LIVES_TREE_MODEL(tstore));
  } else {
    // reverse the list and then prepend the items
    // this is faster (O(1) than traversing the list and appending O(2))
    for (revlist = lives_list_last(list); revlist != NULL; revlist = revlist->prev) {
      if (!lives_combo_prepend_text(LIVES_COMBO(combo), (const char *)revlist->data)) return FALSE;
    }
  }

  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkCellArea *celly;
    lives_widget_object_get(LIVES_WIDGET_OBJECT(combo), "cell-area", &celly);
    gtk_cell_area_foreach(celly, setcellbg, NULL);

    // need to get the GtkCellView !
    lives_widget_object_set(celly, "background", &palette->info_base);
#endif
  }
  return TRUE;
}


///// lives compounds

LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment *adj, double volume) {
  LiVESWidget *volume_scale = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  volume_scale = gtk_volume_button_new();
  gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_scale), volume);
  lives_scale_button_set_orientation(LIVES_SCALE_BUTTON(volume_scale), orientation);
#else
  if (orientation == LIVES_ORIENTATION_HORIZONTAL)
    volume_scale = gtk_hscale_new(adj);
  else
    volume_scale = gtk_vscale_new(adj);

  gtk_scale_set_draw_value(GTK_SCALE(volume_scale), FALSE);
#endif
#endif
  return volume_scale;
}


static void default_changed_cb(LiVESWidgetObject *object, livespointer pspec, livespointer user_data) {
  // change the background colour of the button if it is the default
  boolean woat = widget_opts.apply_theme;
  LiVESWidget *button = (LiVESWidget *)object;
  LiVESWidgetState state = lives_widget_get_state(button);

#if GTK_CHECK_VERSION(3, 0, 0)
  // backdrop state removes the focus, so ignore it
  //if (state & LIVES_WIDGET_STATE_BACKDROP) return;
#endif

  widget_opts.apply_theme = TRUE;
  if (!lives_widget_is_sensitive(button)) set_child_dimmed_colour(button, BUTTON_DIM_VAL);
  else {
    if (!lives_widget_has_default(button) && (state & LIVES_WIDGET_STATE_PRELIGHT)) {
      // makes prelight work for non-def buttons
      widget_opts.apply_theme = woat;
      return;
    }
#if GTK_CHECK_VERSION(3, 0, 0)
    if (lives_widget_has_default(button)) {
      set_child_alt_colour(button, TRUE);
    } else set_child_colour(button, TRUE);
#else
    set_child_alt_colour(button, TRUE);
#endif
  }
  widget_opts.apply_theme = woat;
}


static void button_state_changed_cb(LiVESWidget *widget, LiVESWidgetState state, livespointer user_data) {
  // colour settings here are the exact opposite of what I would expect; however it seems to function in gtk+
  // I think we need to set the WRONG colour here, then correct it in default_changed_cb in order for gtk+
  // to update the background colour
  LiVESWidgetObject *toplevel;
  boolean woat = widget_opts.apply_theme;

#if GTK_CHECK_VERSION(3, 0, 0)
  // backdrop state removes the focus, so ignore it
  if (state & LIVES_WIDGET_STATE_BACKDROP) return;
#endif

  toplevel = LIVES_WIDGET_OBJECT(lives_widget_get_toplevel(widget));
  if (toplevel == NULL) return;

  widget_opts.apply_theme = TRUE;
  if (lives_widget_is_sensitive(widget) && (state & LIVES_WIDGET_STATE_FOCUSED)) {
    lives_widget_grab_default(widget);
    lives_widget_object_set_data(toplevel, CDEF_KEY, widget);
    if (!(state & (LIVES_WIDGET_STATE_PRELIGHT))) set_child_colour(widget, TRUE);
    else set_child_alt_colour(widget, TRUE);
  } else {
    set_child_alt_colour(widget, TRUE);
    if (lives_widget_object_get_data(toplevel, CDEF_KEY) == widget)
      lives_widget_object_set_data(toplevel, CDEF_KEY, NULL);
    if (lives_widget_object_get_data(toplevel, CDEF_KEY) == NULL) {
      // if no buttons are focused, default returns to default default
      LiVESWidget *deflt = lives_widget_object_get_data(toplevel, DEFBUTTON_KEY);
      if (deflt != NULL) {
        lives_widget_grab_default(deflt);
        set_child_colour(deflt, TRUE);
        default_changed_cb(LIVES_WIDGET_OBJECT(deflt), NULL, NULL);
      }
    }
  }

  if (!lives_widget_has_default(widget))
    default_changed_cb(LIVES_WIDGET_OBJECT(widget), NULL, NULL);
  widget_opts.apply_theme = woat;
}


boolean lives_button_grab_default_special(LiVESWidget *button) {
  // grab default and set as default default
  if (!lives_widget_set_can_focus_and_default(button)) return FALSE;
  if (!lives_widget_grab_default(button)) return FALSE;

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(lives_widget_get_toplevel(button)), DEFBUTTON_KEY, button);

  return TRUE;
}


///////////////// lives_layout ////////////////////////

WIDGET_HELPER_LOCAL_INLINE void lives_layout_attach(LiVESLayout *layout, LiVESWidget *widget, int start, int end, int row) {
  lives_table_attach(layout, widget, start, end, row, row + 1,
                     (LiVESAttachOptions)(LIVES_FILL | (LIVES_SHOULD_EXPAND_EXTRA_WIDTH ? LIVES_EXPAND : 0)),
                     (LiVESAttachOptions)(0), 0, 0);
}


static LiVESWidget *lives_layout_expansion_row_new(LiVESLayout *layout, LiVESWidget *widget) {
  LiVESList *xwidgets = (LiVESList *)lives_widget_object_steal_data(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY);
  LiVESWidget *box = lives_layout_row_new(layout);
  int columns = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), COLS_KEY));
  int rows = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY));
  lives_widget_object_ref(LIVES_WIDGET_OBJECT(box));
  lives_widget_unparent(box);
  lives_layout_attach(layout, box, 0, columns, rows - 1);
  lives_widget_object_unref(LIVES_WIDGET_OBJECT(box));
  if (widget != NULL) lives_layout_pack(LIVES_HBOX(box), widget);
  xwidgets = lives_list_prepend(xwidgets, box);
  lives_widget_object_set_data_list(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY, xwidgets);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(box), LROW_KEY, LIVES_INT_TO_POINTER(rows) - 1);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(box), EXPANSION_KEY, LIVES_INT_TO_POINTER(widget_opts.expand));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(box), JUST_KEY, LIVES_INT_TO_POINTER(widget_opts.justify));
  if (widget != NULL) return widget;
  return box;
}


static boolean lives_layout_resize(LiVESLayout *layout, int rows, int columns) {
  LiVESList *xwidgets = (LiVESList *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY);
  lives_table_resize(LIVES_TABLE(layout), rows, columns);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY, LIVES_INT_TO_POINTER(rows));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), COLS_KEY, LIVES_INT_TO_POINTER(columns));
  while (xwidgets != NULL) {
    LiVESWidget *widget = (LiVESWidget *)xwidgets->data;
    int row = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), LROW_KEY));
    int expansion = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), EXPANSION_KEY));
    LiVESJustification justification = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                                       JUST_KEY));
    int woe = widget_opts.expand;
    LiVESJustification woj = widget_opts.justify;
    lives_widget_object_ref(widget);
    lives_widget_unparent(widget);
    widget_opts.expand = expansion;
    widget_opts.justify = justification;
    lives_layout_attach(layout, widget, 0, columns, row);
    widget_opts.expand = woe;
    widget_opts.justify = woj;
    lives_widget_object_unref(widget);
    xwidgets = xwidgets->next;
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_pack(LiVESHBox *box, LiVESWidget *widget) {
  LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), LAYOUT_KEY);
  if (layout != NULL) {
    LiVESList *xwidgets = (LiVESList *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY);
    int row = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY)) - 1;
    // remove any expansion widgets on this row
    if (xwidgets != NULL) {
      if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xwidgets->data), LROW_KEY)) == row) {
        lives_widget_object_set_data_list(LIVES_WIDGET_OBJECT(layout), EXP_LIST_KEY, xwidgets);
      }
    }
  }
  lives_box_pack_start(LIVES_BOX(box), widget, TRUE, TRUE, LIVES_SHOULD_EXPAND_WIDTH ? widget_opts.packing_width >> 1 : 0);
  lives_widget_set_halign(widget, lives_justify_to_align(widget_opts.justify));
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_new(LiVESBox *box) {
  LiVESWidget *layout = lives_table_new(0, 0, FALSE);
  if (LIVES_IS_VBOX(box)) {
    lives_box_pack_start(box, layout, LIVES_SHOULD_EXPAND_EXTRA_HEIGHT, TRUE,
                         LIVES_SHOULD_EXPAND_HEIGHT ? widget_opts.packing_height : 0);
  } else {
    lives_box_pack_start(box, layout, LIVES_SHOULD_EXPAND_EXTRA_WIDTH, TRUE,
                         LIVES_SHOULD_EXPAND_WIDTH ? widget_opts.packing_width : 0);
  }
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY, LIVES_INT_TO_POINTER(1));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), COLS_KEY, LIVES_INT_TO_POINTER(0));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), WADDED_KEY, LIVES_INT_TO_POINTER(0));
  lives_table_set_col_spacings(LIVES_TABLE(layout), 0);
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_table_set_row_spacings(LIVES_TABLE(layout), widget_opts.packing_height);
  if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH)
    lives_table_set_col_spacings(LIVES_TABLE(layout), widget_opts.packing_width);
  //lives_table_set_column_homogeneous(LIVES_TABLE(layout), FALSE);
  return layout;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_hbox_new(LiVESLayout *layout) {
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
#if GTK_CHECK_VERSION(3, 0, 0)
  LiVESWidget *widget = hbox;
#else
  LiVESWidget *alignment = lives_alignment_new(widget_opts.justify == LIVES_JUSTIFY_CENTER ? 0.5 : widget_opts.justify ==
                           LIVES_JUSTIFY_RIGHT
                           ? 1. : 0., .5, 0., 0.);
  LiVESWidget *widget = alignment;
  lives_container_add(LIVES_CONTAINER(alignment), hbox);
#endif

  int nadded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), WADDED_KEY));
  int rows = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY));
  int columns = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), COLS_KEY));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, layout);
  if (++nadded > columns) lives_layout_resize(layout, rows, nadded);
  lives_layout_attach(layout, widget, nadded - 1, nadded, rows - 1);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), WADDED_KEY, LIVES_INT_TO_POINTER(nadded));
  return hbox;
}


WIDGET_HELPER_GLOBAL_INLINE int lives_layout_add_row(LiVESLayout *layout) {
  int rows = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY)) + 1;
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), ROWS_KEY, LIVES_INT_TO_POINTER(rows));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), WADDED_KEY, LIVES_INT_TO_POINTER(0));
  return rows;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_row_new(LiVESLayout *layout) {
  lives_layout_add_row(layout);
  return lives_layout_hbox_new(layout);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_add_label(LiVESLayout *layout, const char *text, boolean horizontal) {
  LiVESWidget *label = lives_label_new(text);
  if (horizontal) return lives_layout_pack(LIVES_HBOX(lives_layout_hbox_new(layout)), label);
  return lives_layout_expansion_row_new(layout, label);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_add_fill(LiVESLayout *layout, boolean horizontal) {
  return lives_layout_add_label(layout, NULL, horizontal);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_layout_add_separator(LiVESLayout *layout, boolean horizontal) {
  LiVESWidget *separator;
  LiVESJustification woj = widget_opts.justify;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  if (!horizontal) separator = lives_layout_pack(LIVES_HBOX(lives_layout_hbox_new(layout)), lives_vseparator_new());
  else separator = add_hsep_to_box(LIVES_BOX(lives_layout_expansion_row_new(layout, NULL)));
  widget_opts.justify = woj;
  return separator;
}

////////////////////////////////////////////////////////////////////

static void add_warn_image(LiVESWidget *widget, LiVESWidget *hbox) {
  LiVESWidget *warn_image = lives_image_new_from_stock(LIVES_STOCK_DIALOG_WARNING, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  lives_box_pack_start(LIVES_BOX(hbox), warn_image, FALSE, FALSE, 0);
  lives_widget_set_no_show_all(warn_image, TRUE);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(warn_image), TTIPS_OVERRIDE_KEY, warn_image);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), WARN_IMAGE_KEY, warn_image);
  lives_widget_set_sensitive_with(widget, warn_image);
}


WIDGET_HELPER_GLOBAL_INLINE boolean show_warn_image(LiVESWidget *widget, const char *text) {
  LiVESWidget *warn_image;
  if (!(warn_image = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), WARN_IMAGE_KEY))) return FALSE;
  if (text) {
    lives_widget_set_tooltip_text(warn_image, text);
    lives_widget_show(warn_image);
  } else lives_widget_hide(warn_image);
  return TRUE;
}


LiVESWidget *lives_standard_button_new(void) {
  return prettify_button(lives_button_new());
}


LiVESWidget *lives_standard_button_new_with_label(const char *label) {
  return prettify_button(lives_button_new_with_label(label));
}


LiVESWidget *lives_standard_button_new_from_stock(const char *stock_id, const char *label) {
  return prettify_button(lives_button_new_from_stock(stock_id, label));
}


LiVESWidget *lives_standard_menu_new(void) {
  LiVESWidget *menu = lives_menu_new();
  if (menu != NULL) {
    lives_widget_apply_theme2(menu, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menu, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menu, BUTTON_DIM_VAL);
  }
  return menu;
}


LiVESWidget *lives_standard_menu_item_new(void) {
  LiVESWidget *menuitem = lives_menu_item_new();
  if (menuitem != NULL) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
  }
  return menuitem;
}


LiVESWidget *lives_standard_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = lives_menu_item_new_with_label(label);
  if (menuitem != NULL) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
  }
  return menuitem;
}


LiVESWidget *lives_standard_image_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem = lives_image_menu_item_new_with_label(label);
  if (menuitem != NULL) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
  }
  return menuitem;
}



LiVESWidget *lives_standard_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group) {
  LiVESWidget *menuitem = lives_image_menu_item_new_from_stock(stock_id, accel_group);
  if (menuitem != NULL) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
  }
  return menuitem;
}


LiVESWidget *lives_standard_radio_menu_item_new_with_label(LiVESSList *group, const char *label) {
  LiVESWidget *menuitem = lives_radio_menu_item_new_with_label(group, label);
  if (menuitem != NULL) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
  }
  return menuitem;
}


LiVESWidget *lives_standard_check_menu_item_new_with_label(const char *label, boolean active) {
  LiVESWidget *menuitem = lives_check_menu_item_new_with_label(label);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(menuitem), active);
  if (menuitem != NULL) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    set_child_dimmed_colour2(menuitem, BUTTON_DIM_VAL);
  }
  return menuitem;
}


LiVESWidget *lives_standard_notebook_new(const LiVESWidgetColor *bg_color, const LiVESWidgetColor *act_color) {
  LiVESWidget *notebook = lives_notebook_new();

#if GTK_CHECK_VERSION(3, 0, 0)
  if (widget_opts.apply_theme) {
    char *colref = gdk_rgba_to_string(bg_color);
    // clear background image
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_NORMAL, "*", "background", "none");
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_NORMAL, "*", "background-color", colref);
    colref = gdk_rgba_to_string(act_color);
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_ACTIVE, "*", "background", "none");
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_ACTIVE, "*", "background-color", colref);
    lives_free(colref);
    set_css_value_direct(notebook, LIVES_WIDGET_STATE_ACTIVE, "*", "hexpand", "TRUE");
  }
#endif
  return notebook;
}


LiVESWidget *lives_standard_label_new(const char *text) {
  LiVESWidget *label = NULL;
  label = lives_label_new(text);
  lives_widget_set_halign(label, lives_justify_to_align(widget_opts.justify));
  if (widget_opts.apply_theme) {
    // non functional in gtk 3.18
    set_child_dimmed_colour(label, BUTTON_DIM_VAL);
    set_child_colour(label, TRUE);
    ///
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(label), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(label), NULL, NULL);
  }
  return label;
}


static LiVESWidget *make_inner_hbox(LiVESBox *box) {
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  LiVESWidget *vbox = lives_vbox_new(FALSE, 0);

  lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);
  if (LIVES_IS_HBOX(box)) {
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, LIVES_SHOULD_EXPAND_FOR(box) ? widget_opts.packing_width : 0);
    lives_widget_set_valign(hbox, LIVES_ALIGN_CENTER);
  } else {
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, LIVES_SHOULD_EXPAND_FOR(box) ? widget_opts.packing_height : 0);
    box = LIVES_BOX(hbox);
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, LIVES_SHOULD_EXPAND_FOR(box) ? widget_opts.packing_width : 0);
  }

  lives_box_pack_start(LIVES_BOX(hbox), vbox, FALSE, FALSE, 0);
  lives_widget_set_show_hide_parent(vbox);

  hbox = lives_hbox_new(FALSE, 0);
  if (!LIVES_SHOULD_EXPAND_EXTRA_FOR(vbox)) lives_widget_set_valign(hbox, LIVES_ALIGN_CENTER);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, LIVES_SHOULD_EXPAND_FOR(vbox) ? widget_opts.packing_height / 2 : 0);
  lives_widget_set_show_hide_parent(hbox);
  return hbox;
}


LiVESWidget *lives_standard_label_new_with_tooltips(const char *text, LiVESBox *box,
    const char *tips) {
  LiVESWidget *img_tips = make_ttips_image(tips);
  LiVESWidget *label = lives_standard_label_new(text);
  LiVESWidget *hbox = make_inner_hbox(LIVES_BOX(box));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, 0);
  lives_widget_set_show_hide_parent(label);
  lives_widget_set_sensitive_with(label, img_tips);
  lives_widget_set_show_hide_with(label, img_tips);
  if (prefs->show_tooltips) lives_widget_show(img_tips);
  return label;
}


LiVESWidget *lives_standard_drawing_area_new(LiVESGuiCallback callback, lives_painter_surface_t **ppsurf) {
  LiVESWidget *darea = NULL;
#ifdef GUI_GTK
  darea = gtk_drawing_area_new();
  lives_widget_set_app_paintable(darea, TRUE);
  if (ppsurf) {
    if (callback)
#if GTK_CHECK_VERSION(4, 0, 0)
      gtk_drawing_area_set_draw_func(darea, callback, (livespointer)surf, NULL);
#else
      lives_signal_sync_connect(LIVES_GUI_OBJECT(darea), LIVES_WIDGET_EXPOSE_EVENT,
                                LIVES_GUI_CALLBACK(callback),
                                (livespointer)ppsurf);
#endif
    lives_signal_sync_connect(LIVES_GUI_OBJECT(darea), LIVES_WIDGET_CONFIGURE_EVENT,
                              LIVES_GUI_CALLBACK(all_config),
                              (livespointer)ppsurf);
  }
  lives_widget_apply_theme(darea, LIVES_WIDGET_STATE_NORMAL);

#endif
  return darea;
}


LiVESWidget *lives_standard_label_new_with_mnemonic_widget(const char *text, LiVESWidget *mnemonic_widget) {
  LiVESWidget *label = NULL;

  label = lives_standard_label_new("");
  lives_label_set_text(LIVES_LABEL(label), text);

  if (mnemonic_widget != NULL) lives_label_set_mnemonic_widget(LIVES_LABEL(label), mnemonic_widget);

  return label;
}


LiVESWidget *lives_standard_frame_new(const char *labeltext, float xalign, boolean invis) {
  LiVESWidget *frame = lives_frame_new(NULL);
  LiVESWidget *label = NULL;
#if GTK_CHECK_VERSION(3, 24, 0)
  char *colref;
#endif

  if (LIVES_SHOULD_EXPAND)
    lives_container_set_border_width(LIVES_CONTAINER(frame), widget_opts.border_width);

  if (labeltext != NULL) {
    label = lives_standard_label_new(labeltext);
    lives_frame_set_label_widget(LIVES_FRAME(frame), label);
  }

  widget_opts.last_label = label;

  if (invis) lives_frame_set_shadow_type(LIVES_FRAME(frame), LIVES_SHADOW_NONE);

  lives_widget_apply_theme(frame, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_set_text_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

#if GTK_CHECK_VERSION(3, 24, 0)
  colref = gdk_rgba_to_string(&palette->menu_and_bars);
  set_css_value_direct(frame, LIVES_WIDGET_STATE_NORMAL, "border", "border-color", colref);
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
#endif

  if (xalign >= 0.) lives_frame_set_label_align(LIVES_FRAME(frame), xalign, 0.5);

  return frame;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESAlign lives_justify_to_align(LiVESJustification justify) {
  if (justify == LIVES_JUSTIFY_DEFAULT) return LIVES_ALIGN_START;
  if (justify == LIVES_JUSTIFY_CENTER) return LIVES_ALIGN_CENTER;
  else return LIVES_ALIGN_END;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESScrollDirection lives_get_scroll_direction(LiVESXEventScroll *event) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 4, 0)
  double dx, dy;
  if (gdk_event_get_scroll_deltas(LIVES_XEVENT(event), &dx, &dy)) {
    if (dy < 0.) return LIVES_SCROLL_UP;
    if (dy > 0.) return LIVES_SCROLL_DOWN;
  }
#endif
#if GTK_CHECK_VERSION(3, 2, 0)
  LiVESScrollDirection direction;
  gdk_event_get_scroll_direction(LIVES_XEVENT(event), &direction);
  return direction;
#else
  return event->direction;
#endif
#endif
  return LIVES_SCROLL_UP;
}


static LiVESWidget *make_label_eventbox(const char *labeltext, LiVESWidget *widget) {
  LiVESWidget *label;
  LiVESWidget *eventbox = lives_event_box_new();
  lives_tooltips_copy(eventbox, widget);

  if (widget_opts.mnemonic_label && labeltext != NULL) {
    label = lives_standard_label_new_with_mnemonic_widget(labeltext, widget);
  } else label = lives_standard_label_new(labeltext);

  widget_opts.last_label = label;
  lives_container_add(LIVES_CONTAINER(eventbox), label);
  lives_widget_set_halign(label, lives_justify_to_align(widget_opts.justify));

  if (LIVES_IS_TOGGLE_BUTTON(widget)) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(label_act_toggle),
                              widget);
  }
  lives_widget_set_sensitive_with(widget, eventbox);
  lives_widget_set_sensitive_with(eventbox, label);
  lives_widget_set_show_hide_with(widget, eventbox);
  lives_widget_set_show_hide_with(eventbox, label);
  if (widget_opts.apply_theme) {
    // default themeing
    lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_INSENSITIVE);
    //lives_widget_apply_theme(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(label), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
  }
  return eventbox;
}


static void sens_insens_cb(LiVESWidgetObject *object, livespointer pspec, livespointer user_data) {
  LiVESWidget *widget = (LiVESWidget *)object;
  LiVESWidget *other = (LiVESWidget *)user_data;
  boolean sensitive = lives_widget_get_sensitive(widget);
  if (lives_widget_get_sensitive(other) != sensitive) {
    lives_widget_set_sensitive(other, sensitive);
  }
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_sensitive_with(LiVESWidget *w1, LiVESWidget *w2) {
  // set w2 sensitivity == w1
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(w1), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                  LIVES_GUI_CALLBACK(sens_insens_cb),
                                  (livespointer)w2);
  return TRUE;
}


static void lives_widget_show_all_cb(LiVESWidget *widget, livespointer user_data) {
  LiVESWidget *parent = (LiVESWidget *)user_data;
  if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_HIDE_KEY)) {
    if (prefs->show_tooltips) lives_widget_show(widget);
    return;
  }
  if (!lives_widget_is_visible(parent))
    lives_widget_show_all(parent);
}


static void lives_widget_hide_cb(LiVESWidget *widget, livespointer user_data) {
  LiVESWidget *parent = (LiVESWidget *)user_data;
  lives_widget_hide(parent);
}


boolean lives_widget_set_show_hide_with(LiVESWidget *widget, LiVESWidget *other) {
  // show / hide the other widget when and only when the child is shown / hidden
  if (widget == NULL || other == NULL) return FALSE;
  if (!widget_opts.no_gui) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_SHOW_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_widget_show_all_cb),
                              (livespointer)(other));

    lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_HIDE_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_widget_hide_cb),
                              (livespointer)(other));
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_show_hide_parent(LiVESWidget *widget) {
  LiVESWidget *parent = lives_widget_get_parent(widget);
  if (parent != NULL) return lives_widget_set_show_hide_with(widget, parent);
  return FALSE;
}


LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean active, LiVESBox *box,
    const char *tooltip) {
  LiVESWidget *checkbutton = NULL;

  // pack a themed check button into box

  // seems like it is not possible to theme the checkbox

  LiVESWidget *eventbox = NULL;
  LiVESWidget *hbox;
  LiVESWidget *img_tips = NULL;

  boolean expand;

  widget_opts.last_label = NULL;

#if LIVES_HAS_SWITCH_WIDGET
  // TODO: need to intercept TOGGLED handler, and replace with "notify:active"
  if (prefs->cb_is_switch) checkbutton = gtk_switch_new();
  else
#endif
    checkbutton = lives_check_button_new();

  if (tooltip) img_tips = lives_widget_set_tooltip_text(checkbutton, tooltip);

  if (box != NULL) {
    int packing_width = 0;

    if (labeltext != NULL) {
      eventbox = make_label_eventbox(labeltext, checkbutton);
    }

    if (LIVES_IS_HBOX(box) && !LIVES_SHOULD_EXPAND_WIDTH) hbox = LIVES_WIDGET(box);
    else {
      hbox = make_inner_hbox(LIVES_BOX(box));
      lives_widget_set_show_hide_parent(checkbutton);
    }

    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);
    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.swap_label && eventbox != NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), checkbutton, expand, expand,
                         eventbox == NULL ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (!widget_opts.swap_label && eventbox != NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    add_warn_image(checkbutton, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, 0);
      if (prefs->show_tooltips) lives_widget_show(img_tips);
      lives_widget_set_show_hide_with(checkbutton, img_tips);
      lives_widget_set_sensitive_with(checkbutton, img_tips);
    }
  }

  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_apply_theme(checkbutton, LIVES_WIDGET_STATE_NORMAL);
#endif
  }

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), active);

  return checkbutton;
}


LiVESWidget *lives_glowing_check_button_new(const char *labeltext, LiVESBox *box, const char *tooltip, boolean *togglevalue) {
  boolean active = FALSE;
  LiVESWidget *checkbutton;
  if (togglevalue != NULL) active = *togglevalue;

  checkbutton = lives_standard_check_button_new(labeltext, active, box, tooltip);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(lives_cool_toggled),
                                  togglevalue);

  if (prefs->lamp_buttons) {
    lives_toggle_button_set_mode(LIVES_TOGGLE_BUTTON(checkbutton), FALSE);
    if (widget_opts.apply_theme) {
      lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
      lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
      lives_cool_toggled(checkbutton, togglevalue);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                                LIVES_GUI_CALLBACK(draw_cool_toggle),
                                NULL);
    }
  }
  return checkbutton;
}


LiVESWidget *lives_glowing_tool_button_new(const char *labeltext, LiVESToolbar *tbar, const char *tooltip,
    boolean *togglevalue) {
  LiVESToolItem *titem = lives_tool_item_new();
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;
  LiVESWidget *button = lives_glowing_check_button_new(labeltext, LIVES_BOX(hbox), tooltip, togglevalue);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_container_add(LIVES_CONTAINER(titem), hbox);
  if (tbar != NULL) lives_toolbar_insert(tbar, titem, -1);
  return button;
}


LiVESToolItem *lives_standard_menu_tool_button_new(LiVESWidget *icon, const char *label) {
  LiVESToolItem *toolitem = NULL;
#ifdef GUI_GTK
  toolitem = lives_menu_tool_button_new(icon, label);
  if (widget_opts.apply_theme) {
    lives_widget_set_bg_color(LIVES_WIDGET(toolitem), LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    LiVESList *children = lives_container_get_children(LIVES_CONTAINER(toolitem)), *list = children;
    while (list != NULL) {
      LiVESWidget *widget = (LiVESWidget *)list->data;
      if (LIVES_IS_VBOX(widget)) {
        LiVESList *children2 = lives_container_get_children(LIVES_CONTAINER(toolitem)), *list2 = children2;
        lives_container_set_border_width(LIVES_CONTAINER(widget), 0);
        while (list2 != NULL) {
          LiVESWidget *child = (LiVESWidget *)list2->data;
          if (LIVES_IS_CONTAINER(child)) lives_container_set_border_width(LIVES_CONTAINER(child), 0);
          lives_widget_set_valign(child, LIVES_ALIGN_FILL);
          lives_widget_set_halign(child, LIVES_ALIGN_FILL);
          lives_widget_set_margin_left(child, 0.);
          lives_widget_set_margin_right(child, 0.);
          lives_widget_set_margin_top(child, 0.);
          lives_widget_set_margin_bottom(child, 0.);
          list2 = list2->next;
        }
        lives_widget_set_bg_color(widget, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
        list2 = list2->next;
        lives_list_free(children2);
      }
      list = list->next;
    }
    lives_list_free(children);
  }
#endif
#ifdef GUI_QT
  toolitem = new LiVESMenuToolButton(QString::fromUtf8(label), LIVES_MAIN_WINDOW_WIDGET, icon);
#endif
  return toolitem;
}


LiVESWidget *lives_standard_radio_button_new(const char *labeltext, LiVESSList **rbgroup, LiVESBox *box, const char *tooltip) {
  LiVESWidget *radiobutton = NULL;

  // pack a themed check button into box

  LiVESWidget *eventbox = NULL;
  LiVESWidget *img_tips = NULL;
  LiVESWidget *hbox;

  boolean expand;

  widget_opts.last_label = NULL;

  radiobutton = lives_radio_button_new(*rbgroup);

  *rbgroup = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton));

  if (tooltip) img_tips = lives_widget_set_tooltip_text(radiobutton, tooltip);

  if (box != NULL) {
    int packing_width = 0;

    if (labeltext != NULL) {
      eventbox = make_label_eventbox(labeltext, radiobutton);
    }

    hbox = make_inner_hbox(LIVES_BOX(box));
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.swap_label && eventbox != NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, TRUE, FALSE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), radiobutton, expand, FALSE,
                         eventbox == NULL ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (!widget_opts.swap_label && eventbox != NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    lives_widget_set_show_hide_parent(radiobutton);

    add_warn_image(radiobutton, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, 0);
      if (prefs->show_tooltips) lives_widget_show(img_tips);
      lives_widget_set_show_hide_with(radiobutton, img_tips);
      lives_widget_set_sensitive_with(radiobutton, img_tips);
    }
  }

  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_apply_theme(radiobutton, LIVES_WIDGET_STATE_NORMAL);
#endif
  }

  return radiobutton;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_step_increment(LiVESSpinButton *button, double step_increment) {
#ifdef GUI_GTK
  LiVESAdjustment *adj = lives_spin_button_get_adjustment(button);
  lives_adjustment_set_step_increment(adj, step_increment);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_spin_button_set_snap_to_multiples(LiVESSpinButton *button, double mult) {
#ifdef GUI_GTK
  lives_spin_button_set_step_increment(button, mult);
  lives_spin_button_set_snap_to_ticks(button, TRUE);
  return TRUE;
#endif
  return FALSE;
}


size_t calc_spin_button_width(double min, double max, int dp) {
  char *txt = lives_strdup_printf("%d", (int)max);
  size_t maxlen = strlen(txt);
  lives_free(txt);
  txt = lives_strdup_printf("%d", (int)min);
  if (strlen(txt) > maxlen) maxlen = strlen(txt);
  lives_free(txt);
  if (dp > 0) maxlen += dp + 1;
  if (maxlen < MIN_SPINBUTTON_SIZE) return MIN_SPINBUTTON_SIZE;
  return maxlen;
}


LiVESWidget *lives_standard_spin_button_new(const char *labeltext, double val, double min,
    double max, double step, double page, int dp, LiVESBox *box,
    const char *tooltip) {
  // pack a themed spin button into box
  LiVESWidget *spinbutton = NULL;

  LiVESWidget *eventbox = NULL;
  LiVESWidget *hbox;
  LiVESAdjustment *adj;

  boolean expand;

  int maxlen;

  widget_opts.last_label = NULL;

  adj = lives_adjustment_new(val, min, max, step, page, 0.);
  spinbutton = lives_spin_button_new(adj, 1, dp);

  val = lives_spin_button_get_snapval(LIVES_SPIN_BUTTON(spinbutton), val);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(spinbutton), val);
  lives_spin_button_update(LIVES_SPIN_BUTTON(spinbutton));
  set_standard_widget(spinbutton, TRUE);

  if (tooltip != NULL) lives_widget_set_tooltip_text(spinbutton, tooltip);

  maxlen = calc_spin_button_width(min, max, dp);
  lives_entry_set_width_chars(LIVES_ENTRY(spinbutton), maxlen);

  lives_entry_set_activates_default(LIVES_ENTRY(spinbutton), TRUE);
  lives_entry_set_has_frame(LIVES_ENTRY(spinbutton), TRUE);
  lives_entry_set_alignment(LIVES_ENTRY(spinbutton), 0.2);
#ifdef GUI_GTK
  //gtk_spin_button_set_update_policy(LIVES_SPIN_BUTTON(spinbutton), GTK_UPDATE_ALWAYS);
  gtk_spin_button_set_numeric(LIVES_SPIN_BUTTON(spinbutton), TRUE);
#endif

  if (box != NULL) {
    LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), LAYOUT_KEY);
    int packing_width = 0;

    if (labeltext != NULL) {
      eventbox = make_label_eventbox(labeltext, spinbutton);
    }

    hbox = make_inner_hbox(LIVES_BOX(box));
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (!widget_opts.swap_label && eventbox != NULL) {
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
      if (layout != NULL) {
        lives_widget_set_show_hide_with(spinbutton, hbox);
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box));
      }
    }

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), spinbutton, expand, TRUE, eventbox == NULL ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label && eventbox != NULL) {
      if (layout != NULL) {
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box));
        lives_widget_set_show_hide_with(spinbutton, hbox);
      }
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
    }
    lives_widget_set_show_hide_parent(spinbutton);
  }

  if (widget_opts.apply_theme) {
    set_child_alt_colour(spinbutton, TRUE);
    set_child_dimmed_colour2(spinbutton, BUTTON_DIM_VAL); // insens, themecols 1, child only
    lives_widget_apply_theme2(LIVES_WIDGET(spinbutton), LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed2(spinbutton, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
  }

  return spinbutton;
}


LiVESWidget *lives_standard_combo_new(const char *labeltext, LiVESList *list, LiVESBox *box, const char *tooltip) {
  LiVESWidget *combo = NULL;

  // pack a themed combo box into box

  // seems like it is not possible to set the arrow colours
  // nor the entireity of the background for the popup list

  LiVESWidget *eventbox = NULL;
  LiVESWidget *hbox;
  LiVESWidget *img_tips = NULL;
  LiVESEntry *entry;

  boolean expand;

  widget_opts.last_label = NULL;

  combo = lives_combo_new();

  if (tooltip != NULL) img_tips = lives_widget_set_tooltip_text(combo, tooltip);

  entry = (LiVESEntry *)lives_combo_get_entry(LIVES_COMBO(combo));

  lives_entry_set_has_frame(entry, TRUE);
  lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);
  lives_entry_set_activates_default(entry, TRUE);
  lives_entry_set_width_chars(LIVES_ENTRY(entry), SHORT_ENTRY_WIDTH);

  lives_widget_set_sensitive_with(LIVES_WIDGET(entry), combo);
  lives_widget_set_sensitive_with(combo, LIVES_WIDGET(entry));

  lives_widget_set_can_focus(LIVES_WIDGET(entry), FALSE);

  lives_combo_set_focus_on_click(LIVES_COMBO(combo), FALSE);

  lives_widget_add_events(LIVES_WIDGET(entry), LIVES_BUTTON_RELEASE_MASK);
  lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                                    LIVES_GUI_CALLBACK(lives_combo_popup),
                                    combo);

  if (box != NULL) {
    LiVESWidget *layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(box), LAYOUT_KEY);
    int packing_width = 0;

    if (labeltext != NULL) {
      eventbox = make_label_eventbox(labeltext, LIVES_WIDGET(entry));
    }

    hbox = make_inner_hbox(LIVES_BOX(box));
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (!widget_opts.swap_label && eventbox != NULL) {
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
      if (layout != NULL) {
        lives_widget_set_show_hide_with(combo, hbox);
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box));
      }
    }

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_widget_set_hexpand(combo, FALSE);
    lives_widget_set_valign(combo, LIVES_ALIGN_CENTER);
    lives_box_pack_start(LIVES_BOX(hbox), combo, LIVES_SHOULD_EXPAND_WIDTH,
                         expand, eventbox == NULL ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label && eventbox != NULL) {
      if (layout != NULL) {
        box = LIVES_BOX(lives_layout_hbox_new(LIVES_TABLE(layout)));
        hbox = make_inner_hbox(LIVES_BOX(box));
        lives_widget_set_show_hide_with(combo, hbox);
      }
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
    }
    lives_widget_set_show_hide_parent(combo);

    add_warn_image(combo, hbox);

    if (img_tips) {
      lives_box_pack_start(LIVES_BOX(hbox), img_tips, FALSE, FALSE, 0);
      if (prefs->show_tooltips) lives_widget_show(img_tips);
      lives_widget_set_show_hide_with(combo, img_tips);
      lives_widget_set_sensitive_with(combo, img_tips);
    }
  }

  if (list != NULL) {
    lives_combo_populate(LIVES_COMBO(combo), list);
    lives_combo_set_active_index(LIVES_COMBO(combo), 0);
  }

  if (widget_opts.apply_theme) {
    set_child_colour(combo, TRUE);
    set_child_dimmed_colour(combo, BUTTON_DIM_VAL); // insens, themecols 1, child only
    lives_widget_apply_theme2(combo, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme2(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed(combo, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    lives_widget_apply_theme_dimmed(LIVES_WIDGET(entry), LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(entry), NULL, NULL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(combo), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(combo), NULL, NULL);
  }

  return combo;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_combo_new_with_model(LiVESTreeModel *model, LiVESBox *box) {
  LiVESWidget *combo = lives_standard_combo_new(NULL, NULL, box, NULL);
  lives_combo_set_model(LIVES_COMBO(combo), model);

  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkCellArea *celly;
    lives_widget_object_get(LIVES_WIDGET_OBJECT(combo), "cell-area", &celly);
    gtk_cell_area_foreach(celly, setcellbg, NULL);

    // need to get the GtkCellView !
    //lives_widget_object_set(celly, "background", &palette->info_base);

#endif
  }

  return combo;
}


LiVESWidget *lives_standard_entry_new(const char *labeltext, const char *txt, int dispwidth, int maxchars,
                                      LiVESBox *box, const char *tooltip) {
  LiVESWidget *entry = NULL;
  LiVESWidget *hbox = NULL;
  LiVESWidget *eventbox = NULL;

  boolean expand;

  widget_opts.last_label = NULL;

  entry = lives_entry_new();
  lives_widget_set_valign(entry, LIVES_ALIGN_CENTER);

  lives_widget_set_font_size(entry, LIVES_WIDGET_STATE_NORMAL, widget_opts.font_size);

  if (tooltip != NULL) lives_widget_set_tooltip_text(entry, tooltip);

  if (txt != NULL)
    lives_entry_set_text(LIVES_ENTRY(entry), txt);
  //pango_parse_markup(xyz, -1, NULL, atrlist_pp, NULL, NULL, NULL);
  // pango_layout_set_attributes(
  //pango_layout_set_markup(gtk_entry_get_layout(entry), "<span foreground=\"red\">OKOK</span>", -1);

  if (dispwidth != -1) lives_entry_set_width_chars(LIVES_ENTRY(entry), dispwidth);
  else {
    if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH) lives_entry_set_width_chars(LIVES_ENTRY(entry), MEDIUM_ENTRY_WIDTH);
    else if (LIVES_SHOULD_EXPAND_WIDTH) lives_entry_set_width_chars(LIVES_ENTRY(entry), MEDIUM_ENTRY_WIDTH);
  }

  if (maxchars != -1) lives_entry_set_max_length(LIVES_ENTRY(entry), maxchars);

  lives_entry_set_activates_default(LIVES_ENTRY(entry), TRUE);
  lives_entry_set_has_frame(LIVES_ENTRY(entry), TRUE);

  //lives_widget_set_halign(entry, LIVES_ALIGN_START);  // NO ! - causes entry to shrink
  if (widget_opts.justify == LIVES_JUSTIFY_LEFT) {
    lives_entry_set_alignment(LIVES_ENTRY(entry), 0.);
  }
  if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
    lives_entry_set_alignment(LIVES_ENTRY(entry), 0.5);
  }
  if (widget_opts.justify == LIVES_JUSTIFY_RIGHT) {
    lives_entry_set_alignment(LIVES_ENTRY(entry), 1.);
  }

  if (box != NULL) {
    int packing_width = 0;

    if (labeltext != NULL) {
      eventbox = make_label_eventbox(labeltext, entry);
    }

    hbox = make_inner_hbox(LIVES_BOX(box));
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (!widget_opts.swap_label && eventbox != NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), entry, LIVES_SHOULD_EXPAND_WIDTH, dispwidth == -1, eventbox == NULL ? packing_width : 0);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label && eventbox != NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, packing_width);
    lives_widget_set_show_hide_parent(entry);
  }

  if (widget_opts.apply_theme) {
#if GTK_CHECK_VERSION(3, 0, 0)
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_NOTIFY_SIGNAL "editable",
                                    LIVES_GUI_CALLBACK(edit_state_cb),
                                    NULL);
    lives_widget_apply_theme_dimmed(entry, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
#else
    lives_widget_apply_theme2(entry, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme_dimmed(entry, LIVES_WIDGET_STATE_INSENSITIVE, BUTTON_DIM_VAL);
    /* lives_widget_set_base_color(entry, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back); */
    /* lives_widget_set_text_color(entry, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore); */
    /* lives_widget_set_base_color(entry, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back); */
#endif
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(entry), NULL, NULL);
  }
  return entry;
}


LiVESWidget *lives_dialog_add_button_from_stock(LiVESDialog *dialog, const char *stock_id, const char *label, int response_id) {
  int bwidth = LIVES_SHOULD_EXPAND_EXTRA_WIDTH ? DEF_BUTTON_WIDTH * 4 : DEF_BUTTON_WIDTH * 2;
  LiVESWidget *button = lives_button_new_from_stock(stock_id, label);
  LiVESWidget *first_button;

  if (dialog != NULL) lives_dialog_add_action_widget(dialog, button, response_id);
  lives_widget_set_size_request(button, bwidth, -1);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), NWIDTH_KEY, LIVES_INT_TO_POINTER(bwidth));
  if (widget_opts.apply_theme) {
    set_standard_widget(button, TRUE);
#if !GTK_CHECK_VERSION(3, 0, 0)
    set_child_alt_colour(button, TRUE);
#endif
    lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_NOTIFY_SIGNAL "has_default",
                              LIVES_GUI_CALLBACK(default_changed_cb),
                              NULL);
    default_changed_cb(LIVES_WIDGET_OBJECT(button), NULL, NULL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(button), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(button_state_changed_cb),
                                    NULL);
    button_state_changed_cb(button, lives_widget_get_state(button), NULL);
  }
  if (dialog != NULL) {
    if ((first_button = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(dialog), FBUTT_KEY)) == NULL) {
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dialog), FBUTT_KEY, (livespointer)button);
      lives_button_center(button);
    } else {
      lives_button_uncenter(first_button, LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(first_button),
                            NWIDTH_KEY)));
    }
  }
  return button;
}


WIDGET_HELPER_LOCAL_INLINE void dlg_focus_changed(LiVESContainer *c, LiVESWidget *widget, livespointer user_data) {
#if GTK_CHECK_VERSION(2, 18, 0)
  LiVESWidget *entry = NULL;
  while (LIVES_IS_CONTAINER(widget)) {
    LiVESWidget *fchild = lives_container_get_focus_child(LIVES_CONTAINER(widget));
    if (fchild == NULL || fchild == widget) break;
    widget = fchild;
  }

  if (LIVES_IS_COMBO(widget)) {
    entry = lives_combo_get_entry(LIVES_COMBO(widget));
  } else entry = widget;

  if (entry != NULL && LIVES_IS_ENTRY(entry)) {
    if (lives_entry_get_activates_default(LIVES_ENTRY(widget))) {
      LiVESWidget *toplevel = lives_widget_get_toplevel(widget);
      LiVESWidget *button;
      if (!LIVES_IS_WIDGET(toplevel)) return;
      button = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(toplevel), DEFBUTTON_KEY);
      if (button != NULL && lives_widget_is_sensitive(button)) {
        boolean woat = widget_opts.apply_theme;
        widget_opts.apply_theme = TRUE;
        // default button gets the default
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(toplevel), CDEF_KEY, NULL);
        lives_widget_grab_default(button);
        set_child_alt_colour(button, TRUE);
        widget_opts.apply_theme = woat;
      }
    }
  }
#endif
}


LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons, int width, int height) {
  // in case of problems, try setting widget_opts.no_gui=TRUE

  LiVESWidget *dialog = NULL;

  dialog = lives_dialog_new();

  if (width <= 0) width = 8;
  if (height <= 0) height = 8;

  if (!widget_opts.no_gui) {
    if (widget_opts.transient == NULL)
      lives_window_set_transient_for(LIVES_WINDOW(dialog), get_transient_full());
    else
      lives_window_set_transient_for(LIVES_WINDOW(dialog), widget_opts.transient);
  }

#if !GTK_CHECK_VERSION(3, 0, 0)
  if (height > 8 && width > 8) {
#endif
    lives_widget_set_minimum_size(dialog, width, height);
#if !GTK_CHECK_VERSION(3, 0, 0)
  }
#endif

  if (widget_opts.screen != NULL) lives_window_set_screen(LIVES_WINDOW(dialog), widget_opts.screen);

  if (title != NULL)
    lives_window_set_title(LIVES_WINDOW(dialog), title);

  lives_window_set_deletable(LIVES_WINDOW(dialog), FALSE);

  if (!widget_opts.non_modal)
    lives_window_set_resizable(LIVES_WINDOW(dialog), FALSE);

  lives_widget_set_hexpand(dialog, TRUE);
  lives_widget_set_vexpand(dialog, TRUE);

#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_dialog_set_has_separator(LIVES_DIALOG(dialog), FALSE);
#endif

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(dialog, LIVES_WIDGET_STATE_NORMAL);
    funkify_dialog(dialog);
#if GTK_CHECK_VERSION(2, 18, 0)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(lives_dialog_get_content_area(LIVES_DIALOG(dialog))),
                              LIVES_WIDGET_SET_FOCUS_CHILD_SIGNAL,
                              LIVES_GUI_CALLBACK(dlg_focus_changed),
                              NULL);
#endif
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width * 2);
  }

  // do this before widget_show(), then call lives_window_center() afterwards
  lives_window_set_position(LIVES_WINDOW(dialog), LIVES_WIN_POS_CENTER_ALWAYS);

  if (add_std_buttons) {
    // cancel button will automatically destroy the dialog
    // ok button needs manual destruction

    LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    LiVESWidget *cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                                LIVES_RESPONSE_CANCEL);

    LiVESWidget *okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
                            LIVES_RESPONSE_OK);

    lives_button_grab_default_special(okbutton);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked),
                              NULL);

    lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

    if (widget_opts.apply_theme) {
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                      LIVES_GUI_CALLBACK(widget_state_cb),
                                      NULL);
      widget_state_cb(LIVES_WIDGET_OBJECT(cancelbutton), NULL, NULL);

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                      LIVES_GUI_CALLBACK(widget_state_cb),
                                      NULL);
      widget_state_cb(LIVES_WIDGET_OBJECT(okbutton), NULL, NULL);
    }

    lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(dialog), LIVES_WIDGET_DELETE_EVENT,
                            LIVES_GUI_CALLBACK(return_true),
                            NULL);

  if (!widget_opts.non_modal)
    lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);

  return dialog;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_font_chooser_new(void) {
  LiVESWidget *font_choo = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 2, 0)
  char *ttl;
  font_choo = gtk_font_button_new();
  gtk_font_button_set_show_size(GTK_FONT_BUTTON(font_choo), FALSE);
  gtk_font_chooser_set_show_preview_entry(GTK_FONT_CHOOSER(font_choo), TRUE);
  gtk_font_chooser_set_preview_text(GTK_FONT_CHOOSER(font_choo), "LiVES");
  ttl = lives_strdup_printf("%s%s", widget_opts.title_prefix, _("Choose a Font..."));
  gtk_font_button_set_title(GTK_FONT_BUTTON(font_choo), ttl);
  lives_free(ttl);
#endif
#endif
  return font_choo;
}


extern void on_filesel_button_clicked(LiVESButton *, livespointer);

static LiVESWidget *lives_standard_dfentry_new(const char *labeltext, const char *txt, const char *defdir, int dispwidth,
    int maxchars,
    LiVESBox *box, const char *tooltip, boolean isdir) {
  LiVESWidget *direntry = NULL;
  LiVESWidget *buttond;

  if (box == NULL) return NULL;

  direntry = lives_standard_entry_new(labeltext, txt, dispwidth, maxchars == -1 ? PATH_MAX : maxchars, box, tooltip);
  lives_entry_set_editable(LIVES_ENTRY(direntry), FALSE);

  // add dir, with filechooser button
  buttond = lives_standard_file_button_new(isdir, defdir);
  if (widget_opts.last_label != NULL) lives_label_set_mnemonic_widget(LIVES_LABEL(widget_opts.last_label), buttond);
  lives_box_pack_start(LIVES_BOX(lives_widget_get_parent(direntry)), buttond, FALSE, FALSE, widget_opts.packing_width);

  lives_signal_sync_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            (livespointer)direntry);
  lives_widget_set_sensitive_with(buttond, direntry);
  lives_widget_set_show_hide_with(buttond, direntry);
  return direntry;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_direntry_new(const char *labeltext, const char *txt, int dispwidth,
    int maxchars,
    LiVESBox *box, const char *tooltip) {
  return lives_standard_dfentry_new(labeltext, txt, txt, dispwidth, maxchars, box, tooltip, TRUE);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_fileentry_new(const char *labeltext, const char *txt,
    const char *defdir,
    int dispwidth, int maxchars, LiVESBox *box, const char *tooltip) {
  return lives_standard_dfentry_new(labeltext, txt, defdir, dispwidth, maxchars, box, tooltip, FALSE);
}


LiVESWidget *lives_standard_hscale_new(LiVESAdjustment *adj) {
  LiVESWidget *hscale = NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hscale = gtk_scale_new(LIVES_ORIENTATION_HORIZONTAL, adj);
#if 0
  /// css inline example
  if (GTK_IS_RANGE(hscale)) {
    GtkCssProvider *provider;
    GtkStyleContext *ctx;
    char *wname = make_random_string(RND_STR_PREFIX);
    char *tmp = lives_strdup_printf("#%s * {min-width: 10px;		\
    min-height: 10px;							\
    margin: -1px;							\
    margin-top: 2px;							\
    margin-bottom: 2px;							\
    border: 1px solid @border;						\
    border-radius: 8px;							\
    background-clip: padding-box;					\
    background-color: transparent;					\
    background-image: linear-gradient(to right,				\
                                      shade(#FFFFFF, 1.12),	\
                                      shade(#FFFFFF, 0.95));    \
    box-shadow: 1px 0 alpha(white, 0.5);}\n", wname);

    gtk_widget_set_name(hscale, wname);
    ctx = gtk_widget_get_style_context(hscale);
    provider = gtk_css_provider_new();
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER
                                   (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                    tmp,
                                    -1, NULL);

    lives_free(wname);
    lives_free(tmp);
    lives_widget_object_unref(provider);
  }
#endif

#else
  hscale = gtk_hscale_new(adj);
#endif
  gtk_scale_set_draw_value(LIVES_SCALE(hscale), FALSE);
#endif
#ifdef GUI_QT
  hscale = new LiVESScale(LIVES_ORIENTATION_HORIZONTAL, adj);
#endif
  return hscale;
}


LiVESWidget *lives_standard_hruler_new(void) {
  LiVESWidget *hruler = NULL;

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  hruler = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, NULL);
  gtk_scale_set_draw_value(GTK_SCALE(hruler), FALSE);
#if GTK_CHECK_VERSION(3, 4, 0)
  gtk_scale_set_has_origin(GTK_SCALE(hruler), FALSE);
#endif
  gtk_scale_set_digits(GTK_SCALE(hruler), 8);
#else
  hruler = gtk_hruler_new();
  lives_widget_apply_theme(hruler, LIVES_WIDGET_STATE_INSENSITIVE);
#endif

#endif

  return hruler;
}


LiVESWidget *lives_standard_scrolled_window_new(int width, int height, LiVESWidget *child) {
  LiVESWidget *scrolledwindow = NULL;
  LiVESWidget *swchild;

  scrolledwindow = lives_scrolled_window_new(NULL, NULL);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(scrolledwindow), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);

  if (LIVES_SHOULD_EXPAND_WIDTH)
    lives_widget_set_hexpand(scrolledwindow, TRUE);
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_vexpand(scrolledwindow, TRUE);

  lives_container_set_border_width(LIVES_CONTAINER(scrolledwindow), widget_opts.border_width);

  if (child != NULL) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
    if (!LIVES_IS_SCROLLABLE(child))
#else
    if (!LIVES_IS_TEXT_VIEW(child))
#endif
    {
      lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), child);
    } else {
      if (!LIVES_SHOULD_EXPAND_EXTRA_WIDTH) {
        LiVESWidget *align;
        align = lives_alignment_new(.5, 0., 0., 0.);
        lives_container_add(LIVES_CONTAINER(align), child);
        lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), align);
      } else {
        lives_container_add(LIVES_CONTAINER(scrolledwindow), child);
      }
    }
#endif
#ifdef GUI_QT
    lives_container_add(scrolledwindow, child);
#endif
  }

  swchild = lives_bin_get_child(LIVES_BIN(scrolledwindow));

#ifdef GUI_QT
  if (width > -1 || height > -1)
    lives_widget_set_minimum_size(scrolledwindow, width, height);
#endif

  lives_widget_apply_theme(swchild, LIVES_WIDGET_STATE_NORMAL);

  if (LIVES_SHOULD_EXPAND_WIDTH) {
    lives_widget_set_halign(swchild, LIVES_ALIGN_FILL);
    lives_widget_set_hexpand(swchild, TRUE);
  }
  if (LIVES_SHOULD_EXPAND_HEIGHT)
    lives_widget_set_vexpand(swchild, TRUE);

  if (LIVES_IS_CONTAINER(child) && LIVES_SHOULD_EXPAND) lives_container_set_border_width(LIVES_CONTAINER(child),
        widget_opts.border_width >> 1);

#ifdef GUI_GTK
  if (GTK_IS_VIEWPORT(swchild))
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(swchild), LIVES_SHADOW_IN);

  if (width != 0 && height != 0) {
#if !GTK_CHECK_VERSION(3, 0, 0)
    if (width > -1 || height > -1)
      lives_widget_set_size_request(scrolledwindow, width, height);
    lives_widget_set_minimum_size(scrolledwindow, width, height); // crash if we dont have toplevel win
#else
    if (height != -1) lives_scrolled_window_set_min_content_height(LIVES_SCROLLED_WINDOW(scrolledwindow), height);
    if (width != -1) lives_scrolled_window_set_min_content_width(LIVES_SCROLLED_WINDOW(scrolledwindow), width);
#endif
  }
#endif

  return scrolledwindow;
}


LiVESWidget *lives_standard_expander_new(const char *ltext, LiVESBox *box, LiVESWidget *child) {
  LiVESWidget *expander = NULL;

#ifdef GUI_GTK
  LiVESWidget *hbox;
  char *labeltext;

  if (LIVES_SHOULD_EXPAND) {
    labeltext = lives_strdup_printf("<big>%s</big>", ltext);
  } else labeltext = lives_strdup(ltext);

  expander = lives_expander_new(labeltext);
  lives_free(labeltext);

  lives_expander_set_use_markup(LIVES_EXPANDER(expander), TRUE);

  if (box != NULL) {
    int packing_width = 0;

    if (LIVES_IS_HBOX(box)) hbox = LIVES_WIDGET(box);
    else {
      hbox = make_inner_hbox(LIVES_BOX(box));
      lives_widget_set_show_hide_parent(expander);
    }

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width;

    if (widget_opts.justify == LIVES_JUSTIFY_CENTER || widget_opts.justify == LIVES_JUSTIFY_LEFT) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.justify == LIVES_JUSTIFY_LEFT) lives_widget_set_halign(expander, LIVES_ALIGN_START);
    if (widget_opts.justify != LIVES_JUSTIFY_RIGHT) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.justify == LIVES_JUSTIFY_CENTER) lives_widget_set_halign(expander, LIVES_ALIGN_CENTER);
    lives_box_pack_start(LIVES_BOX(hbox), expander, TRUE, TRUE, packing_width);

    if (widget_opts.justify == LIVES_JUSTIFY_RIGHT) lives_widget_set_halign(expander, LIVES_ALIGN_END);
    if (widget_opts.justify != LIVES_JUSTIFY_LEFT) add_fill_to_box(LIVES_BOX(hbox));

    if (child != NULL) lives_container_add(LIVES_CONTAINER(expander), child);
    lives_container_set_border_width(LIVES_CONTAINER(expander), widget_opts.border_width);
  }

  if (widget_opts.apply_theme) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(expander), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(expander), NULL, NULL);


    if (widget_opts.last_label != NULL) {
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(widget_opts.last_label), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                      LIVES_GUI_CALLBACK(widget_state_cb),
                                      NULL);
      widget_state_cb(LIVES_WIDGET_OBJECT(widget_opts.last_label), NULL, NULL);
    }


    widget_opts.last_label = lives_expander_get_label_widget(LIVES_EXPANDER(expander));
    lives_widget_apply_theme(expander, LIVES_WIDGET_STATE_NORMAL);
#ifdef GUI_GTK
    lives_container_forall(LIVES_CONTAINER(expander), set_child_colour_internal, LIVES_INT_TO_POINTER(TRUE));
#endif
  }
#endif

  return expander;
}


LiVESWidget *lives_standard_table_new(uint32_t rows, uint32_t cols, boolean homogeneous) {
  LiVESWidget *table = lives_table_new(rows, cols, homogeneous);
  lives_widget_apply_theme(table, LIVES_WIDGET_STATE_NORMAL);
  if (LIVES_SHOULD_EXPAND_WIDTH) lives_table_set_row_spacings(LIVES_TABLE(table),
        LIVES_SHOULD_EXPAND_EXTRA_WIDTH ? (widget_opts.packing_width << 2) : widget_opts.packing_width);
  else lives_table_set_row_spacings(LIVES_TABLE(table), 0);
  if (LIVES_SHOULD_EXPAND_HEIGHT) lives_table_set_col_spacings(LIVES_TABLE(table),
        LIVES_SHOULD_EXPAND_EXTRA_HEIGHT ? (widget_opts.packing_height << 2) : widget_opts.packing_height);
  else lives_table_set_col_spacings(LIVES_TABLE(table), 0);
  return table;
}


LiVESWidget *lives_standard_text_view_new(const char *text, LiVESTextBuffer *tbuff) {
  LiVESWidget *textview;

  if (tbuff == NULL)
    textview = lives_text_view_new();
  else
    textview = lives_text_view_new_with_buffer(tbuff);

  lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), FALSE);
  lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(textview), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textview), FALSE);

  if (text != NULL) {
    lives_text_view_set_text(LIVES_TEXT_VIEW(textview), text, -1);
  }

  lives_widget_apply_theme3(textview, LIVES_WIDGET_STATE_NORMAL);

  lives_text_view_set_justification(LIVES_TEXT_VIEW(textview), widget_opts.justify);
  if (widget_opts.justify == LIVES_JUSTIFY_CENTER) {
    lives_widget_set_halign(textview, LIVES_ALIGN_CENTER);
    lives_widget_set_valign(textview, LIVES_ALIGN_CENTER);
  }
  return textview;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir) {
  LiVESWidget *fbutton = fbutton = lives_standard_button_new();
  LiVESWidget *image = lives_image_new_from_stock(LIVES_STOCK_OPEN, LIVES_ICON_SIZE_BUTTON);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(fbutton), ISDIR_KEY, LIVES_INT_TO_POINTER(is_dir));
  if (def_dir != NULL) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(fbutton), DEFDIR_KEY, (livespointer)def_dir);
  lives_container_add(LIVES_CONTAINER(fbutton), image);
  return fbutton;
}


static void lives_widget_destroy_if_image(LiVESWidget *widget, livespointer user_data) {
  if (LIVES_IS_IMAGE(widget)) {
    lives_widget_destroy(widget);
  }
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_lock_button_get_locked(LiVESButton *button) {
  return (boolean)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), ISLOCKED_KEY));
}


static void _on_lock_button_clicked(LiVESButton *button, livespointer user_data) {
  LiVESWidget *image;
  boolean apply_theme = (boolean)LIVES_POINTER_TO_INT(user_data);

  int locked = !(LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), ISLOCKED_KEY)));
  int width = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), WIDTH_KEY));
  int height = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), HEIGHT_KEY));

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), ISLOCKED_KEY, LIVES_INT_TO_POINTER(locked));
  lives_container_foreach(LIVES_CONTAINER(button), (LiVESWidgetCallback)lives_widget_destroy_if_image, NULL);
  if (locked) image = lives_image_new_from_stock_at_size(LIVES_LIVES_STOCK_LOCKED, LIVES_ICON_SIZE_CUSTOM,
                        LOCK_BUTTON_WIDTH,
                        LOCK_BUTTON_HEIGHT);
  else image = lives_image_new_from_stock_at_size(LIVES_LIVES_STOCK_UNLOCKED, LIVES_ICON_SIZE_CUSTOM, LOCK_BUTTON_WIDTH,
                 LOCK_BUTTON_HEIGHT);
  if (!LIVES_IS_IMAGE(image)) return;
  lives_image_scale(LIVES_IMAGE(image), width, height, LIVES_INTERP_BEST);
  lives_container_add(LIVES_CONTAINER(button), image);
  lives_widget_show(image);
  if (apply_theme) {
    boolean woat = widget_opts.apply_theme;
    widget_opts.apply_theme = TRUE;
    lives_widget_apply_theme(image, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(LIVES_WIDGET(button), LIVES_WIDGET_STATE_NORMAL);
    widget_opts.apply_theme = woat;
  }
}


boolean label_act_lockbutton(LiVESWidget *widget, LiVESXEventButton *event, LiVESButton *lockbutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(lockbutton))) return FALSE;
  _on_lock_button_clicked(lockbutton, NULL);
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_lock_button_new(boolean is_locked, int width, int height,
    const char *tooltip) {
  LiVESWidget *lockbutton;
  lockbutton = lives_button_new();
  prettify_button(lockbutton);
  lives_widget_set_size_request(lockbutton, width, height);
  if (tooltip != NULL) lives_widget_set_tooltip_text(lockbutton, tooltip);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(lockbutton), ISLOCKED_KEY, LIVES_INT_TO_POINTER(!is_locked));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(lockbutton), WIDTH_KEY, LIVES_INT_TO_POINTER(width));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(lockbutton), HEIGHT_KEY, LIVES_INT_TO_POINTER(height));
  lives_signal_sync_connect(lockbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(_on_lock_button_clicked), NULL);
  _on_lock_button_clicked(LIVES_BUTTON(lockbutton), LIVES_INT_TO_POINTER(widget_opts.apply_theme));
  return lockbutton;
}


static void on_pwcolselx(LiVESButton *button, lives_rfx_t *rfx) {
  LiVESWidgetColor selected;

  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPRED_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPGREEN_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPBLUE_KEY);
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), SPALPHA_KEY);

  int r, g, b, a;

  lives_color_button_get_color(LIVES_COLOR_BUTTON(button), &selected);

  // get 0. -> 255. values
  if (sp_red != NULL) {
    r = (int)((double)(selected.red + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_red), (double)r);
  }

  if (sp_green != NULL) {
    g = (int)((double)(selected.green + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_green), (double)g);
  }

  if (sp_blue != NULL) {
    b = (int)((double)(selected.blue + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_blue), (double)b);
  }

  if (sp_alpha != NULL) {
#if !LIVES_WIDGET_COLOR_HAS_ALPHA
    a = lives_color_button_get_alpha(LIVES_COLOR_BUTTON(button)) / 255.;
#else
    a = (int)((double)(selected.alpha + LIVES_WIDGET_COLOR_SCALE_255(0.5)) / (double)LIVES_WIDGET_COLOR_SCALE_255(1.));
#endif
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(sp_alpha), (double)a);
  }

  lives_color_button_set_color(LIVES_COLOR_BUTTON(button), &selected);
}


static void after_param_red_changedx(LiVESSpinButton *spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY);
#endif

  int new_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_green));
  int old_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_blue));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(new_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(old_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(old_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (sp_alpha != NULL) {
    int old_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_alpha));
    colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(old_alpha);
  } else colr.alpha = 1.0;
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


static void after_param_green_changedx(LiVESSpinButton *spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY);
#endif

  int new_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_red));
  int old_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_blue));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(old_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(new_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(old_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (sp_alpha != NULL) {
    int old_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_alpha));
    colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(old_alpha);
  } else colr.alpha = 1.0;
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


static void after_param_blue_changedx(LiVESSpinButton *spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY);
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  LiVESWidget *sp_alpha = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY);
#endif

  int new_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_green));
  int old_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_red));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(old_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(old_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(new_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (sp_alpha != NULL) {
    int old_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_alpha));
    colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(old_alpha);
  } else colr.alpha = 1.0;
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


static void after_param_alpha_changedx(LiVESSpinButton *spinbutton, livespointer udata) {
  LiVESWidgetColor colr;

  LiVESWidget *cbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton), CBUTTON_KEY);
  LiVESWidget *sp_green = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY);
  LiVESWidget *sp_blue = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY);
  LiVESWidget *sp_red = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY);

  int new_alpha = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
  int old_red = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_red));
  int old_green = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_green));
  int old_blue = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(sp_blue));

  colr.red = LIVES_WIDGET_COLOR_SCALE_255(old_red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_255(old_green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_255(old_blue);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  colr.alpha = LIVES_WIDGET_COLOR_SCALE_255(new_alpha);
#else
  lives_color_button_set_alpha(LIVES_COLOR_BUTTON(cbutton), LIVES_WIDGET_COLOR_SCALE_255(new_alpha));
#endif
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
}


LiVESWidget *lives_standard_color_button_new(LiVESBox *box, const char *name, boolean use_alpha, lives_colRGBA64_t *rgba,
    LiVESWidget **sb_red, LiVESWidget **sb_green, LiVESWidget **sb_blue, LiVESWidget **sb_alpha) {
  LiVESWidgetColor colr;
  LiVESWidget *cbutton, *labelcname = NULL;
  LiVESWidget *hbox = NULL;
  LiVESWidget *layout;
  LiVESWidget *frame = lives_standard_frame_new(NULL, 0., FALSE);
  LiVESWidget *spinbutton_red = NULL, *spinbutton_green = NULL, *spinbutton_blue = NULL, *spinbutton_alpha = NULL;
  LiVESWidget *parent = NULL;
  char *tmp, *tmp2;

  int packing_width = 0;

  boolean parent_is_layout = FALSE;
  boolean expand = FALSE;

  widget_opts.last_label = NULL;

  lives_container_set_border_width(LIVES_CONTAINER(frame), 0);

  if (box != NULL) {
    parent = lives_widget_get_parent(LIVES_WIDGET(box));
    if (parent != NULL && LIVES_IS_TABLE(parent) &&
        lives_widget_object_get_data(LIVES_WIDGET_OBJECT(parent), WADDED_KEY) != NULL) {
      parent_is_layout = TRUE;
      lives_table_set_column_homogeneous(LIVES_TABLE(parent), FALSE);
      hbox = LIVES_WIDGET(box);
    } else {
      hbox = LIVES_WIDGET(make_inner_hbox(LIVES_BOX(box)));
    }
    expand = LIVES_SHOULD_EXPAND_EXTRA_FOR(hbox);

    if (LIVES_SHOULD_EXPAND_WIDTH) packing_width = widget_opts.packing_width >> 1;
  }

  colr.red = LIVES_WIDGET_COLOR_SCALE_65535(rgba->red);
  colr.green = LIVES_WIDGET_COLOR_SCALE_65535(rgba->green);
  colr.blue = LIVES_WIDGET_COLOR_SCALE_65535(rgba->blue);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  if (use_alpha) colr.alpha = LIVES_WIDGET_COLOR_SCALE_65535(rgba->alpha);
  else colr.alpha = 1.;
#endif

  cbutton = lives_color_button_new_with_color(&colr);

  lives_color_button_set_use_alpha(LIVES_COLOR_BUTTON(cbutton), use_alpha);
  lives_color_button_set_color(LIVES_COLOR_BUTTON(cbutton), &colr);
  lives_widget_apply_theme(cbutton, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme2(cbutton, LIVES_WIDGET_STATE_PRELIGHT, TRUE);
  lives_widget_set_border_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

#if !LIVES_WIDGET_COLOR_HAS_ALPHA
  if (use_alpha)
    lives_color_button_set_alpha(LIVES_COLOR_BUTTON(cbutton), rgba->alpha);
#endif

  if (name != NULL && box != NULL) {
    // must do this before re-using translation string !
    if (widget_opts.mnemonic_label) {
      labelcname = lives_standard_label_new_with_mnemonic_widget(name, cbutton);
    } else labelcname = lives_standard_label_new(name);
    lives_widget_set_show_hide_with(cbutton, labelcname);
    lives_widget_set_sensitive_with(cbutton, labelcname);
  }

  lives_widget_set_tooltip_text(cbutton, (_("Click to set the colour")));
  lives_color_button_set_title(LIVES_COLOR_BUTTON(cbutton), _("Select Colour"));

  if (box != NULL) {
    if (!widget_opts.swap_label) {
      if (labelcname != NULL) {
        if (LIVES_SHOULD_EXPAND_WIDTH) lives_widget_set_margin_left(labelcname, widget_opts.packing_width >> 2);
        lives_box_pack_start(LIVES_BOX(hbox), labelcname, FALSE, FALSE, widget_opts.packing_width);
        if (parent_is_layout) {
          hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
          widget_opts.justify = LIVES_JUSTIFY_RIGHT;
        }
      }
    }

    if (sb_red != NULL) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, NULL);
      spinbutton_red = lives_standard_spin_button_new((tmp = lives_strdup(_("_Red"))), rgba->red / 255., 0., 255., 1., 1., 0,
                       (LiVESBox *)hbox, (tmp2 = lives_strdup(_("The red value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_red), CBUTTON_KEY, cbutton);
      *sb_red = spinbutton_red;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_red), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_red_changedx),
                                NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_red);
      lives_widget_set_show_hide_with(cbutton, spinbutton_red);
    }

    if (sb_green != NULL) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, NULL);
      spinbutton_green = lives_standard_spin_button_new((tmp = lives_strdup(_("_Green"))), rgba->green / 255., 0., 255., 1., 1., 0,
                         (LiVESBox *)hbox, (tmp2 = lives_strdup(_("The green value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_green), CBUTTON_KEY, cbutton);
      *sb_green = spinbutton_green;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_green), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_green_changedx),
                                NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_green);
      lives_widget_set_show_hide_with(cbutton, spinbutton_green);
    }

    if (sb_blue != NULL) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, NULL);
      spinbutton_blue = lives_standard_spin_button_new((tmp = lives_strdup(_("_Blue"))), rgba->blue / 255., 0., 255., 1., 1., 0,
                        (LiVESBox *)hbox, (tmp2 = lives_strdup(_("The blue value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_blue), CBUTTON_KEY, cbutton);
      *sb_blue = spinbutton_blue;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_blue), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_blue_changedx),
                                NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_blue);
      lives_widget_set_show_hide_with(cbutton, spinbutton_blue);
    }

    if (use_alpha && sb_alpha != NULL) {
      layout = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, NULL);
      spinbutton_alpha = lives_standard_spin_button_new((tmp = lives_strdup(_("_Alpha"))), rgba->alpha / 255., 0., 255., 1., 1., 0,
                         (LiVESBox *)hbox, (tmp2 = lives_strdup(_("The alpha value (0 - 255)"))));
      lives_free(tmp);
      lives_free(tmp2);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), LAYOUT_KEY, layout);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(spinbutton_alpha), CBUTTON_KEY, cbutton);
      *sb_alpha = spinbutton_alpha;
      lives_signal_sync_connect(LIVES_GUI_OBJECT(spinbutton_alpha), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                LIVES_GUI_CALLBACK(after_param_alpha_changedx),
                                NULL);
      if (parent_is_layout) {
        hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
      } else if (expand) add_fill_to_box(LIVES_BOX(hbox));
      lives_widget_set_sensitive_with(cbutton, spinbutton_alpha);
      lives_widget_set_show_hide_with(cbutton, spinbutton_alpha);
    }

    if (parent_is_layout) {
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
      hbox = make_inner_hbox(LIVES_BOX(hbox));
    }

    lives_container_add(LIVES_CONTAINER(frame), cbutton);
    lives_box_pack_start(LIVES_BOX(hbox), frame, TRUE, FALSE, packing_width * 2.);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPRED_KEY, spinbutton_red);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPGREEN_KEY, spinbutton_green);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPBLUE_KEY, spinbutton_blue);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cbutton), SPALPHA_KEY, spinbutton_alpha);

    lives_widget_set_show_hide_parent(cbutton);

    if (widget_opts.swap_label) {
      if (labelcname != NULL) {
        if (parent_is_layout) {
          hbox = lives_layout_hbox_new(LIVES_TABLE(parent));
          widget_opts.justify = LIVES_JUSTIFY_LEFT;
        }
        if (LIVES_SHOULD_EXPAND_WIDTH) lives_widget_set_margin_right(labelcname, widget_opts.packing_width >> 2);
        lives_box_pack_start(LIVES_BOX(hbox), labelcname, FALSE, FALSE, widget_opts.packing_width);
      }
    }
  }

  if (parent_is_layout) {
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cbutton), LIVES_WIDGET_COLOR_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(on_pwcolselx),
                            NULL);

  widget_opts.last_label = labelcname;
  return cbutton;
}


// utils

boolean widget_helper_init(void) {
#ifdef GUI_GTK
  GSList *flist, *slist;
  LiVESList *dlist, *xlist = NULL;
  register int i;
#endif

#if GTK_CHECK_VERSION(3, 10, 0) || defined GUI_QT
  lives_snprintf(LIVES_STOCK_LABEL_CANCEL, 32, "%s", (_("_Cancel")));
  lives_snprintf(LIVES_STOCK_LABEL_OK, 32, "%s", (_("_OK")));
  lives_snprintf(LIVES_STOCK_LABEL_YES, 32, "%s", (_("_Yes")));
  lives_snprintf(LIVES_STOCK_LABEL_NO, 32, "%s", (_("_No")));
  lives_snprintf(LIVES_STOCK_LABEL_SAVE, 32, "%s", (_("_Save")));
  lives_snprintf(LIVES_STOCK_LABEL_SAVE_AS, 32, "%s", (_("Save _As")));
  lives_snprintf(LIVES_STOCK_LABEL_OPEN, 32, "%s", (_("_Open")));
  lives_snprintf(LIVES_STOCK_LABEL_QUIT, 32, "%s", (_("_Quit")));
  lives_snprintf(LIVES_STOCK_LABEL_APPLY, 32, "%s", (_("_Apply")));
  lives_snprintf(LIVES_STOCK_LABEL_CLOSE, 32, "%s", (_("_Close")));
  lives_snprintf(LIVES_STOCK_LABEL_REVERT, 32, "%s", (_("_Revert")));
  lives_snprintf(LIVES_STOCK_LABEL_REFRESH, 32, "%s", (_("_Refresh")));
  lives_snprintf(LIVES_STOCK_LABEL_DELETE, 32, "%s", (_("_Delete")));
  lives_snprintf(LIVES_STOCK_LABEL_GO_FORWARD, 32, "%s", (_("_Forward")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_FORWARD, 32, "%s", (_("R_ewind")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_REWIND, 32, "%s", (_("_Forward")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_PLAY, 32, "%s", (_("_Play")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_PAUSE, 32, "%s", (_("P_ause")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_STOP, 32, "%s", (_("_Stop")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_RECORD, 32, "%s", (_("_Record")));
  lives_snprintf(LIVES_STOCK_LABEL_SELECT_ALL, 32, "%s", (_("_Select All")));
#endif

  widget_opts = def_widget_opts;

#ifdef GUI_GTK
  gtk_accel_map_add_entry("<LiVES>/save", LIVES_KEY_s, LIVES_CONTROL_MASK);
  gtk_accel_map_add_entry("<LiVES>/quit", LIVES_KEY_q, LIVES_CONTROL_MASK);

  slist = flist = gdk_pixbuf_get_formats();
  while (slist != NULL) {
    GdkPixbufFormat *form = (GdkPixbufFormat *)slist->data;
    char **ext = gdk_pixbuf_format_get_extensions(form);
    for (i = 0; ext[i] != NULL; i++) {
      xlist = lives_list_append_unique(xlist, lives_strdup(ext[i]));
    }
    lives_strfreev(ext);
    slist = slist->next;
  }
  g_slist_free(flist);
#endif

  if (xlist != NULL) {
    dlist = xlist;
    widget_opts.image_filter = (char **)lives_malloc((lives_list_length(xlist) + 1) * sizeof(char *));
    for (i = 0; dlist != NULL; i++) {
      widget_opts.image_filter[i] = lives_strdup_printf("*.%s", (char *)dlist->data);
      dlist = dlist->next;
    }
    widget_opts.image_filter[i] = NULL;
    lives_list_free_all(&xlist);
  }

#ifdef GUI_GTK
  // I think this is correct...
  if (gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL) widget_opts.default_justify = LIVES_JUSTIFY_RIGHT;
#endif

  widget_opts.justify = widget_opts.default_justify;
  return TRUE;
}


boolean widget_opts_rescale(double scale) {
  widget_opts.scale = scale;
  widget_opts.border_width = (float)def_widget_opts.border_width * widget_opts.scale;
  widget_opts.packing_width = (float)def_widget_opts.packing_width * widget_opts.scale;
  widget_opts.packing_height = (float)def_widget_opts.packing_height * widget_opts.scale;
  widget_opts.filler_len = (float)def_widget_opts.filler_len * widget_opts.scale;
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_if_visible(LiVESWidget *widget) {
  if (GTK_IS_WIDGET(widget) && gtk_widget_is_drawable(widget)) {
    lives_widget_queue_draw(widget);
    return TRUE;
  }
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_queue_draw_and_update(LiVESWidget *widget) {
  lives_widget_queue_draw(widget);
  lives_widget_process_updates(widget, TRUE);
  return FALSE;
}


int lives_utf8_strcmpfunc(livesconstpointer a, livesconstpointer b, livespointer fwd) {
  // do not inline !
  int ret;
  char *tmp1, *tmp2;
  if (LIVES_POINTER_TO_INT(fwd))
    ret = lives_strcmp_ordered((tmp1 = lives_utf8_collate_key(a, -1)),
                               (tmp2 = lives_utf8_collate_key(b, -1)));
  else
    ret = lives_strcmp_ordered((tmp1 = lives_utf8_collate_key(b, -1)),
                               (tmp2 = lives_utf8_collate_key(a, -1)));
  lives_free(tmp1);
  lives_free(tmp2);
  return ret;
}


static int lives_utf8_menu_strcmpfunc(livesconstpointer a, livesconstpointer b, livespointer fwd) {
  return lives_utf8_strcmpfunc(lives_menu_item_get_text((LiVESWidget *)a), lives_menu_item_get_text((LiVESWidget *)b), fwd);
}


WIDGET_HELPER_LOCAL_INLINE LiVESList *lives_menu_list_sort_alpha(LiVESList *list, boolean fwd) {
  return lives_list_sort_with_data(list, lives_utf8_menu_strcmpfunc, LIVES_INT_TO_POINTER(fwd));
}


LiVESList *add_sorted_list_to_menu(LiVESMenu *menu, LiVESList *menu_list) {
  LiVESList **seclist;
  LiVESList *xmenu_list = menu_list = lives_menu_list_sort_alpha(menu_list, TRUE);
  while (menu_list != NULL) {
    if (!(LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menu_list->data), HIDDEN_KEY)))) {
      lives_container_add(LIVES_CONTAINER(menu), (LiVESWidget *)menu_list->data);
    }
    if ((seclist = (LiVESList **)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menu_list->data), SECLIST_KEY)) != NULL)
      * seclist = lives_list_prepend(*seclist, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menu_list->data),
                                     SECLIST_VAL_KEY));
    menu_list = menu_list->next;
  }
  return xmenu_list;
}


boolean lives_has_icon(const char *stock_id, LiVESIconSize size)  {
  boolean has_icon = FALSE;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  GtkIconInfo *iset = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), stock_id, size, GTK_ICON_LOOKUP_USE_BUILTIN);
#else
  GtkIconSet *iset = gtk_icon_factory_lookup_default(stock_id);
#endif
  has_icon = (iset != NULL);
#endif
  return has_icon;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGB48_t *lives_painter_set_source_rgb_from_lives_rgb(lives_painter_t *cr,
    lives_colRGB48_t *col) {
  lives_painter_set_source_rgb(cr,
                               (double)col->red / 65535.,
                               (double)col->green / 65535.,
                               (double)col->blue / 65535.
                              );
  return col;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t *lives_painter_set_source_rgb_from_lives_rgba(lives_painter_t *cr,
    lives_colRGBA64_t *col) {
  lives_painter_set_source_rgb(cr,
                               (double)col->red / 65535.,
                               (double)col->green / 65535.,
                               (double)col->blue / 65535.
                              );
  return col;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_painter_set_source_rgb_from_lives_widget_color(lives_painter_t *cr,
    LiVESWidgetColor *wcol) {
  lives_colRGBA64_t col;
  widget_color_to_lives_rgba(&col, wcol);
  lives_painter_set_source_rgb_from_lives_rgba(cr, &col);
  return wcol;
}


WIDGET_HELPER_GLOBAL_INLINE boolean clear_widget_bg(LiVESWidget *widget) {
  if (!LIVES_IS_WIDGET(widget)) return FALSE;
  else {
    lives_painter_t *cr = lives_painter_create_from_widget(widget);
    if (cr == NULL) return FALSE;
    else {
      int rwidth = lives_widget_get_allocation_width(LIVES_WIDGET(widget));
      int rheight = lives_widget_get_allocation_height(LIVES_WIDGET(widget));
      lives_painter_render_background(widget, cr, 0., 0., rwidth, rheight);
      if (!lives_painter_remerge(cr)) lives_painter_destroy(cr);
    }
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_cursor_unref(LiVESXCursor *cursor) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  g_object_unref(LIVES_GUI_OBJECT(cursor));
#else
  gdk_cursor_unref(cursor);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  delete cursor;
  return TRUE;
#endif
  return FALSE;
}


void lives_widget_apply_theme(LiVESWidget *widget, LiVESWidgetState state) {
  if (!widget_opts.apply_theme) return;
  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(widget, state, &palette->normal_fore);
    lives_widget_set_bg_color(widget, state, &palette->normal_back);
#if GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_set_base_color(widget, state, &palette->normal_back);
    lives_widget_set_text_color(widget, state, &palette->normal_fore);
#endif
  }
}


void lives_widget_apply_theme2(LiVESWidget *widget, LiVESWidgetState state, boolean set_fg) {
  if (!widget_opts.apply_theme) return;
  if (palette->style & STYLE_1) {
    if (set_fg)
      lives_widget_set_fg_color(widget, state, &palette->menu_and_bars_fore);
    lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
  }
}


void lives_widget_apply_theme3(LiVESWidget *widget, LiVESWidgetState state) {
  if (!widget_opts.apply_theme) return;
  if (palette->style & STYLE_1) {
    lives_widget_set_text_color(widget, state, &palette->info_text);
    lives_widget_set_base_color(widget, state, &palette->info_base);
    lives_widget_set_fg_color(widget, state, &palette->info_text);
    lives_widget_set_bg_color(widget, state, &palette->info_base);
  }
}


void lives_widget_apply_theme_dimmed(LiVESWidget *widget, LiVESWidgetState state, int dimval) {
  if (!widget_opts.apply_theme) return;
  if (palette->style & STYLE_1) {
    LiVESWidgetColor dimmed_fg;
    lives_widget_color_copy(&dimmed_fg, &palette->normal_fore);
    lives_widget_color_mix(&dimmed_fg, &palette->normal_back, (float)dimval / 65535.);
    lives_widget_set_fg_color(widget, state, &dimmed_fg);
    lives_widget_set_bg_color(widget, state, &palette->normal_back);
  }
}


void lives_widget_apply_theme_dimmed2(LiVESWidget *widget, LiVESWidgetState state, int dimval) {
  if (!widget_opts.apply_theme) return;
  if (palette->style & STYLE_1) {
    LiVESWidgetColor dimmed_fg;
    lives_widget_color_copy(&dimmed_fg, &palette->menu_and_bars_fore);
    lives_widget_color_mix(&dimmed_fg, &palette->menu_and_bars, (float)dimval / 65535.);
    lives_widget_set_fg_color(widget, state, &dimmed_fg);
    lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
  }
}


boolean lives_entry_set_completion_from_list(LiVESEntry *entry, LiVESList *xlist) {
#ifdef GUI_GTK
  GtkListStore *store;
  LiVESEntryCompletion *completion;
  store = gtk_list_store_new(1, LIVES_COL_TYPE_STRING);

  while (xlist != NULL) {
    LiVESTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, (char *)xlist->data, -1);
    xlist = xlist->next;
  }

  completion = gtk_entry_completion_new();
  gtk_entry_completion_set_model(completion, (GtkTreeModel *)store);
  gtk_entry_completion_set_text_column(completion, 0);
  gtk_entry_completion_set_inline_completion(completion, TRUE);
  gtk_entry_completion_set_popup_set_width(completion, TRUE);
  gtk_entry_completion_set_popup_completion(completion, TRUE);
  gtk_entry_completion_set_popup_single_match(completion, FALSE);
  gtk_entry_set_completion(entry, completion);
  return TRUE;
#endif
#ifdef GUI_QT
  QStringList qsl;
  for (int i = 0; xlist != NULL; i++) {
    qsl.append((QString::fromUtf8((const char *)xlist->data)));
    xlist = xlist->next;
  }
  QCompleter *qcmp = new QCompleter(qsl);
  QLineEdit *qe = static_cast<QLineEdit *>(entry);
  qe->setCompleter(qcmp);
#endif
  return FALSE;
}


boolean lives_window_center(LiVESWindow *window) {
  if (!widget_opts.no_gui) {
    int xcen, ycen;
    int width, height;
    int bx, by;

    if (widget_opts.screen != NULL) lives_window_set_screen(LIVES_WINDOW(window), widget_opts.screen);

    if (mainw->mgeom == NULL) {
      lives_widget_show(LIVES_WIDGET(window));
      lives_window_set_position(LIVES_WINDOW(window), LIVES_WIN_POS_CENTER_ALWAYS);
      return TRUE;
    }

    lives_window_set_position(LIVES_WINDOW(window), LIVES_WIN_POS_CENTER_ALWAYS);

    width = lives_widget_get_allocation_width(LIVES_WIDGET(window));
    if (width == 0) width = ((int)(620. * widget_opts.scale)); // MIN_MSGBOX_WIDTH in interface.h
    height = lives_widget_get_allocation_height(LIVES_WIDGET(window));

    get_border_size(LIVES_WIDGET(window), &bx, &by);
    width += bx;
    height += by;

    xcen = mainw->mgeom[widget_opts.monitor].x + ((mainw->mgeom[widget_opts.monitor].width - width) >> 1);

    ycen = mainw->mgeom[widget_opts.monitor].y + ((mainw->mgeom[widget_opts.monitor].height - height) >> 1);
    lives_window_move(LIVES_WINDOW(window), xcen, ycen);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_get_fg_color(LiVESWidget *widget, LiVESWidgetColor *color) {
  return lives_widget_get_fg_state_color(widget, LIVES_WIDGET_STATE_NORMAL, color);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_unparent(LiVESWidget *widget) {
  return lives_container_remove(LIVES_CONTAINER(lives_widget_get_parent(widget)), widget);
}



static void _toggle_if_condmet(LiVESWidget *tbut, livespointer widget, boolean cond) {
  char *keyval;
  int *condx;

  if (!cond) {
    keyval = lives_strdup_printf("%p_insens_cond", widget);
    condx = (int *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tbut), keyval);
    if (condx && *condx != 0) cond = TRUE;
  } else {
    keyval = lives_strdup_printf("%p_sens_cond", widget);
    condx = (int *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tbut), keyval);
    if (condx && *condx <= 0) cond = FALSE;
  }
  lives_free(keyval);
  lives_widget_set_sensitive(LIVES_WIDGET(widget), cond);
}

static void toggle_set_sensitive(LiVESWidget *tbut, livespointer widget) {
  _toggle_if_condmet(tbut, widget, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)));
}

static void toggle_set_insensitive(LiVESWidget *tbut, livespointer widget) {
  _toggle_if_condmet(tbut, widget, !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbut)));
}

static void togglevar_cb(LiVESToggleButton *tbut, boolean *var) {
  if (var) *var = !(*var);
}

// togglebutton functions

WIDGET_HELPER_GLOBAL_INLINE boolean toggle_sets_sensitive_cond(LiVESToggleButton *tb, LiVESWidget *widget,
    livespointer condsens, livespointer condinsens, boolean invert) {
  if (condsens) {
    /// set sensitive only if *condsens > 0
    char *keyval = lives_strdup_printf("%p_sens_cond", widget);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(tb), keyval, condsens);
  }

  if (condinsens) {
    /// set insensitive only if *condinsens == 0
    char *keyval = lives_strdup_printf("%p_insens_cond", widget);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(tb), keyval, condinsens);
  }

  if (!invert) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(toggle_set_sensitive),
                              (livespointer)widget);
    toggle_set_sensitive(LIVES_WIDGET(tb), (livespointer)widget);
  } else {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(tb), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(toggle_set_insensitive),
                              (livespointer)widget);
    toggle_set_insensitive(LIVES_WIDGET(tb), (livespointer)widget);
  }
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean toggle_sets_sensitive(LiVESToggleButton *tb, LiVESWidget *widget, boolean invert) {
  return toggle_sets_sensitive_cond(tb, widget, NULL, NULL, invert);
}


// widget callback sets togglebutton active
boolean widget_act_toggle(LiVESWidget *widget, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active(togglebutton, TRUE);
  return FALSE;
}


// widget callback sets togglebutton inactive
boolean widget_inact_toggle(LiVESWidget *widget, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active(togglebutton, FALSE);
  return FALSE;
}


boolean label_act_toggle(LiVESWidget *widget, LiVESXEventButton *event, LiVESToggleButton *togglebutton) {
  return widget_act_toggle(widget, togglebutton);
}


// set callback so that togglebutton controls var
WIDGET_HELPER_GLOBAL_INLINE boolean toggle_toggles_var(LiVESToggleButton *tbut, boolean *var, boolean invert) {
  if (invert) lives_toggle_button_set_active(tbut, !(*var));
  else lives_toggle_button_set_active(tbut, *var);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tbut), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(togglevar_cb),
                                  (livespointer)var);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_toggle_button_toggle(LiVESToggleButton *tbutton) {
  if (lives_toggle_button_get_active(tbutton)) return lives_toggle_button_set_active(tbutton, FALSE);
  else return lives_toggle_button_set_active(tbutton, TRUE);
}


static void _set_tooltips_state(LiVESWidget *widget, livespointer state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  char *ttip;
  if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_OVERRIDE_KEY)) return;

  if (LIVES_POINTER_TO_INT(state)) {
    // enable
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_HIDE_KEY)) {
      lives_widget_show(widget);
      return;
    }
    ttip = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY);
    if (ttip != NULL) {
      lives_widget_set_tooltip_text(widget, ttip);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, NULL);
    }
  } else {
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), TTIPS_HIDE_KEY)) {
      lives_widget_hide(widget);
      return;
    }
    ttip = gtk_widget_get_tooltip_text(widget);
    lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(widget), TTIPS_KEY, ttip);
    lives_widget_set_tooltip_text(widget, NULL);
  }
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), _set_tooltips_state, state);
  }
#endif
#endif

}


WIDGET_HELPER_GLOBAL_INLINE boolean set_tooltips_state(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 12, 0)
  _set_tooltips_state(widget, LIVES_INT_TO_POINTER(state));
  return TRUE;
#endif
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE double lives_spin_button_get_snapval(LiVESSpinButton *button, double val) {
  double stepval, min, max, nval, stepfix;
  int digs = gtk_spin_button_get_digits(button);
  boolean wrap = gtk_spin_button_get_wrap(button);
  double tenpow = (double)lives_10pow(digs);
  gtk_spin_button_get_increments(button, &stepval, NULL);
  gtk_spin_button_get_range(button, &min, &max);
  stepfix = tenpow / stepval;
  if (val >= 0.)
    nval = (double)((int64_t)(val * stepfix + .5)) / stepfix;
  else
    nval = (double)((int64_t)(val * stepfix  - .5)) / stepfix;
  if (nval < min) {
    if (wrap) while (nval < min) nval += (max - min);
    else nval = min;
  }
  if (nval > max) {
    if (wrap) while (nval > max) nval -= (max - min);
    else nval = max;
  }
  return nval;
}


static void set_child_colour_internal(LiVESWidget *widget, livespointer set_allx) {
  boolean set_all = LIVES_POINTER_TO_INT(set_allx);

  if (!set_all && LIVES_IS_BUTTON(widget)) return; // avoids a problem with filechooser
  if (set_all || LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_NORMAL);
    if (!LIVES_IS_LABEL(widget))
      lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_INSENSITIVE);
  }
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), set_child_colour_internal, set_allx);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_colour(LiVESWidget *widget, boolean set_all) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)
  set_child_colour_internal(widget, LIVES_INT_TO_POINTER(set_all));
}


static void set_child_dimmed_colour_internal(LiVESWidget *widget, livespointer dim) {
  int dimval = LIVES_POINTER_TO_INT(dim);

  lives_widget_apply_theme_dimmed(widget, LIVES_WIDGET_STATE_INSENSITIVE, dimval);
  lives_widget_apply_theme_dimmed(widget, LIVES_WIDGET_STATE_NORMAL, dimval);

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), set_child_dimmed_colour_internal, dim);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_dimmed_colour(LiVESWidget *widget, int dim) {
  // set widget and all children widgets
  // fg is affected dim value
  // dim takes a value from 0 (full fg) -> 65535 (full bg)
  set_child_dimmed_colour_internal(widget, LIVES_INT_TO_POINTER(dim));
}


static void set_child_dimmed_colour2_internal(LiVESWidget *widget, livespointer dim) {
  int dimval = LIVES_POINTER_TO_INT(dim);

  lives_widget_apply_theme_dimmed2(widget, LIVES_WIDGET_STATE_INSENSITIVE, dimval);

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), set_child_dimmed_colour2_internal, dim);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_dimmed_colour2(LiVESWidget *widget, int dim) {
  // set widget and all children widgets
  // fg is affected dim value
  // dim takes a value from 0 (full fg) -> 65535 (full bg)
  set_child_dimmed_colour2_internal(widget, LIVES_INT_TO_POINTER(dim));
}


static void set_child_alt_colour_internal(LiVESWidget *widget, livespointer set_allx) {
  boolean set_all = LIVES_POINTER_TO_INT(set_allx);

  if (!set_all && LIVES_IS_BUTTON(widget)) return;

  if (set_all || LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_INSENSITIVE, TRUE);
    lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), set_child_alt_colour_internal, set_allx);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_alt_colour(LiVESWidget *widget, boolean set_all) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)

  set_child_alt_colour_internal(widget, LIVES_INT_TO_POINTER(set_all));
}


static void set_child_alt_colour_internal_prelight(LiVESWidget *widget, livespointer data) {
  lives_widget_apply_theme2(widget, LIVES_WIDGET_STATE_PRELIGHT, TRUE);
  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), set_child_alt_colour_internal_prelight, NULL);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_alt_colour_prelight(LiVESWidget *widget) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)
  set_child_alt_colour_internal_prelight(widget, NULL);
}


static void set_child_colour3_internal(LiVESWidget *widget, livespointer set_allx) {
  boolean set_all = LIVES_POINTER_TO_INT(set_allx);

  if (!set_all && (LIVES_IS_BUTTON(widget))) {// || LIVES_IS_SCROLLBAR(widget))) {
    lives_widget_set_base_color(widget, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_text_color(widget, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
    return;
  }

  if (set_all || LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme3(widget, LIVES_WIDGET_STATE_NORMAL);
  }

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget), set_child_colour3_internal, set_allx);
  }
}


WIDGET_HELPER_GLOBAL_INLINE void set_child_colour3(LiVESWidget *widget, boolean set_all) {
  // set widget and all children widgets
  // if set_all is FALSE, we only set labels (and ignore labels in buttons)

  set_child_colour3_internal(widget, LIVES_INT_TO_POINTER(set_all));
}


char *lives_text_view_get_text(LiVESTextView *textview) {
  LiVESTextIter siter, eiter;
  LiVESTextBuffer *textbuf = lives_text_view_get_buffer(textview);
  lives_text_buffer_get_start_iter(textbuf, &siter);
  lives_text_buffer_get_end_iter(textbuf, &eiter);

  return lives_text_buffer_get_text(textbuf, &siter, &eiter, FALSE);
}


boolean lives_text_view_set_text(LiVESTextView *textview, const char *text, int len) {
  LiVESTextBuffer *textbuf = lives_text_view_get_buffer(textview);
  if (textbuf != NULL)
    return lives_text_buffer_set_text(textbuf, text, len);
  return FALSE;
}


boolean lives_text_buffer_insert_at_end(LiVESTextBuffer *tbuff, const char *text) {
  LiVESTextIter xiter;
  if (lives_text_buffer_get_end_iter(tbuff, &xiter))
    return lives_text_buffer_insert(tbuff, &xiter, text, -1);
  return FALSE;
}


int get_box_child_index(LiVESBox *box, LiVESWidget *tchild) {
  LiVESList *list = lives_container_get_children(LIVES_CONTAINER(box));
  int val = -1;
  if (list != NULL) {
    val = lives_list_index(list, tchild);
    lives_list_free(list);
  }
  return val;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_box_pack_top(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill,
    uint32_t padding) {
  lives_box_pack_start(box, child, expand, fill, padding);
  lives_box_reorder_child(box, child, 0);
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_container_child_set_shrinkable(LiVESContainer *c, LiVESWidget *child, boolean val) {
#ifdef GUI_GTK
  GValue bool = G_VALUE_INIT;
  g_value_init(&bool, G_TYPE_BOOLEAN);
  g_value_set_boolean(&bool, val);
  gtk_container_child_set_property(c, child, "shrink", &bool);
  return TRUE;
#endif
  return FALSE;
}


boolean set_submenu_colours(LiVESMenu *menu, LiVESWidgetColor *colf, LiVESWidgetColor *colb) {
  LiVESList *children = lives_container_get_children(LIVES_CONTAINER(menu)), *list = children;
  lives_widget_set_bg_color(LIVES_WIDGET(menu), LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_fg_color(LIVES_WIDGET(menu), LIVES_WIDGET_STATE_NORMAL, colf);
  while (list != NULL) {
    LiVESWidget *child = (LiVESWidget *)list->data;
    if (LIVES_IS_MENU_ITEM(child)) {
      if ((menu = (LiVESMenu *)lives_menu_item_get_submenu(LIVES_MENU_ITEM(child))) != NULL) set_submenu_colours(menu, colf, colb);
      else {
        lives_widget_set_bg_color(LIVES_WIDGET(child), LIVES_WIDGET_STATE_NORMAL, colb);
        lives_widget_set_fg_color(LIVES_WIDGET(child), LIVES_WIDGET_STATE_NORMAL, colf);
      }
    }
    list = list->next;
  }
  if (children != NULL) lives_list_free(children);
  return TRUE;
}


boolean lives_spin_button_configure(LiVESSpinButton *spinbutton,
                                    double value,
                                    double lower,
                                    double upper,
                                    double step_increment,
                                    double page_increment) {
  LiVESAdjustment *adj = lives_spin_button_get_adjustment(spinbutton);

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2, 14, 0)
  gtk_adjustment_configure(adj, value, lower, upper, step_increment, page_increment, 0.);
#else
  g_object_freeze_notify(LIVES_WIDGET_OBJECT(adj));
  adj->upper = upper;
  adj->lower = lower;
  adj->value = value;
  adj->step_increment = step_increment;
  adj->page_increment = page_increment;
  g_object_thaw_notify(LIVES_WIDGET_OBJECT(adj));
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  adj->set_lower(lower);
  adj->set_upper(upper);
  adj->set_value(value);
  adj->set_step_increment(step_increment);
  adj->set_page_increment(page_increment);
  return TRUE;
#endif
  return FALSE;
}


boolean lives_tree_store_find_iter(LiVESTreeStore *tstore, int col, const char *val, LiVESTreeIter *titer1,
                                   LiVESTreeIter *titer2) {
#ifdef GUI_GTK
  if (gtk_tree_model_iter_children(LIVES_TREE_MODEL(tstore), titer2, titer1)) {
    char *ret;
    while (1) {
      gtk_tree_model_get(LIVES_TREE_MODEL(tstore), titer2, col, &ret, -1);
      if (!lives_strcmp(ret, val)) {
        lives_free(ret);
        return TRUE;
      }
      lives_free(ret);
      if (!gtk_tree_model_iter_next(LIVES_TREE_MODEL(tstore), titer2)) break;
    }
  }
  lives_tree_store_append(tstore, titer2, titer1);
  lives_tree_store_set(tstore, titer2, col, val, -1);
  return TRUE;
#endif
  return FALSE;
}


///// lives specific functions

#include "rte_window.h"
#include "ce_thumbs.h"

static boolean noswitch = FALSE;
static boolean mt_needs_idlefunc = FALSE;

static void do_some_things(void) {
  // som old junk that may or may not be relevant now
  //
  /// clip switching is not permitted during these "artificial" context updates
  noswitch = mainw->noswitch;
  /// except under very specific conditions, e.g.
  mainw->noswitch = mainw->cs_is_permitted;

  if (mainw->multitrack != NULL && mainw->multitrack->idlefunc > 0) {
    lives_timer_remove(mainw->multitrack->idlefunc);
    mainw->multitrack->idlefunc = 0;
    mt_needs_idlefunc = TRUE;
  }

  if (!mainw->is_exiting) {
    if (rte_window != NULL) rtew_set_key_check_state();
    if (mainw->ce_thumbs) {
      ce_thumbs_set_key_check_state();
      ce_thumbs_apply_liberation();
      if (mainw->ce_upd_clip) {
        ce_thumbs_highlight_current_clip();
        mainw->ce_upd_clip = FALSE;
      }
    }
  }
}


static void do_more_stuff(void) {
  if (!mainw->is_exiting && mt_needs_idlefunc && mainw->multitrack) {
    mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
  }
  /// re-enable clip switching. It should be possible during "natural" context updates (i.e outside of callbacks)
  /// (unless we are playing, in which case noswitch is always FALSE)
  mainw->noswitch = noswitch;
}


/* #define MAX_NULL_EVENTS 512 // general max, some events allow twice this */
/* #define LOOP_LIMIT 32 // max when playing and not in multitrack */
boolean lives_widget_context_update(void) {
  static volatile boolean norecurse = FALSE;

  if (mainw->no_context_update) return FALSE;
  if (norecurse) return FALSE;
  else {
    LiVESWidgetContext *ctx = lives_widget_context_get_thread_default();
    norecurse = TRUE;
    if (ctx != NULL && ctx != lives_widget_context_default() && gov_running) {
      do_some_things();
      mainw->clutch = FALSE;
      while (!mainw->clutch && !mainw->is_exiting) {
        lives_nanosleep(NSLEEP_TIME);
        sched_yield();
      }
      do_more_stuff();
    } else {
      while (lives_widget_context_iteration(NULL, FALSE));
    }
  }
  norecurse = FALSE;
  return TRUE;
}


LiVESWidget *lives_menu_add_separator(LiVESMenu *menu) {
  LiVESWidget *separatormenuitem = lives_menu_item_new();
  if (separatormenuitem != NULL) {
    lives_container_add(LIVES_CONTAINER(menu), separatormenuitem);
    lives_widget_set_sensitive(separatormenuitem, FALSE);
  }
  return separatormenuitem;
}


WIDGET_HELPER_GLOBAL_INLINE void lives_menu_item_set_text(LiVESWidget *menuitem, const char *text, boolean use_mnemonic) {
  LiVESWidget *label;
  if (LIVES_IS_MENU_ITEM(menuitem)) {
    label = lives_bin_get_child(LIVES_BIN(menuitem));
    widget_opts.mnemonic_label = use_mnemonic;
    lives_label_set_text(LIVES_LABEL(label), text);
    widget_opts.mnemonic_label = TRUE;
  }
}


WIDGET_HELPER_GLOBAL_INLINE const char *lives_menu_item_get_text(LiVESWidget *menuitem) {
  // text MUST be at least 255 chars long
  LiVESWidget *label = lives_bin_get_child(LIVES_BIN(menuitem));
  return lives_label_get_text(LIVES_LABEL(label));
}


WIDGET_HELPER_GLOBAL_INLINE int lives_display_get_n_screens(LiVESXDisplay *disp) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  return 1;
#else
  return gdk_display_get_n_screens(disp);
#endif
#endif
#ifdef GUI_QT
  return QApplication::desktop()->screenCount();
#endif
  return 1;
}


void lives_set_cursor_style(lives_cursor_t cstyle, LiVESWidget *widget) {
#ifdef GUI_GTK
  LiVESXWindow *window;
  GdkCursor *cursor = NULL;
  GdkDisplay *disp;
  GdkCursorType ctype = GDK_X_CURSOR;

  if (widget == NULL) {
    if (mainw->recovering_files || ((mainw->multitrack == NULL && mainw->is_ready) || (mainw->multitrack != NULL &&
                                    mainw->multitrack->is_ready))) {
      if (cstyle != LIVES_CURSOR_NORMAL && mainw->cursor_style == cstyle) return;
      window = lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET);
    } else return;
  } else window = lives_widget_get_xwindow(widget);

  if (!LIVES_IS_XWINDOW(window)) return;

  switch (cstyle) {
  case LIVES_CURSOR_NORMAL:
    break;
  case LIVES_CURSOR_BUSY:
    ctype = GDK_WATCH;
    break;
  case LIVES_CURSOR_CENTER_PTR:
    ctype = GDK_CENTER_PTR;
    break;
  case LIVES_CURSOR_HAND2:
    ctype = GDK_HAND2;
    break;
  case LIVES_CURSOR_SB_H_DOUBLE_ARROW:
    ctype = GDK_SB_H_DOUBLE_ARROW;
    break;
  case LIVES_CURSOR_CROSSHAIR:
    ctype = GDK_CROSSHAIR;
    break;
  case LIVES_CURSOR_TOP_LEFT_CORNER:
    ctype = GDK_TOP_LEFT_CORNER;
    break;
  case LIVES_CURSOR_BOTTOM_RIGHT_CORNER:
    ctype = GDK_BOTTOM_RIGHT_CORNER;
    break;
  default:
    return;
  }
  if (widget == NULL) {
    if (mainw->multitrack != NULL) mainw->multitrack->cursor_style = cstyle;
    else mainw->cursor_style = cstyle;
  }
#if GTK_CHECK_VERSION(2, 22, 0)
  cursor = gdk_window_get_cursor(window);
  if (cursor != NULL && gdk_cursor_get_cursor_type(cursor) == ctype) return;
  cursor = NULL;
#endif
  disp = gdk_window_get_display(window);
  if (cstyle != LIVES_CURSOR_NORMAL) {
    cursor = gdk_cursor_new_for_display(disp, ctype);
    gdk_window_set_cursor(window, cursor);
  } else gdk_window_set_cursor(window, NULL);
  if (cursor != NULL) lives_cursor_unref(cursor);
#endif

#ifdef GUI_QT
  if (widget == NULL) {
    if (mainw->multitrack == NULL && mainw->is_ready) {
      if (cstyle != LIVES_CURSOR_NORMAL && mainw->cursor_style == cstyle) return;
      widget = LIVES_MAIN_WINDOW_WIDGET;
    } else if (mainw->multitrack != NULL && mainw->multitrack->is_ready) {
      if (cstyle != LIVES_CURSOR_NORMAL && mainw->multitrack->cursor_style == cstyle) return;
      widget = mainw->multitrack->window;
    } else return;
  }

  switch (cstyle) {
  case LIVES_CURSOR_NORMAL:
    widget->setCursor(Qt::ArrowCursor);
    break;
  case LIVES_CURSOR_BUSY:
    widget->setCursor(Qt::WaitCursor);
    break;
  case LIVES_CURSOR_CENTER_PTR:
    widget->setCursor(Qt::UpArrowCursor);
    break;
  case LIVES_CURSOR_HAND2:
    widget->setCursor(Qt::PointingHandCursor);
    break;
  case LIVES_CURSOR_SB_H_DOUBLE_ARROW:
    widget->setCursor(Qt::SizeHorCursor);
    break;
  case LIVES_CURSOR_CROSSHAIR:
    widget->setCursor(Qt::CrossCursor);
    break;
  case LIVES_CURSOR_TOP_LEFT_CORNER:
    widget->setCursor(Qt::SizeFDiagCursor);
    break;
  case LIVES_CURSOR_BOTTOM_RIGHT_CORNER:
    widget->setCursor(Qt::SizeBDiagCursor);
    break;
  default:
    return;
  }
#endif
}


void hide_cursor(LiVESXWindow *window) {
  //make the cursor invisible in playback windows
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2, 16, 0)
  if (GDK_IS_WINDOW(window)) {
#if GTK_CHECK_VERSION(3, 16, 0)
    GdkCursor *cursor = gdk_cursor_new_for_display(gdk_window_get_display(window), GDK_BLANK_CURSOR);
#else
    GdkCursor *cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
#endif
    if (cursor != NULL) {
      gdk_window_set_cursor(window, cursor);
      lives_cursor_unref(cursor);
    }
  }
#else
  static GdkCursor *hidden_cursor = NULL;

  char cursor_bits[] = {0x00};
  char cursormask_bits[] = {0x00};
  GdkPixmap *source, *mask;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0, 0, 0 };

  if (hidden_cursor == NULL) {
    source = gdk_bitmap_create_from_data(NULL, cursor_bits,
                                         1, 1);
    mask = gdk_bitmap_create_from_data(NULL, cursormask_bits,
                                       1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 0, 0);
    g_object_unref(source);
    g_object_unref(mask);
  }
  if (GDK_IS_WINDOW(window)) gdk_window_set_cursor(window, hidden_cursor);
#endif
#endif
#ifdef GUI_QT
  window->setCursor(Qt::BlankCursor);
#endif
}


WIDGET_HELPER_GLOBAL_INLINE boolean unhide_cursor(LiVESXWindow *window) {
  if (LIVES_IS_XWINDOW(window)) return lives_xwindow_set_cursor(window, NULL);
  return FALSE;
}


void funkify_dialog(LiVESWidget *dialog) {
  if (prefs->funky_widgets) {
    LiVESWidget *frame = lives_standard_frame_new(NULL, 0., FALSE);
    LiVESWidget *box = lives_vbox_new(FALSE, 0);
    LiVESWidget *content = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
    LiVESWidget *action = lives_dialog_get_action_area(LIVES_DIALOG(dialog));

    lives_container_set_border_width(LIVES_CONTAINER(dialog), 0);
    lives_container_set_border_width(LIVES_CONTAINER(frame), 0);

    lives_widget_object_ref(content);
    lives_widget_unparent(content);

    lives_container_add(LIVES_CONTAINER(dialog), frame);
    lives_container_add(LIVES_CONTAINER(frame), box);

    lives_box_pack_start(LIVES_BOX(box), content, TRUE, TRUE, 0);

    lives_widget_set_margin_top(action, widget_opts.packing_height); // only works for gtk+ 3.x

    lives_widget_show_all(frame);

    lives_container_set_border_width(LIVES_CONTAINER(box), widget_opts.border_width * 2);
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width);
  }
}


void lives_cool_toggled(LiVESWidget *tbutton, livespointer user_data) {
#if GTK_CHECK_VERSION(3, 0, 0)
  // connect toggled event to this
  boolean *ret = (boolean *)user_data, active;
  if (!LIVES_IS_INTERACTIVE) return;
  active = ((LIVES_IS_TOGGLE_BUTTON(tbutton) && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tbutton))) ||
            (LIVES_IS_TOGGLE_TOOL_BUTTON(tbutton) && lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(tbutton))));
  if (prefs->lamp_buttons) {
    if (active)
      lives_widget_set_bg_color(tbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    else lives_widget_set_bg_color(tbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
  }
  if (ret != NULL) *ret = active;
#endif
}


EXPOSE_FN_DECL(draw_cool_toggle, widget, user_data) {
  // connect expose event to this
  lives_painter_t *cr;
  double rwidth = (double)lives_widget_get_allocation_width(LIVES_WIDGET(widget));
  double rheight = (double)lives_widget_get_allocation_height(LIVES_WIDGET(widget));

  double rad;

  double scalex = 1.;
  double scaley = .8;

  boolean active = ((LIVES_IS_TOGGLE_BUTTON(widget) && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget))) ||
                    (LIVES_IS_TOGGLE_TOOL_BUTTON(widget)
                     && lives_toggle_tool_button_get_active(LIVES_TOGGLE_TOOL_BUTTON(widget))));

  if (cairo == NULL) cr = lives_painter_create_from_widget(widget);
  else cr = cairo;

  lives_painter_translate(cr, rwidth * (1. - scalex) / 2., rheight * (1. - scaley) / 2.);

  rwidth *= scalex;
  rheight *= scaley;

  // draw the inside

  if (active) {
    lives_painter_set_source_rgba(cr, palette->light_green.red, palette->light_green.green,
                                  palette->light_green.blue, 1.);
  } else {
    lives_painter_set_source_rgba(cr, palette->dark_red.red, palette->dark_red.green,
                                  palette->dark_red.blue, 1.);
  }

  // draw rounded rctangle
  lives_painter_rectangle(cr, 0, rwidth / 4,
                          rwidth,
                          rheight - rwidth / 2);
  lives_painter_fill(cr);

  lives_painter_rectangle(cr, rwidth / 4, 0,
                          rwidth / 2,
                          rwidth / 4);
  lives_painter_fill(cr);

  lives_painter_rectangle(cr, rwidth / 4, rheight - rwidth / 4,
                          rwidth / 2,
                          rwidth / 4);
  lives_painter_fill(cr);

  rad = rwidth / 4.;

  lives_painter_move_to(cr, rwidth / 4., rwidth / 4.);
  lives_painter_line_to(cr, 0., rwidth / 4.);
  lives_painter_arc(cr, rwidth / 4., rwidth / 4., rad, M_PI, 1.5 * M_PI);
  lives_painter_line_to(cr, rwidth / 4., rwidth / 4.);
  lives_painter_fill(cr);

  lives_painter_move_to(cr, rwidth / 4.*3., rwidth / 4.);
  lives_painter_line_to(cr, rwidth / 4.*3., 0.);
  lives_painter_arc(cr, rwidth / 4.*3., rwidth / 4., rad, -M_PI / 2., 0.);
  lives_painter_line_to(cr, rwidth / 4.*3., rwidth / 4.);
  lives_painter_fill(cr);

  lives_painter_move_to(cr, rwidth / 4., rheight - rwidth / 4.);
  lives_painter_line_to(cr, rwidth / 4., rheight);
  lives_painter_arc(cr, rwidth / 4., rheight - rwidth / 4., rad, M_PI / 2., M_PI);
  lives_painter_line_to(cr, rwidth / 4., rheight - rwidth / 4.);
  lives_painter_fill(cr);

  lives_painter_move_to(cr, rwidth / 4.*3., rheight - rwidth / 4.);
  lives_painter_line_to(cr, rwidth, rheight - rwidth / 4.);
  lives_painter_arc(cr, rwidth / 4.*3., rheight - rwidth / 4., rad, 0., M_PI / 2.);
  lives_painter_line_to(cr, rwidth / 4.*3., rheight - rwidth / 4.);
  lives_painter_fill(cr);

  // draw the surround

  lives_painter_new_path(cr);

  lives_painter_set_source_rgba(cr, 0., 0., 0., .8);
  lives_painter_set_line_width(cr, 1.);

  lives_painter_arc(cr, rwidth / 4., rwidth / 4., rad, M_PI, 1.5 * M_PI);
  lives_painter_stroke(cr);
  lives_painter_arc(cr, rwidth / 4.*3., rwidth / 4., rad, -M_PI / 2., 0.);
  lives_painter_stroke(cr);
  lives_painter_arc(cr, rwidth / 4., rheight - rwidth / 4., rad, M_PI / 2., M_PI);
  lives_painter_stroke(cr);
  lives_painter_arc(cr, rwidth / 4.*3., rheight - rwidth / 4., rad, 0., M_PI / 2.);

  lives_painter_stroke(cr);

  lives_painter_move_to(cr, rwidth / 4., 0);
  lives_painter_line_to(cr, rwidth / 4.*3., 0);

  lives_painter_stroke(cr);

  lives_painter_move_to(cr, rwidth / 4., rheight);
  lives_painter_line_to(cr, rwidth / 4.*3., rheight);

  lives_painter_stroke(cr);

  lives_painter_move_to(cr, 0., rwidth / 4.);
  lives_painter_line_to(cr, 0., rheight - rwidth / 4.);

  lives_painter_stroke(cr);

  lives_painter_move_to(cr, rwidth, rwidth / 4.);
  lives_painter_line_to(cr, rwidth, rheight - rwidth / 4.);

  lives_painter_stroke(cr);

  if (active) {
    lives_painter_set_source_rgba(cr, 1., 1., 1., .6);

    lives_painter_move_to(cr, rwidth / 4., rwidth / 4.);
    lives_painter_line_to(cr, rwidth / 4.*3., rheight - rwidth / 4.);
    lives_painter_stroke(cr);

    lives_painter_move_to(cr, rwidth / 4., rheight - rwidth / 4.);
    lives_painter_line_to(cr, rwidth / 4.*3., rwidth / 4.);
    lives_painter_stroke(cr);
  }
  if (cr != cairo) if (!lives_painter_remerge(cr)) lives_painter_destroy(cr);
  return TRUE;
}
EXPOSE_FN_END


WIDGET_HELPER_GLOBAL_INLINE boolean lives_window_get_inner_size(LiVESWindow *win, int *x, int *y) {
  // get size request for child to fill window "win" (assuming win is maximised and moved maximum top / left)
#ifdef GUI_GTK
  GdkRectangle rect;
  gint wx, wy;
  gdk_window_get_frame_extents(lives_widget_get_xwindow(LIVES_WIDGET(win)), &rect);
  gdk_window_get_origin(lives_widget_get_xwindow(LIVES_WIDGET(win)), &wx, &wy);
  if (x != NULL) *x = mainw->mgeom[widget_opts.monitor].width - (wx - rect.x) * 2;
  if (y != NULL) *y = mainw->mgeom[widget_opts.monitor].height;
  return TRUE;
#endif
  return FALSE;
}


boolean get_border_size(LiVESWidget *win, int *bx, int *by) {
#ifdef GUI_GTK
  GdkRectangle rect;
  gint wx, wy;
  GdkWindow *xwin = lives_widget_get_xwindow(win);
  if (xwin == NULL) {
    lives_thread_data_t *tdata = get_thread_data();
    LiVESWidgetContext *ctx = tdata->ctx;
    lives_widget_context_pop_thread_default(ctx);
    //gtk_widget_realize(win);
    lives_widget_context_update();
    lives_widget_context_push_thread_default(ctx);
    xwin = lives_widget_get_xwindow(win);
    if (xwin == NULL) {
      if (bx != NULL) *bx = 0;
      if (by != NULL) *by = 0;
      return FALSE;
    }
  }
  gdk_window_get_frame_extents(lives_widget_get_xwindow(win), &rect);
  //g_print("F.EXT = %d\n", rect.height);
  gdk_window_get_origin(lives_widget_get_xwindow(win), &wx, &wy);
  if (bx != NULL) {
    *bx = rect.width - lives_widget_get_allocation_width(win);
  }
  if (by != NULL) {
    *by = rect.height - lives_widget_get_allocation_height(win);
  }
  return TRUE;
#endif
#ifdef GUI_QT
  win->winId();
  QWindow *qwindow = win->windowHandle();
  QMargins qm = qwindow->frameMargins();
  *bx = qm.left() + qm.right();
  *by = qm.top() + qm.bottom();
  return TRUE;
#endif
  return FALSE;
}


/*
   Set active string to the combo box
*/
WIDGET_HELPER_GLOBAL_INLINE boolean lives_combo_set_active_string(LiVESCombo *combo, const char *active_str) {
  return lives_entry_set_text(LIVES_ENTRY(lives_bin_get_child(LIVES_BIN(combo))), active_str);
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_combo_get_entry(LiVESCombo *widget) {
  return lives_bin_get_child(LIVES_BIN(widget));
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_widget_set_can_focus_and_default(LiVESWidget *widget) {
  if (!lives_widget_set_can_focus(widget, TRUE)) return FALSE;
  return lives_widget_set_can_default(widget, TRUE);
}


void lives_general_button_clicked(LiVESButton *button, livespointer data_to_free) {
  // destroy the button top-level and free data
  if (LIVES_IS_WIDGET(lives_widget_get_toplevel(LIVES_WIDGET(button)))) {
    lives_widget_destroy(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  }
  lives_freep((void **)&data_to_free);
  maybe_add_mt_idlefunc();
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_standard_hseparator_new(void) {
  LiVESWidget *hseparator = lives_hseparator_new();
  lives_widget_apply_theme(hseparator, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_set_fg_color(hseparator, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  return hseparator;
}


LiVESWidget *add_hsep_to_box(LiVESBox *box) {
  LiVESWidget *hseparator = lives_standard_hseparator_new();
  int packing_height = widget_opts.packing_height;
  if (LIVES_IS_HBOX(box)) packing_height = 0;
  lives_box_pack_start(box, hseparator, LIVES_IS_HBOX(box) || LIVES_SHOULD_EXPAND_EXTRA_FOR(box), TRUE, packing_height);
  return hseparator;
}


LiVESWidget *add_vsep_to_box(LiVESBox *box) {
  LiVESWidget *vseparator = lives_vseparator_new();
  int packing_width = widget_opts.packing_width >> 1;
  if (LIVES_SHOULD_EXPAND_EXTRA_FOR(box)) packing_width *= 2;
  lives_box_pack_start(box, vseparator, FALSE, FALSE, packing_width);
  lives_widget_apply_theme(vseparator, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_set_bg_color(vseparator, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  return vseparator;
}


//#define SHOW_FILL
LiVESWidget *add_fill_to_box(LiVESBox *box) {
#ifdef SHOW_FILL
  LiVESWidget *widget = lives_label_new("fill");
#else
  LiVESWidget *widget = lives_standard_label_new("");
#endif
  if (LIVES_IS_HBOX(box)) {
    if (LIVES_SHOULD_EXPAND_EXTRA_FOR(box)) {
      lives_box_pack_start(box, widget, TRUE, TRUE, widget_opts.filler_len);
    } else lives_box_pack_start(box, widget, FALSE, TRUE, LIVES_SHOULD_EXPAND_FOR(box) ? widget_opts.filler_len : 0);
  } else {
    if (LIVES_SHOULD_EXPAND_EXTRA_FOR(box)) {
      lives_box_pack_start(box, widget, TRUE, TRUE, widget_opts.filler_len);
    } else lives_box_pack_start(box, widget, FALSE, TRUE, LIVES_SHOULD_EXPAND_FOR(box) ? widget_opts.packing_height : 0);
  }

  return widget;
}


LiVESWidget *add_spring_to_box(LiVESBox *box, int min) {
  LiVESWidget *widget;
  int filler_len = widget_opts.filler_len;
  int woe = widget_opts.expand;
  widget_opts.filler_len = min;
  widget_opts.expand = LIVES_EXPAND_EXTRA;
  widget = add_fill_to_box(box);
  widget_opts.expand = woe;
  widget_opts.filler_len = filler_len;
  return widget;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toolbar_insert_space(LiVESToolbar *bar) {
  LiVESWidget *spacer = NULL;
#ifdef GUI_GTK
  spacer = LIVES_WIDGET(lives_separator_tool_item_new());
  gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(spacer), FALSE);
  gtk_tool_item_set_homogeneous(LIVES_TOOL_ITEM(spacer), FALSE);
  gtk_tool_item_set_expand(LIVES_TOOL_ITEM(spacer), LIVES_SHOULD_EXPAND_WIDTH);
  lives_toolbar_insert(LIVES_TOOLBAR(bar), LIVES_TOOL_ITEM(spacer), -1);
#endif
  return spacer;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidget *lives_toolbar_insert_label(LiVESToolbar *bar, const char *text) {
  LiVESWidget *item = NULL;
  widget_opts.last_label = NULL;
#ifdef GUI_GTK
  item = LIVES_WIDGET(lives_tool_item_new());
  widget_opts.last_label = lives_label_new(text);
  lives_container_add(LIVES_CONTAINER(item), widget_opts.last_label);
  lives_toolbar_insert(bar, LIVES_TOOL_ITEM(item), -1);
#endif
  return item;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_box_set_button_width(LiVESButtonBox *bbox, LiVESWidget *button,
    int min_width) {
  lives_button_box_set_layout(bbox, LIVES_BUTTONBOX_SPREAD);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_button_box_set_child_size(bbox, min_width / 4, -1);
  return TRUE;
#endif
#endif
  lives_widget_set_size_request(button, min_width, -1);
  return TRUE;
#ifdef GUI_QT
  button->setMinimumWidth(min_width);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_center(LiVESWidget *button) {
  lives_widget_set_size_request(button, DEF_BUTTON_WIDTH * 4, -1);
#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_button_box_set_layout(LIVES_BUTTON_BOX(lives_widget_get_parent(button)), LIVES_BUTTONBOX_CENTER);
#else
  lives_widget_set_halign(lives_widget_get_parent(button), LIVES_ALIGN_CENTER);
#endif
  return TRUE;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_button_uncenter(LiVESWidget *button, int normal_width) {
  lives_widget_set_size_request(button, normal_width, -1);
#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_button_box_set_layout(LIVES_BUTTON_BOX(lives_widget_get_parent(button)), LIVES_BUTTONBOX_END);
#else
  lives_widget_set_halign(lives_widget_get_parent(button), LIVES_ALIGN_FILL);
#endif
  return TRUE;
}


boolean lives_tool_button_set_border_colour(LiVESWidget *button, LiVESWidgetState state, LiVESWidgetColor *colour) {
  if (LIVES_IS_TOOL_BUTTON(button)) {
    LiVESWidget *widget, *parent;
    widget = lives_tool_button_get_icon_widget(LIVES_TOOL_BUTTON(button));
    if (widget == NULL) widget = lives_tool_button_get_label_widget(LIVES_TOOL_BUTTON(button));
    if (widget != NULL) {
      parent  = lives_widget_get_parent(widget);
      if (parent != NULL) lives_widget_set_bg_color(parent, state, colour);
      lives_widget_set_valign(widget, LIVES_ALIGN_FILL);
      lives_widget_set_halign(widget, LIVES_ALIGN_FILL);
    }
    return TRUE;
  }
  return FALSE;
}


LiVESWidget *lives_standard_tool_button_new(LiVESToolbar *bar, GtkWidget *icon_widget, const char *label,
    const char *tooltips) {
  LiVESToolItem *tbutton;
  widget_opts.last_label = NULL;
  if (label != NULL) {
    if (LIVES_SHOULD_EXPAND_HEIGHT) {
      char *labeltext = lives_strdup_printf("\n%s\n", label);
      widget_opts.last_label = lives_standard_label_new(labeltext);
      lives_free(labeltext);
    } else widget_opts.last_label = lives_standard_label_new(label);
  }
  tbutton = lives_tool_button_new(icon_widget, NULL);
  if (widget_opts.last_label != NULL) lives_tool_button_set_label_widget(LIVES_TOOL_BUTTON(tbutton), widget_opts.last_label);
  if (widget_opts.apply_theme) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tbutton), LIVES_WIDGET_NOTIFY_SIGNAL "sensitive",
                                    LIVES_GUI_CALLBACK(widget_state_cb),
                                    NULL);
    widget_state_cb(LIVES_WIDGET_OBJECT(tbutton), NULL, NULL);
  }
  if (tooltips != NULL) lives_widget_set_tooltip_text(LIVES_WIDGET(tbutton), tooltips);
  if (bar != NULL) lives_toolbar_insert(bar, tbutton, -1);
  return LIVES_WIDGET(tbutton);
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_accel_path_disconnect(LiVESAccelGroup *group, const char *path) {
#ifdef GUI_GTK
  GtkAccelKey key;
  gtk_accel_map_lookup_entry(path, &key);
  gtk_accel_group_disconnect_key(group, key.accel_key, key.accel_mods);
  return TRUE;
#endif
  return FALSE;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t lives_rgba_col_new(int red, int green, int blue, int alpha) {
  lives_colRGBA64_t lcol = {red, green, blue, alpha};
  /* lcol.red = red; */
  /* lcol.green = green; */
  /* lcol.blue = blue; */
  /* lcol.alpha = alpha; */
  return lcol;
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t *widget_color_to_lives_rgba(lives_colRGBA64_t *lcolor, LiVESWidgetColor *color) {
  lcolor->red = LIVES_WIDGET_COLOR_STRETCH(color->red);
  lcolor->green = LIVES_WIDGET_COLOR_STRETCH(color->green);
  lcolor->blue = LIVES_WIDGET_COLOR_STRETCH(color->blue);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  lcolor->alpha = LIVES_WIDGET_COLOR_STRETCH(color->alpha);
#else
  lcolor->alpha = 65535;
#endif
  return lcolor;
}


WIDGET_HELPER_GLOBAL_INLINE LiVESWidgetColor *lives_rgba_to_widget_color(LiVESWidgetColor *color, lives_colRGBA64_t *lcolor) {
  color->red = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->red);
  color->green = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->green);
  color->blue = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->blue);
#if LIVES_WIDGET_COLOR_HAS_ALPHA
  color->alpha = LIVES_WIDGET_COLOR_SCALE_65535(lcolor->alpha);
#endif
  return color;
}


WIDGET_HELPER_GLOBAL_INLINE boolean lives_rgba_equal(lives_colRGBA64_t *col1, lives_colRGBA64_t *col2) {
  return lives_memcmp(col1, col2, sizeof(lives_colRGBA64_t));
}


WIDGET_HELPER_GLOBAL_INLINE lives_colRGBA64_t *lives_rgba_copy(lives_colRGBA64_t *col1, lives_colRGBA64_t *col2) {
  col1->red = col2->red;
  col1->green = col2->green;
  col1->blue = col2->blue;
  col1->alpha = col2->alpha;
  return col1;
}


LiVESList *get_textsizes_list(void) {
  LiVESList *textsize_list = NULL;
#ifdef GUI_GTK
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Extra extra small")));
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Extra small")));
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Small")));
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Medium")));
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Large")));
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Extra large")));
  textsize_list = lives_list_append(textsize_list, lives_strdup(_("Extra extra large")));
#endif
  return textsize_list;
}


const char *lives_textsize_to_string(int val) {
  switch (val) {
  case 0:
    return LIVES_FONT_SIZE_XX_SMALL;
  case 1:
    return LIVES_FONT_SIZE_X_SMALL;
  case 2:
    return LIVES_FONT_SIZE_SMALL;
  case 3:
    return LIVES_FONT_SIZE_MEDIUM;
  case 4:
    return LIVES_FONT_SIZE_LARGE;
  case 5:
    return LIVES_FONT_SIZE_X_LARGE;
  case 6:
    return LIVES_FONT_SIZE_XX_LARGE;
  default:
    return LIVES_FONT_SIZE_NORMAL;
  }
}
