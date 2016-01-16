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

// giwtimeline.c (c) 2013 - 2014 G. Finch salsaman@gmail.com

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3,0,0)

#ifndef ROUND
#define ROUND(a) ((int)(a<0.?a-.5:a+.5))
#endif

#include <math.h>
#include <string.h>

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
typedef struct {
  GtkOrientation   orientation;
  GiwTimeUnit      unit;
  gdouble          max_size;

  GdkWindow       *input_window;
  cairo_surface_t *backing_store;
  PangoLayout     *layout;
  gdouble          font_scale;

  gint             xsrc;
  gint             ysrc;

  GList           *track_widgets;
} GiwTimelinePrivate;

#define GIW_TIMELINE_GET_PRIVATE(timeline) \
  G_TYPE_INSTANCE_GET_PRIVATE (timeline, GIW_TYPE_TIMELINE, GiwTimelinePrivate)


static const struct {
  const gdouble  timeline_scale[16];
  const gint     subdivide[5];
} timeline_metric = {
  { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000 },
  { 1, 5, 10, 50, 100 }
};


static void          giw_timeline_dispose(GObject        *object);
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



//G_DEFINE_TYPE (GiwTimeline, giw_timeline, GTK_TYPE_WIDGET)

G_DEFINE_TYPE(GiwTimeline, giw_timeline, GTK_TYPE_SCALE);

#define parent_class giw_timeline_parent_class



