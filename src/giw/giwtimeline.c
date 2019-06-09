/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

// giwtimeline.c (c) 2013 - 2019 G. Finch salsaman+lives@gmail.com

// TODO - has own window
// - on click, update value
// - on motion with button held, update val
// - should work exaclt like vslider

// add button_press, button_release, motion_notify

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 0, 0)

#ifndef ROUND
#define ROUND(a) ((int)(a<0.?a-.5:a+.5))
#endif

#include <math.h>
#include <string.h>

#include "main.h"
#include "giwtimeline.h"

/**
 * SECTION: giwtimeline
 * @title: GiwTimeline
 * @short_description: A timeline widget with configurable unit and orientation.
 *
 * A timeline widget with configurable unit and orientation.
 **/

#define DEFAULT_TIMELINE_FONT_SCALE  PANGO_SCALE_SMALL
#define MINIMUM_INCR              5
#define DEFAULT_MAX_SIZE 100000000.

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_UNIT,
  PROP_MAX_SIZE
};

/* All distances below are in 1/72nd's of an inch. (According to
 * Adobe that's a point, but points are really 1/72.27 in.)
 */

static const struct {
  const gdouble  timeline_scale[16];
  const gint     subdivide[5];
} timeline_metric = {
  { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000 },
  { 1, 5, 10, 50, 100 }
};

//static void          giw_timeline_dispose(GObject        *object);
static void          giw_timeline_set_property(GObject        *object,
    guint            prop_id,
    const GValue   *value,
    GParamSpec     *pspec);
static void          giw_timeline_get_property(GObject        *object,
    guint           prop_id,
    GValue         *value,
    GParamSpec     *pspec);

static void          giw_timeline_realize(GtkWidget      *widget);
static void          giw_timeline_unrealize(GtkWidget      *widget);
static void          giw_timeline_map(GtkWidget      *widget);
static void          giw_timeline_unmap(GtkWidget      *widget);
static void          giw_timeline_size_allocate(GtkWidget      *widget,
    GtkAllocation  *allocation);
static void          giw_timeline_get_preferred_width(GtkWidget      *widget,
    gint           *minimum_width,
    gint           *natural_width);
static void          giw_timeline_get_preferred_height(GtkWidget      *widget,
    gint           *minimum_height,
    gint           *natural_height);
static void          giw_timeline_style_updated(GtkWidget      *widget);
static gboolean      giw_timeline_draw(GtkWidget      *widget,
                                       cairo_t        *cr);

static void          giw_timeline_draw_ticks(GiwTimeline      *timeline);
static void          giw_timeline_draw_pos(GiwTimeline      *timeline);
static void          giw_timeline_make_pixmap(GiwTimeline      *timeline);

static PangoLayout *giw_timeline_get_layout(GtkWidget      *widget,
    const gchar    *text);

static void giw_timeline_style_set(GtkWidget *widget, GtkStyle *previous_style);

static boolean giw_timeline_button_press(GtkWidget *widget, GdkEventButton *event);
static gboolean giw_timeline_button_release(GtkWidget *widget, GdkEventButton *event);
static gboolean giw_timeline_motion_notify(GtkWidget *widget, GdkEventMotion *event);

static void giw_timeline_adjustment_changed(GtkAdjustment *adjustment, gpointer data);
static void giw_timeline_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

G_DEFINE_TYPE(GiwTimeline, giw_timeline, GTK_TYPE_SCALE)


#define parent_class giw_timeline_parent_class