static void
giw_timeline_class_init(GiwTimelineClass *klass) {
  GObjectClass   *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose              = giw_timeline_dispose;
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

  g_type_class_add_private(object_class, sizeof(GiwTimelinePrivate));

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

static void
giw_timeline_init(GiwTimeline *timeline) {
  GiwTimelinePrivate *priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  gtk_widget_set_has_window(GTK_WIDGET(timeline), FALSE);

  priv->orientation   = GTK_ORIENTATION_HORIZONTAL;
  priv->unit          = GIW_TIME_UNIT_SECONDS;
  priv->max_size      = DEFAULT_MAX_SIZE;
  priv->backing_store = NULL;
  priv->font_scale    = DEFAULT_TIMELINE_FONT_SCALE;
}

static void
giw_timeline_dispose(GObject *object) {
  GiwTimeline        *timeline = GIW_TIMELINE(object);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);

  while (priv->track_widgets)
    giw_timeline_remove_track_widget(timeline, (GtkWidget *)priv->track_widgets->data);

  G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
giw_timeline_set_property(GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec) {
  GiwTimeline        *timeline = GIW_TIMELINE(object);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);

  switch (prop_id) {
  case PROP_ORIENTATION:
    priv->orientation = (GtkOrientation)g_value_get_enum(value);
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

static void
giw_timeline_get_property(GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec) {
  GiwTimeline        *timeline = GIW_TIMELINE(object);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);

  switch (prop_id) {
  case PROP_ORIENTATION:
    g_value_set_enum(value, priv->orientation);
    break;

  case PROP_UNIT:
    g_value_set_int(value, priv->unit);
    break;

  case PROP_MAX_SIZE:
    g_value_set_double(value, priv->max_size);
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
GtkWidget *
giw_timeline_new(GtkOrientation orientation) {
  GiwTimeline *timeline;

  timeline = (GiwTimeline *)g_object_new(GIW_TYPE_TIMELINE,
                                         "orientation", orientation,
                                         NULL);
  return GTK_WIDGET(timeline);
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
void
giw_timeline_add_track_widget(GiwTimeline *timeline,
                              GtkWidget *widget) {
  GiwTimelinePrivate *priv;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));
  g_return_if_fail(GTK_IS_WIDGET(timeline));

  priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  g_return_if_fail(g_list_find(priv->track_widgets, widget) == NULL);

  priv->track_widgets = g_list_prepend(priv->track_widgets, widget);

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
void
giw_timeline_remove_track_widget(GiwTimeline *timeline,
                                 GtkWidget *widget) {
  GiwTimelinePrivate *priv;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));
  g_return_if_fail(GTK_IS_WIDGET(timeline));

  priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  g_return_if_fail(g_list_find(priv->track_widgets, widget) != NULL);

  priv->track_widgets = g_list_remove(priv->track_widgets, widget);

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


void
giw_timeline_set_unit(GiwTimeline *timeline,
                      GiwTimeUnit   unit) {
  GiwTimelinePrivate *priv;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  if (priv->unit != unit) {
    priv->unit = unit;
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

GiwTimeUnit
giw_timeline_get_unit(GiwTimeline *timeline) {
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), (GiwTimeUnit)0);

  return GIW_TIMELINE_GET_PRIVATE(timeline)->unit;
}



/**
 * giw_timeline_get_position:
 * @timeline: a #GiwTimeline
 *
 * Return value: the current position of the @timeline widget.
 *
 * Since: GIW 2.8
 **/
static gdouble
giw_timeline_get_position(GiwTimeline *timeline) {
  GtkWidget *widget;
  GtkRange *range;
  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), 0.0);

  widget=GTK_WIDGET(timeline);
  range=GTK_RANGE(widget);

  //return GIW_TIMELINE_GET_PRIVATE (timeline)->position;

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
void
giw_timeline_set_max_size(GiwTimeline *timeline,
                          gdouble    max_size) {
  GiwTimelinePrivate *priv;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  g_object_freeze_notify(G_OBJECT(timeline));
  if (priv->max_size != max_size) {
    priv->max_size = max_size;
    g_object_notify(G_OBJECT(timeline), "max-size");
  }
  g_object_thaw_notify(G_OBJECT(timeline));

  gtk_widget_queue_draw(GTK_WIDGET(timeline));
}



gdouble
giw_timeline_get_max_size(GiwTimeline *timeline) {
  GiwTimelinePrivate *priv;

  g_return_val_if_fail(GIW_IS_TIMELINE(timeline), 0.0);

  priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  return priv->max_size;
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
static void
giw_timeline_get_range(GiwTimeline *timeline,
                       gdouble   *lower,
                       gdouble   *upper,
                       gdouble   *max_size) {
  GiwTimelinePrivate *priv;
  GtkWidget *widget;
  GtkRange *range;
  GtkAdjustment *adj;

  g_return_if_fail(GIW_IS_TIMELINE(timeline));

  priv = GIW_TIMELINE_GET_PRIVATE(timeline);

  widget=GTK_WIDGET(timeline);
  range=GTK_RANGE(widget);

  adj=gtk_range_get_adjustment(range);
  if (lower)
    *lower=gtk_adjustment_get_lower(adj);
  if (upper)
    *upper=gtk_adjustment_get_upper(adj);
  if (max_size)
    *max_size=priv->max_size;

}

static void
giw_timeline_realize(GtkWidget *widget) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);
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
  attributes.wclass      = GDK_INPUT_ONLY;
  attributes.event_mask  = (gtk_widget_get_events(widget) |
                            GDK_EXPOSURE_MASK              |
                            GDK_POINTER_MOTION_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->input_window = gdk_window_new(gtk_widget_get_window(widget),
                                      &attributes, attributes_mask);
  gdk_window_set_user_data(priv->input_window, timeline);

  giw_timeline_make_pixmap(timeline);
}

static void
giw_timeline_unrealize(GtkWidget *widget) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);

  if (priv->backing_store) {
    cairo_surface_destroy(priv->backing_store);
    priv->backing_store = NULL;
  }

  if (priv->layout) {
    g_object_unref(priv->layout);
    priv->layout = NULL;
  }

  if (priv->input_window) {
    gdk_window_destroy(priv->input_window);
    priv->input_window = NULL;
  }

  GTK_WIDGET_CLASS(giw_timeline_parent_class)->unrealize(widget);
}

static void
giw_timeline_map(GtkWidget *widget) {
  GiwTimelinePrivate *priv = GIW_TIMELINE_GET_PRIVATE(widget);

  GTK_WIDGET_CLASS(parent_class)->map(widget);

  if (priv->input_window)
    gdk_window_show(priv->input_window);
}

static void
giw_timeline_unmap(GtkWidget *widget) {
  GiwTimelinePrivate *priv = GIW_TIMELINE_GET_PRIVATE(widget);

  if (priv->input_window)
    gdk_window_hide(priv->input_window);

  GTK_WIDGET_CLASS(parent_class)->unmap(widget);
}

static void
giw_timeline_size_allocate(GtkWidget     *widget,
                           GtkAllocation *allocation) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);
  GtkAllocation     widget_allocation;
  gboolean          resized;

  gtk_widget_get_allocation(widget, &widget_allocation);

  resized = (widget_allocation.width  != allocation->width ||
             widget_allocation.height != allocation->height);

  gtk_widget_set_allocation(widget, allocation);

  if (gtk_widget_get_realized(widget)) {
    gdk_window_move_resize(priv->input_window,
                           allocation->x, allocation->y,
                           allocation->width, allocation->height);

    if (resized)
      giw_timeline_make_pixmap(timeline);
  }
}