static void giw_timeline_class_init(GiwTimelineClass *klass) {
  GObjectClass   *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  //object_class->dispose              = giw_timeline_dispose;
  object_class->set_property         = giw_timeline_set_property;
  object_class->get_property         = giw_timeline_get_property;

  widget_class->realize              = giw_timeline_realize;
  widget_class->unrealize            = giw_timeline_unrealize;
  widget_class->map                  = giw_timeline_map;
  widget_class->unmap                = giw_timeline_unmap;
  widget_class->get_preferred_width  = giw_timeline_get_preferred_width;
  widget_class->get_preferred_height = giw_timeline_get_preferred_height;
  widget_class->size_allocate        = giw_timeline_size_allocate;
  widget_class->style_updated        = giw_timeline_style_updated;
  widget_class->draw                 = giw_timeline_draw;
  widget_class->button_press_event = giw_timeline_button_press;
  widget_class->button_release_event = giw_timeline_button_release;
  widget_class->motion_notify_event = giw_timeline_motion_notify;
  widget_class->style_set = giw_timeline_style_set;

#ifndef GTK_PARAM_READABLE
#define GTK_PARAM_READABLE (GParamFlags)(G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB)
#define GTK_PARAM_WRITABLE (GParamFlags)(G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB)
#define GTK_PARAM_READWRITE (GParamFlags)(G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB)
#endif

  g_object_class_install_property(object_class,
                                  PROP_ORIENTATION,
                                  g_param_spec_enum("orientation",
                                      "Orientation",
                                      "The orientation of the timeline",
                                      GTK_TYPE_ORIENTATION,
                                      GTK_ORIENTATION_HORIZONTAL,
                                      GTK_PARAM_READWRITE));

  g_object_class_install_property(object_class,
                                  PROP_MAX_SIZE,
                                  g_param_spec_double("max-size",
                                      "Max Size",
                                      "Maximum size of the timeline",
                                      -G_MAXDOUBLE,
                                      G_MAXDOUBLE,
                                      0.0,
                                      GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property(widget_class,
                                          g_param_spec_double("font-scale",
                                              NULL, NULL,
                                              0.0,
                                              G_MAXDOUBLE,
                                              DEFAULT_TIMELINE_FONT_SCALE,
                                              GTK_PARAM_READABLE));
}


static void giw_timeline_init(GiwTimeline *timeline) {
  gtk_widget_set_has_window(GTK_WIDGET(timeline), TRUE);

  timeline->orientation   = GTK_ORIENTATION_HORIZONTAL;
  timeline->unit          = GIW_TIME_UNIT_SECONDS;
  timeline->max_size      = DEFAULT_MAX_SIZE;
  timeline->backing_store = NULL;
  timeline->font_scale    = DEFAULT_TIMELINE_FONT_SCALE;
  timeline->button       = 0;
  // Default mouse policy : automatic
  timeline->mouse_policy = GIW_TIMELINE_MOUSE_AUTOMATIC;
}


/*static void giw_timeline_dispose(GObject *object) {
  GiwTimeline        *timeline = GIW_TIMELINE(object);

  while (timeline->track_widgets)
    giw_timeline_remove_track_widget(timeline, (GtkWidget *)timeline->track_widgets->data);

  G_OBJECT_CLASS(parent_class)->dispose(object);
  }*/


static void giw_timeline_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  GiwTimeline        *timeline = GIW_TIMELINE(object);

  switch (prop_id) {
  case PROP_ORIENTATION:
    timeline->orientation = (GtkOrientation)g_value_get_enum(value);
    gtk_widget_queue_resize(GTK_WIDGET(timeline));
    break;

  case PROP_UNIT:
    giw_timeline_set_unit(timeline, (GiwTimeUnit)g_value_get_int(value));
    break;

  case PROP_MAX_SIZE:
    giw_timeline_set_max_size(timeline,
                              g_value_get_double(value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}


static void giw_timeline_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  GiwTimeline        *timeline = GIW_TIMELINE(object);

  switch (prop_id) {
  case PROP_ORIENTATION:
    g_value_set_enum(value, timeline->orientation);
    break;

  case PROP_UNIT:
    g_value_set_int(value, timeline->unit);
    break;

  case PROP_MAX_SIZE:
    g_value_set_double(value, timeline->max_size);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}


/**
 * giw_timeline_new:
 * @orientation: the timeline's orientation.
 *
 * Creates a new timeline.
 *
 * Return value: a new #GiwTimeline widget.
 *
 **/
GtkWidget *giw_timeline_new(GtkOrientation orientation, GtkAdjustment *adjustment) {
  GiwTimeline *timeline;

  timeline = (GiwTimeline *)g_object_new(GIW_TYPE_TIMELINE,
                                         "orientation", orientation,
                                         NULL);

  if (adjustment != NULL) giw_timeline_set_adjustment(timeline, adjustment);
  return GTK_WIDGET(timeline);
}


GtkWidget *giw_timeline_new_with_adjustment(GtkOrientation orientation, gdouble value, gdouble lower, gdouble upper, gdouble max_size) {
  GiwTimeline *timeline;

  timeline = (GiwTimeline *)g_object_new(GIW_TYPE_TIMELINE,
                                         "orientation", orientation,
                                         NULL);
  giw_timeline_set_adjustment(timeline, (GtkAdjustment *) gtk_adjustment_new(value, lower, upper, 1.0, 1.0, 1.0));
  giw_timeline_set_max_size(timeline, max_size);

  return GTK_WIDGET(timeline);
}


void giw_timeline_set_adjustment(GiwTimeline *timeline, GtkAdjustment *adjustment) {
  g_return_if_fail(timeline != NULL);
  g_return_if_fail(GIW_IS_TIMELINE(timeline));
  g_return_if_fail(adjustment != NULL);

  // Freeing the last one
  if (timeline->adjustment) {
#if GTK_CHECK_VERSION(3, 0, 0)
    g_signal_handler_disconnect((gpointer)(timeline->adjustment), timeline->chsig);
    g_signal_handler_disconnect((gpointer)(timeline->adjustment), timeline->vchsig);
#else
    gtk_signal_disconnect_by_data(LIVES_GUI_OBJECT(timeline->adjustment), (gpointer) timeline);
#endif
    g_object_unref(G_OBJECT(timeline->adjustment));
    timeline->adjustment = NULL;
  }

  timeline->adjustment = adjustment;
  g_object_ref(LIVES_GUI_OBJECT(timeline->adjustment));

  timeline->chsig = g_signal_connect(LIVES_GUI_OBJECT(adjustment), "changed",
                                     (GCallback) giw_timeline_adjustment_changed,
                                     (gpointer) timeline);

  timeline->vchsig = g_signal_connect(LIVES_GUI_OBJECT(adjustment), "value_changed",
                                      (GCallback) giw_timeline_adjustment_value_changed,
                                      (gpointer) timeline);

#if !GTK_CHECK_VERSION(3,18,0)
  gtk_adjustment_value_changed(timeline->adjustment);
  gtk_adjustment_changed(timeline->adjustment);
#endif
}


void giw_timeline_set_mouse_policy(GiwTimeline *timeline, GiwTimelineMousePolicy policy) {
  g_return_if_fail(timeline != NULL);
  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  timeline->mouse_policy = policy;
}


/**
 * giw_timeline_add_track_widget:
 * @timeline: a #GiwTimeline
 * @widget: the track widget to add
 *
 * Adds a "track widget" to the timeline. The timeline will connect to
 * GtkWidget:motion-notify-event: on the track widget and update its
 * position marker accordingly. The marker is correctly updated also
 * for the track widget's children, regardless of whether they are
 * ordinary children of off-screen children.
 *
 */
void giw_timeline_add_track_widget(GiwTimeline *timeline, GtkWidget *widget) {

  g_return_if_fail(GIW_IS_TIMELINE(timeline));
  g_return_if_fail(GTK_IS_WIDGET(timeline));

  g_return_if_fail(g_list_find(timeline->track_widgets, widget) == NULL);

  timeline->track_widgets = g_list_prepend(timeline->track_widgets, widget);

  g_signal_connect_swapped(widget, "destroy",
                           G_CALLBACK(giw_timeline_remove_track_widget),
                           timeline);
}

/**
 * giw_timeline_remove_track_widget:
 * @timeline: a #GiwTimeline
 * @widget: the track widget to remove
 *
 * Removes a previously added track widget from the timeline. See
 * giw_timeline_add_track_widget().
 *
 */
void giw_timeline_remove_track_widget(GiwTimeline *timeline, GtkWidget *widget) {

  g_return_if_fail(GIW_IS_TIMELINE(timeline));
  g_return_if_fail(GTK_IS_WIDGET(timeline));

  g_return_if_fail(g_list_find(timeline->track_widgets, widget) != NULL);

  timeline->track_widgets = g_list_remove(timeline->track_widgets, widget);

  g_signal_handlers_disconnect_by_func(widget,
                                       (gpointer)giw_timeline_remove_track_widget,
                                       timeline);
}

/**
 * giw_timeline_set_unit:
 * @timeline: a #GiwTimeline
 * @unit:  the #GiwTimeUnit to set the timeline to
 *
 * This sets the unit of the timeline.
 *
 */


void giw_timeline_set_unit(GiwTimeline *timeline, GiwTimeUnit unit) {

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  if (timeline->unit != unit) {
    timeline->unit = unit;
    g_object_notify(G_OBJECT(timeline), "unit");

    gtk_widget_queue_draw(GTK_WIDGET(timeline));
  }
}


/**
 * giw_timeline_get_unit:
 * @timeline: a #GiwTimeline
 *
 * Return value: the unit currently used in the @timeline widget.
 *
 **/
GiwTimeUnit giw_timeline_get_unit(GiwTimeline *timeline) {
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), (GiwTimeUnit)0);

  return timeline->unit;
}


/**
 * giw_timeline_get_position:
 * @timeline: a #GiwTimeline
 *
 * Return value: the current position of the @timeline widget.
 *
 * Since: GIW 2.8
 **/
static gdouble giw_timeline_get_position(GiwTimeline *timeline) {
  GtkWidget *widget;
  GtkRange *range;
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), 0.0);

  widget = GTK_WIDGET(timeline);
  range = GTK_RANGE(widget);

  return gtk_range_get_value(range);
}

/**
 * giw_timeline_set_max_size:
 * @timeline: a #GiwTimeline
 * @max_size: the maximum size of the timeline used when calculating the space to
 * leave for the text
 *
 * This sets the max_size of the timeline.
 *
 */
void giw_timeline_set_max_size(GiwTimeline *timeline,  gdouble max_size) {

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  g_object_freeze_notify(G_OBJECT(timeline));
  if (timeline->max_size != max_size) {
    timeline->max_size = max_size;
    g_object_notify(G_OBJECT(timeline), "max-size");
  }
  g_object_thaw_notify(G_OBJECT(timeline));

  gtk_widget_queue_draw(GTK_WIDGET(timeline));
}


gdouble giw_timeline_get_max_size(GiwTimeline *timeline) {
  GtkAdjustment *adjustment;
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), 0.0);
  adjustment = gtk_range_get_adjustment(GTK_RANGE(timeline));
  return gtk_adjustment_get_upper(adjustment);
}


/**
 * giw_timeline_get_range:
 * @timeline: a #GiwTimeline
 * @lower: location to store lower limit of the timeline, or %NULL
 * @upper: location to store upper limit of the timeline, or %NULL
 * @max_size: location to store the maximum size of the timeline used when
 *            calculating the space to leave for the text, or %NULL.
 *
 * Retrieves values indicating the range and current position of a #GiwTimeline.
 * See giw_timeline_set_range().
 *
 **/