static void
giw_timeline_size_request(GtkWidget      *widget,
                          GtkRequisition *requisition) {
  GiwTimelinePrivate *priv    = GIW_TIMELINE_GET_PRIVATE(widget);
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

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
    requisition->width  += 1;
    requisition->height += size;
  } else {
    requisition->width  += size;
    requisition->height += 1;
  }
}

static void
giw_timeline_get_preferred_width(GtkWidget *widget,
                                 gint      *minimum_width,
                                 gint      *natural_width) {
  GtkRequisition requisition;

  giw_timeline_size_request(widget, &requisition);

  *minimum_width = *natural_width = requisition.width;
}

static void
giw_timeline_get_preferred_height(GtkWidget *widget,
                                  gint      *minimum_height,
                                  gint      *natural_height) {
  GtkRequisition requisition;

  giw_timeline_size_request(widget, &requisition);

  *minimum_height = *natural_height = requisition.height;
}

static void
giw_timeline_style_updated(GtkWidget *widget) {
  GiwTimelinePrivate *priv = GIW_TIMELINE_GET_PRIVATE(widget);

  GTK_WIDGET_CLASS(giw_timeline_parent_class)->style_updated(widget);

  gtk_widget_style_get(widget,
                       "font-scale", &priv->font_scale,
                       NULL);

  if (priv->layout) {
    g_object_unref(priv->layout);
    priv->layout = NULL;
  }
}


static gboolean
giw_timeline_draw(GtkWidget *widget,
                  cairo_t   *cr) {
  GiwTimeline        *timeline = GIW_TIMELINE(widget);
  GiwTimelinePrivate *priv  = GIW_TIMELINE_GET_PRIVATE(timeline);

  giw_timeline_draw_ticks(timeline);

  cairo_set_source_surface(cr, priv->backing_store, 0, 0);
  cairo_paint(cr);

  giw_timeline_draw_pos(timeline);

  return FALSE;
}

static void
giw_timeline_draw_ticks(GiwTimeline *timeline) {
  GtkWidget        *widget  = GTK_WIDGET(timeline);
  GtkStyleContext  *context = gtk_widget_get_style_context(widget);
  GiwTimelinePrivate *priv    = GIW_TIMELINE_GET_PRIVATE(timeline);
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

  gint scaleh,scalem,scales;

  gint curi,curh,curm,curs;
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

#ifdef GTK_RENDER_BACKGROUND_BUG
  GdkRGBA col;
#endif
  
  if (! gtk_widget_is_drawable(widget))
    return;

  gtk_widget_get_allocation(widget, &allocation);
  gtk_style_context_get_border(context, gtk_widget_get_state_flags(widget), &border);

  layout = giw_timeline_get_layout(widget, "0123456789");
  pango_layout_get_extents(layout, &ink_rect, &logical_rect);

  digit_height = PANGO_PIXELS(ink_rect.height) + 2;
  digit_offset = ink_rect.y;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
    width  = allocation.width;
    height = allocation.height - (border.top + border.bottom);
  } else {
    width  = allocation.height;
    height = allocation.width - (border.top + border.bottom);
  }

  cr = cairo_create(priv->backing_store);

  gtk_render_background(context, cr, 0., 0., allocation.width, allocation.height);

#ifdef GTK_RENDER_BACKGROUND_BUG
#ifdef G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#endif
  gtk_style_context_get_background_color(context,gtk_widget_get_state_flags(widget),&col);
#ifdef G_GNUC_END_IGNORE_DEPRECATIONS
  G_GNUC_END_IGNORE_DEPRECATIONS
#endif
  cairo_set_source_rgb(cr, col.red, col.green, col.blue);
  cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
  cairo_fill(cr);