static void giw_timeline_get_range(GiwTimeline *timeline, gdouble *lower, gdouble *upper, gdouble *max_size) {
  GtkAdjustment *adj;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  adj = timeline->adjustment;
  if (lower)
    *lower = gtk_adjustment_get_lower(adj);
  if (upper)
    *upper = gtk_adjustment_get_upper(adj);
  if (max_size)
    *max_size = timeline->max_size;
}


void giw_timeline_set_range(GiwTimeline *timeline, gdouble lower, gdouble upper, gdouble max_size) {
  GtkAdjustment *adj;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  adj = timeline->adjustment;
  gtk_adjustment_set_lower(adj, lower);
  gtk_adjustment_set_upper(adj, upper);
  giw_timeline_set_max_size(timeline, max_size);
}


static void giw_timeline_realize(GtkWidget *widget) {
  GtkStyleContext *stylecon;
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GtkAllocation     allocation;
  GdkWindowAttr     attributes;
  gint              attributes_mask;

  GTK_WIDGET_CLASS(giw_timeline_parent_class)->realize(widget);

  gtk_widget_get_allocation(widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x           = allocation.x;
  attributes.y           = allocation.y;
  attributes.width       = allocation.width;
  attributes.height      = allocation.height;
  attributes.wclass      = GDK_INPUT_OUTPUT;
  attributes.event_mask = gtk_widget_get_events(widget) |
                          GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  gtk_widget_set_window(widget, (gdk_window_new(gtk_widget_get_window(gtk_widget_get_parent(widget)), &attributes, attributes_mask)));

  stylecon = gtk_style_context_new();
  gtk_style_context_set_path(stylecon, gtk_widget_get_path(widget));
  gtk_style_context_set_state(stylecon, GTK_STATE_FLAG_ACTIVE);

  gdk_window_set_user_data(gtk_widget_get_window(GTK_WIDGET(timeline)), timeline);
  g_object_ref(LIVES_GUI_OBJECT(gtk_widget_get_window(GTK_WIDGET(timeline))));

  giw_timeline_make_pixmap(timeline);
}


static void giw_timeline_unrealize(GtkWidget *widget) {
  GiwTimeline *timeline = GIW_TIMELINE(widget);

  if (timeline->backing_store) {
    cairo_surface_destroy(timeline->backing_store);
    timeline->backing_store = NULL;
  }

  if (timeline->layout) {
    g_object_unref(timeline->layout);
    timeline->layout = NULL;
  }

  /* if (gtk_widget_get_window(GTK_WIDGET(timeline)) != NULL) { */
  /*   gdk_window_destroy(gtk_widget_get_window(GTK_WIDGET(timeline))); */
  /*   gtk_widget_set_window(GTK_WIDGET(timeline), NULL); */
  /* } */

  GTK_WIDGET_CLASS(giw_timeline_parent_class)->unrealize(widget);
}


static void giw_timeline_map(GtkWidget *widget) {
  GTK_WIDGET_CLASS(parent_class)->map(widget);

  if (gtk_widget_get_window(widget) != NULL)
    gdk_window_show(gtk_widget_get_window(widget));
}


static void giw_timeline_unmap(GtkWidget *widget) {

  if (gtk_widget_get_window(widget) != NULL)
    gdk_window_hide(gtk_widget_get_window(widget));

  GTK_WIDGET_CLASS(parent_class)->unmap(widget);
}


static void giw_timeline_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GtkAllocation     widget_allocation;
  gboolean          resized;

  gtk_widget_get_allocation(widget, &widget_allocation);

  resized = (widget_allocation.width  != allocation->width ||
             widget_allocation.height != allocation->height);

  gtk_widget_set_allocation(widget, allocation);

  if (gtk_widget_get_realized(widget)) {
    gdk_window_move_resize(gtk_widget_get_window(widget),
                           allocation->x, allocation->y,
                           allocation->width, allocation->height);

    if (resized)
      giw_timeline_make_pixmap(timeline);
  }
}