#endif
  
  gtk_render_frame(context, cr, 0, 0, allocation.width, allocation.height);

  gtk_style_context_get_color(context, gtk_widget_get_state_flags(widget),
                              &color);
  gdk_cairo_set_source_rgba(cr, &color);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
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
   *
   *   We calculate the text size as for the vtimeline instead of
   *   actually measuring the text width, so that the result for the
   *   scale looks consistent with an accompanying vtimeline.
   */
  scale = ceil(max_size);

  if (priv->unit==GIW_TIME_UNIT_SECONDS) {
    g_snprintf(unit_str, sizeof(unit_str), "%d", scale);
  } else {
    scaleh=(int)((double)scale/3600.);
    scalem=(int)((double)(scale-scaleh*3600)/60.);
    scales=scale-scaleh*3600-scalem*60;

    if (scale<0) {
      scalem=-scalem;
      scales=-scales;
    }

    g_snprintf(unit_str, sizeof(unit_str), "%02d:%02d:%02d", scaleh,scalem,scales);
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

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
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
        curi=(int)cur;
        if (priv->unit==GIW_TIME_UNIT_SECONDS) {
          g_snprintf(unit_str, sizeof(unit_str), "%d", curi);
        } else {
          curh=(int)(cur/3600.);
          curm=(int)((double)(curi-curh*3600)/60.);
          curs=curi-curh*3600-curm*60;

          if (curi<0) {
            curm=-curm;
            curs=-curs;
          }

          g_snprintf(unit_str, sizeof(unit_str), "%02d:%02d:%02d", curh,curm,curs);
        }

        if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
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

static void
giw_timeline_draw_pos(GiwTimeline *timeline) {
  GtkWidget        *widget  = GTK_WIDGET(timeline);
  GtkStyleContext  *context = gtk_widget_get_style_context(widget);
  GiwTimelinePrivate *priv    = GIW_TIMELINE_GET_PRIVATE(timeline);
  GtkAllocation     allocation;
  GtkBorder         border;
  GdkRGBA           color;
  gint              x, y;
  gint              width, height;
  gint              bs_width, bs_height;

  if (! gtk_widget_is_drawable(widget))
    return;

  gtk_widget_get_allocation(widget, &allocation);
  gtk_style_context_get_border(context, gtk_widget_get_state_flags(widget), &border);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
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

    /*  If a backing store exists, restore the timeline  */
    if (priv->backing_store) {
      cairo_set_source_surface(cr, priv->backing_store, 0, 0);
      cairo_rectangle(cr, priv->xsrc, priv->ysrc, bs_width, bs_height);
      cairo_fill(cr);
    }

    position = giw_timeline_get_position(timeline);

    giw_timeline_get_range(timeline, &lower, &upper, NULL);

    if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
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

    if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
      cairo_line_to(cr, x + bs_width / 2.0, y + bs_height);
      cairo_line_to(cr, x + bs_width,       y);
    } else {
      cairo_line_to(cr, x + bs_width, y + bs_height / 2.0);
      cairo_line_to(cr, x,            y + bs_height);
    }

    cairo_fill(cr);

    cairo_destroy(cr);

    priv->xsrc = x;
    priv->ysrc = y;
  }
}




static void
giw_timeline_make_pixmap(GiwTimeline *timeline) {
  GtkWidget        *widget = GTK_WIDGET(timeline);
  GiwTimelinePrivate *priv   = GIW_TIMELINE_GET_PRIVATE(timeline);
  GtkAllocation     allocation;

  gtk_widget_get_allocation(widget, &allocation);

  if (priv->backing_store)
    cairo_surface_destroy(priv->backing_store);

  priv->backing_store =
    gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                      CAIRO_CONTENT_COLOR,
                                      allocation.width,
                                      allocation.height);
}

static PangoLayout *
giw_timeline_create_layout(GtkWidget   *widget,
                           const gchar *text) {
  GiwTimelinePrivate *priv = GIW_TIMELINE_GET_PRIVATE(widget);
  PangoLayout      *layout;
  PangoAttrList    *attrs;
  PangoAttribute   *attr;

  layout = gtk_widget_create_pango_layout(widget, text);

  attrs = pango_attr_list_new();

  attr = pango_attr_scale_new(priv->font_scale);
  attr->start_index = 0;
  attr->end_index   = -1;
  pango_attr_list_insert(attrs, attr);

  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);

  return layout;
}

static PangoLayout *
giw_timeline_get_layout(GtkWidget   *widget,
                        const gchar *text) {
  GiwTimelinePrivate *priv = GIW_TIMELINE_GET_PRIVATE(widget);

  if (priv->layout) {
    pango_layout_set_text(priv->layout, text, -1);
    return priv->layout;
  }

  priv->layout = giw_timeline_create_layout(widget, text);

  return priv->layout;
}



#endif // #if GTK_CHECK_VERSION(3,0,0)