static void giw_timeline_size_request(GtkWidget *widget, GtkRequisition *requisition) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GtkStyleContext  *context = gtk_widget_get_style_context(widget);
  PangoLayout      *layout;
  PangoRectangle    ink_rect;
  GtkBorder         border;
  gint              size;

  layout = giw_timeline_get_layout(widget, "0123456789");
  pango_layout_get_pixel_extents(layout, &ink_rect, NULL);

  size = 2 + ink_rect.height * 1.7;

  gtk_style_context_get_border(context, gtk_widget_get_state_flags(widget), &border);

  requisition->width  = border.left + border.right;
  requisition->height = border.top + border.bottom;

  if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
    requisition->width  += 1;
    requisition->height += size;
  } else {
    requisition->width  += size;
    requisition->height += 1;
  }
}


static void giw_timeline_get_preferred_width(GtkWidget *widget, gint *minimum_width, gint *natural_width) {
  GtkRequisition requisition;

  giw_timeline_size_request(widget, &requisition);

  *minimum_width = *natural_width = requisition.width;
}


static void giw_timeline_get_preferred_height(GtkWidget *widget, gint *minimum_height, gint *natural_height) {
  GtkRequisition requisition;

  giw_timeline_size_request(widget, &requisition);

  *minimum_height = *natural_height = requisition.height;
}


static void giw_timeline_style_updated(GtkWidget *widget) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GTK_WIDGET_CLASS(giw_timeline_parent_class)->style_updated(widget);

  gtk_widget_style_get(widget,
                       "font-scale", &timeline->font_scale,
                       NULL);

  if (timeline->layout) {
    g_object_unref(timeline->layout);
    timeline->layout = NULL;
  }
}


static void giw_timeline_style_set(GtkWidget *widget, GtkStyle *previous_style) {
  giw_timeline_style_updated(widget);
}


static gboolean giw_timeline_draw(GtkWidget *widget, cairo_t *cr) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);

  giw_timeline_draw_ticks(timeline);

  cairo_set_source_surface(cr, timeline->backing_store, 0, 0);
  cairo_paint(cr);

  giw_timeline_draw_pos(timeline);

  return FALSE;
}


static void giw_timeline_draw_ticks(GiwTimeline *timeline) {
  GtkWidget        *widget  = GTK_WIDGET(timeline);
  GtkStyleContext  *context = gtk_widget_get_style_context(widget);
  GtkAllocation     allocation;
  GtkBorder         border;
  GdkRGBA           color;
  cairo_t          *cr;
  gint              i;
  gint              width, height;
  gint              length, ideal_length;
  gdouble           lower, upper;  /* Upper and lower limits, in timeline units */
  gdouble           increment;     /* Number of pixels per unit */
  gint              scale;         /* Number of units per major unit */

  gint scaleh, scalem, scales;

  gint curi, curh, curm, curs;
  gdouble           start, end, cur;
  gchar             unit_str[32];
  gint              digit_height;
  gint              digit_offset;
  gint              text_size;
  gint              pos;
  gdouble           max_size;
  //GiwTimeUnit       unit;
  PangoLayout      *layout;
  PangoRectangle    logical_rect, ink_rect;

  if (! gtk_widget_is_drawable(widget)) return;

  gtk_widget_get_allocation(widget, &allocation);
  gtk_style_context_get_border(context, gtk_widget_get_state_flags(widget), &border);

  layout = giw_timeline_get_layout(widget, "0123456789");
  pango_layout_get_extents(layout, &ink_rect, &logical_rect);

  digit_height = PANGO_PIXELS(ink_rect.height) + 2;
  digit_offset = ink_rect.y;

  if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
    width  = allocation.width;
    height = allocation.height - (border.top + border.bottom);
  } else {
    width  = allocation.height;
    height = allocation.width - (border.top + border.bottom);
  }

  cr = cairo_create(timeline->backing_store);

  gtk_render_background(context, cr, 0., 0., allocation.width, allocation.height);

  gtk_render_frame(context, cr, 0, 0, allocation.width, allocation.height);

  gtk_style_context_get_color(context, gtk_widget_get_state_flags(widget),
                              &color);
  gdk_cairo_set_source_rgba(cr, &color);

  if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
    cairo_rectangle(cr,
                    border.left,
                    height - border.bottom,
                    allocation.width - (border.left + border.right),
                    1);
  } else {
    cairo_rectangle(cr,
                    border.left,
                    border.top,
                    1,
                    allocation.height - (border.top + border.bottom));
  }
  cairo_stroke(cr);

  giw_timeline_get_range(timeline, &lower, &upper, &max_size);

  if ((upper - lower) == 0)
    goto out;

  increment = (gdouble) width / (upper - lower);

  /* determine the scale
   *   use the maximum extents of the timeline to determine the largest
   *   possible number to be displayed.  Calculate the height in pixels
   *   of this displayed text. Use this height to find a scale which
   *   leaves sufficient room for drawing the timeline.
   */
  scale = ceil(max_size);

  if (timeline->unit == GIW_TIME_UNIT_SECONDS) {
    g_snprintf(unit_str, sizeof(unit_str), "%d", scale);
  } else {
    scaleh = (int)((double)scale / 3600.);
    scalem = (int)((double)(scale - scaleh * 3600) / 60.);
    scales = scale - scaleh * 3600 - scalem * 60;

    if (scale < 0) {
      scalem = -scalem;
      scales = -scales;
    }

    g_snprintf(unit_str, sizeof(unit_str), "%02d:%02d:%02d", scaleh, scalem, scales);
  }

  text_size = strlen(unit_str) * digit_height + 1;

  for (scale = 0; scale < G_N_ELEMENTS(timeline_metric.timeline_scale); scale++)
    if (timeline_metric.timeline_scale[scale] * fabs(increment) > 2 * text_size)
      break;

  if (scale == G_N_ELEMENTS(timeline_metric.timeline_scale))
    scale = G_N_ELEMENTS(timeline_metric.timeline_scale) - 1;

  //unit = giw_timeline_get_unit (timeline);
  //unit=GIW_TIME_UNIT_SECONDS;

  /* drawing starts here */
  length = 0;
  for (i = G_N_ELEMENTS(timeline_metric.subdivide) - 1; i >= 0; i--) {
    gdouble subd_incr;

    /* hack to get proper subdivisions at full pixels */
    if (scale == 1 && i == 1)
      subd_incr = 1.0;
    else
      subd_incr = ((gdouble) timeline_metric.timeline_scale[scale] /
                   (gdouble) timeline_metric.subdivide[i]);

    if (subd_incr * fabs(increment) <= MINIMUM_INCR)
      continue;

    /* don't subdivide pixels */
    if (subd_incr < 1.0)
      continue;

    /* Calculate the length of the tickmarks. Make sure that
     * this length increases for each set of ticks
     */
    ideal_length = height / (i + 1) - 1;
    if (ideal_length > ++length)
      length = ideal_length;

    if (lower < upper) {
      start = floor(lower / subd_incr) * subd_incr;
      end   = ceil(upper / subd_incr) * subd_incr;
    } else {
      start = floor(upper / subd_incr) * subd_incr;
      end   = ceil(lower / subd_incr) * subd_incr;
    }

    cairo_set_line_width(cr, 1.);

    for (cur = start; cur <= end; cur += subd_incr) {
      pos = ROUND((cur - lower) * increment);

      if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
        cairo_rectangle(cr,
                        pos, height + border.top - length,
                        1,   length);
      } else {
        cairo_rectangle(cr,
                        height + border.left - length, pos,
                        length,                        1);
      }
      cairo_stroke(cr);

      /* draw label */
      if (i == 0) {
        curi = (int)cur;
        if (timeline->unit == GIW_TIME_UNIT_SECONDS) {
          g_snprintf(unit_str, sizeof(unit_str), "%d", curi);
        } else {
          curh = (int)(cur / 3600.);
          curm = (int)((double)(curi - curh * 3600) / 60.);
          curs = curi - curh * 3600 - curm * 60;

          if (curi < 0) {
            curm = -curm;
            curs = -curs;
          }

          g_snprintf(unit_str, sizeof(unit_str), "%02d:%02d:%02d", curh, curm, curs);
        }

        if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
          pango_layout_set_text(layout, unit_str, -1);
          pango_layout_get_extents(layout, &logical_rect, NULL);

          cairo_move_to(cr,
                        pos + 2,
                        border.top + PANGO_PIXELS(logical_rect.y - digit_offset));
          pango_cairo_show_layout(cr, layout);
        } else {
          gint j;

          for (j = 0; j < (int) strlen(unit_str); j++) {
            pango_layout_set_text(layout, unit_str + j, 1);
            pango_layout_get_extents(layout, NULL, &logical_rect);

            cairo_move_to(cr,
                          border.left + 1,
                          pos + digit_height * j + 2 + PANGO_PIXELS(logical_rect.y - digit_offset));
            pango_cairo_show_layout(cr, layout);
          }
        }
      }
    }
  }

  cairo_fill(cr);
out:
  cairo_destroy(cr);
}


/** This is supposed to draw the timeline pointer,
    it used to work at one point but now it has stopped working.
*/
static void giw_timeline_draw_pos(GiwTimeline *timeline) {
  GtkWidget        *widget  = GTK_WIDGET(timeline);
  GtkStyleContext  *context = gtk_widget_get_style_context(widget);
  GtkAllocation     allocation;
  GtkBorder         border;
  GdkRGBA           color;
  gint              x, y;
  gint              width, height;
  gint              bs_width, bs_height;

  if (!gtk_widget_is_drawable(widget)) return;

  gtk_widget_get_allocation(widget, &allocation);
  gtk_style_context_get_border(context, gtk_widget_get_state_flags(widget), &border);

  if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
    width  = allocation.width;
    height = allocation.height - (border.top + border.bottom);

    bs_width = height / 2 + 2;
    bs_width |= 1;  /* make sure it's odd */
    bs_height = bs_width / 2 + 1;
  } else {
    width  = allocation.width - (border.left + border.right);
    height = allocation.height;

    bs_height = width / 2 + 2;
    bs_height |= 1;  /* make sure it's odd */
    bs_width = bs_height / 2 + 1;
  }

  if ((bs_width > 0) && (bs_height > 0)) {
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
    gdouble  lower;
    gdouble  upper;
    gdouble  position;
    gdouble  increment;

    cairo_rectangle(cr,
                    allocation.x, allocation.y,
                    allocation.width, allocation.height);
    cairo_clip(cr);

    cairo_translate(cr, allocation.x, allocation.y);

    //If a backing store exists, restore the timeline
    if (timeline->backing_store) {
      cairo_set_source_surface(cr, timeline->backing_store, 0, 0);
      cairo_rectangle(cr, timeline->xsrc, timeline->ysrc, bs_width, bs_height);
      cairo_fill(cr);
    }

    cairo_new_path(cr);

    position = giw_timeline_get_position(timeline);

    giw_timeline_get_range(timeline, &lower, &upper, NULL);

    if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
      increment = (gdouble) width / (upper - lower);

      x = ROUND((position - lower) * increment) + (border.left - bs_width) / 2 - 1;
      y = (height + bs_height) / 2 + border.top;
    } else {
      increment = (gdouble) height / (upper - lower);

      x = (width + bs_width) / 2 + border.left;
      y = ROUND((position - lower) * increment) + (border.top - bs_height) / 2 - 1;
    }

    gtk_style_context_get_color(context, gtk_widget_get_state_flags(widget),
                                &color);

    gdk_cairo_set_source_rgba(cr, &color);

    cairo_move_to(cr, x, y);

    if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
      cairo_line_to(cr, x + bs_width / 2.0, y + bs_height);
      cairo_line_to(cr, x + bs_width,       y);
    } else {
      cairo_line_to(cr, x + bs_width, y + bs_height / 2.0);
      cairo_line_to(cr, x,            y + bs_height);
    }

    cairo_fill(cr);

    cairo_destroy(cr);

    timeline->xsrc = x;
    timeline->ysrc = y;
  }
}


static void giw_timeline_make_pixmap(GiwTimeline *timeline) {
  GtkWidget        *widget = GTK_WIDGET(timeline);
  GtkAllocation     allocation;

  gtk_widget_get_allocation(widget, &allocation);

  if (timeline->backing_store)
    cairo_surface_destroy(timeline->backing_store);

  timeline->backing_store =
    gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                      CAIRO_CONTENT_COLOR,
                                      allocation.width,
                                      allocation.height);
}


static PangoLayout *giw_timeline_create_layout(GtkWidget *widget, const gchar *text) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  PangoLayout      *layout;
  PangoAttrList    *attrs;
  PangoAttribute   *attr;

  layout = gtk_widget_create_pango_layout(widget, text);

  attrs = pango_attr_list_new();

  attr = pango_attr_scale_new(timeline->font_scale);
  attr->start_index = 0;
  attr->end_index   = -1;
  pango_attr_list_insert(attrs, attr);

  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);

  return layout;
}


static PangoLayout *giw_timeline_get_layout(GtkWidget *widget, const gchar *text) {
  GiwTimeline *timeline = GIW_TIMELINE(widget);

  if (timeline->layout) {
    pango_layout_set_text(timeline->layout, text, -1);
    return timeline->layout;
  }

  timeline->layout = giw_timeline_create_layout(widget, text);

  return timeline->layout;
}


void giw_timeline_set_value(GiwTimeline *timeline, gdouble value) {
  g_return_if_fail(timeline != NULL);
  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  if (gtk_adjustment_get_value(timeline->adjustment) != value) {
    gtk_adjustment_set_value(timeline->adjustment, value);

#if !GTK_CHECK_VERSION(3,18,0)
    gtk_adjustment_value_changed(timeline->adjustment);
#endif
  }
}


gdouble giw_timeline_get_value(GiwTimeline *timeline) {
  g_return_val_if_fail(timeline != NULL, 0.0);
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), 0.0);
  g_return_val_if_fail(timeline->adjustment != NULL, 0.0);

  return (gtk_adjustment_get_value(timeline->adjustment));
}


GtkAdjustment *giw_timeline_get_adjustment(GiwTimeline *timeline) {
  g_return_val_if_fail(timeline != NULL, NULL);
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), NULL);

  return (timeline->adjustment);
}


static gboolean giw_timeline_button_press(GtkWidget *widget, GdkEventButton *event) {
  GiwTimeline *timeline;
  GtkAllocation allocation;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GIW_IS_TIMELINE(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  timeline = GIW_TIMELINE(widget);
  if (timeline->mouse_policy == GIW_TIMELINE_MOUSE_DISABLED) return (FALSE);

  gtk_widget_get_allocation(widget, &allocation);

  if (event->x < 0 || event->x >= allocation.width || event->y <
      0 || event->y >= allocation.height) return FALSE;

  if (event->button == 1) {
    GtkAdjustment *adjustment = giw_timeline_get_adjustment(timeline);
    gdouble new_value;
    gint width, pos;

    timeline->button = event->button;

    if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
      width  = allocation.width;
    } else {
      width  = allocation.height;
    }

    if (timeline->orientation == GTK_ORIENTATION_HORIZONTAL) {
      pos = event->x;
    } else {
      pos = event->y;
    }

    new_value = gtk_adjustment_get_lower(adjustment) + (gdouble)pos / (gdouble)width *
                (gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_lower(adjustment));
    if ((new_value <= gtk_adjustment_get_upper(adjustment)) &&
        (new_value >= gtk_adjustment_get_lower(adjustment)))
      giw_timeline_set_value(timeline, new_value);

    gtk_widget_queue_draw(GTK_WIDGET(timeline));
  }

  return TRUE;
}


static gboolean giw_timeline_button_release(GtkWidget *widget, GdkEventButton *event) {
  GiwTimeline *timeline;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GIW_IS_TIMELINE(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  timeline = GIW_TIMELINE(widget);

  if (timeline->mouse_policy == GIW_TIMELINE_MOUSE_DISABLED) return (FALSE);

  if (timeline->button == event->button) {
    giw_timeline_button_press(widget, event);
    timeline->button = 0;
  }

  return TRUE;
}


static gboolean giw_timeline_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
  GdkEventButton *ebutton = (GdkEventButton *)event;
  GdkDevice *device;
  GdkModifierType mask;
  GiwTimeline *timeline;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GIW_IS_TIMELINE(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  timeline = GIW_TIMELINE(widget);
  if (timeline->mouse_policy == GIW_TIMELINE_MOUSE_DISABLED) return (FALSE);
  if (timeline->button == 0) return TRUE;

  device = event->device;
  gdk_device_get_state(device, gtk_widget_get_window(widget), NULL, &mask);
  if (!(mask & GDK_BUTTON1_MASK)) {
    // handle the situation where the button release happened outside the window boundary
    timeline->button = 0;
    return TRUE;
  }

  ebutton->button = timeline->button;
  ebutton->y = 0.;
  giw_timeline_button_press(widget, ebutton);
  return TRUE;
}


static void giw_timeline_adjustment_changed(GtkAdjustment *adjustment, gpointer data) {
  GtkWidget *timeline;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);
  g_return_if_fail(GIW_IS_TIMELINE(data));

  timeline = GTK_WIDGET(data);
  gtk_widget_queue_draw(timeline);
}


static void giw_timeline_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data) {
  GiwTimeline *timeline;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);
  g_return_if_fail(GIW_IS_TIMELINE(data));

  timeline = GIW_TIMELINE(data);
  giw_timeline_draw_pos(timeline);
}

#endif // #if GTK_CHECK_VERSION(3, 0, 0)
