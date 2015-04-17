/* giwled.c  -  GiwLed widget's source
Copyright (C) 2006  Alexandre Pereira Bueno, Eduardo Parente Ribeiro

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Maintainers
Alexandre Pereira Bueno - alpebu@yahoo.com.br
James Scott Jr <skoona@users.sourceforge.net>
*/

// additional code G. Finch (salsaman@gmail.com) 2010 - 2013

#include <math.h>
#include <stdio.h>

#include "main.h"

#include "giw/giwled.h"

#define LED_DEFAULT_SIZE 14

enum {
  MODE_CHANGED_SIGNAL,
  LAST_SIGNAL
};

/* Forward declarations */

static void giw_led_class_init(GiwLedClass    *klass);
static void giw_led_init(GiwLed         *led);
static void giw_led_realize(GtkWidget        *widget);
static void giw_led_size_allocate(GtkWidget     *widget,
                                  GtkAllocation *allocation);
static gint giw_led_button_press(GtkWidget        *widget,
                                 GdkEventButton   *event);
static void giw_led_size_request(GtkWidget      *widget,
                                 GtkRequisition *requisition);
#if GTK_CHECK_VERSION(3,0,0)
static void giw_led_dispose(GObject        *object);
static void giw_led_get_preferred_width(GtkWidget *widget,
                                        gint      *minimal_width,
                                        gint      *natural_width);
static void giw_led_get_preferred_height(GtkWidget *widget,
    gint      *minimal_height,
    gint      *natural_height);
static gboolean giw_led_draw(GtkWidget *widget, cairo_t *cairo);
#else
static gint giw_led_expose(GtkWidget        *widget,
                           GdkEventExpose   *event);
static void giw_led_destroy(GtkObject        *object);
#endif

/* Local data */

static guint giw_led_signals[LAST_SIGNAL] = { 0 };


#if GTK_CHECK_VERSION(3,0,0)
G_DEFINE_TYPE(GiwLed, giw_led, GTK_TYPE_WIDGET)
#define parent_class giw_led_parent_class
#else
static GtkWidgetClass *parent_class = NULL;


/*********************
* Widget's Functions *
*********************/

GType giw_led_get_type() {
  static GType led_type = 0;

  if (!led_type) {
    static const GtkTypeInfo led_info = {
      "GiwLed",
      sizeof(GiwLed),
      sizeof(GiwLedClass),
      (GtkClassInitFunc) giw_led_class_init,
      (GtkObjectInitFunc) giw_led_init,
      /*(GtkArgSetFunc)*/ NULL,
      /*(GtkArgGetFunc)*/ NULL,
      (GtkClassInitFunc) NULL,
    };

    led_type = gtk_type_unique(gtk_widget_get_type() , &led_info);
  }

  return led_type;
}


#endif

static void
giw_led_class_init(GiwLedClass *xclass) {
#if GTK_CHECK_VERSION(3,0,0)
  GObjectClass *object_class = G_OBJECT_CLASS(xclass);
#else
  GtkObjectClass *object_class = (GtkObjectClass *) xclass;
#endif
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass *) xclass;

#if GTK_CHECK_VERSION(3,0,0)
  object_class->dispose = giw_led_dispose;
#else
  parent_class = (GtkWidgetClass *)gtk_type_class(gtk_widget_get_type());
  object_class->destroy = giw_led_destroy;
#endif

  widget_class->realize = giw_led_realize;
#if GTK_CHECK_VERSION(3,0,0)
  widget_class->get_preferred_width = giw_led_get_preferred_width;
  widget_class->get_preferred_height = giw_led_get_preferred_height;
  widget_class->draw = giw_led_draw;
#else
  widget_class->expose_event = giw_led_expose;
  widget_class->size_request = giw_led_size_request;
#endif
  widget_class->size_allocate = giw_led_size_allocate;
  widget_class->button_press_event = giw_led_button_press;

  giw_led_signals[MODE_CHANGED_SIGNAL] = g_signal_new("mode_changed",
                                         G_TYPE_FROM_CLASS(xclass),
                                         (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                                         G_STRUCT_OFFSET(GiwLedClass, mode_changed),
                                         NULL,
                                         NULL,
                                         g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE, 0);
}

static void
giw_led_init(GiwLed *led) {
  g_return_if_fail(led != NULL);
  g_return_if_fail(GIW_IS_LED(led));

  led->on=0; //Default position: off
  led->enable_mouse=FALSE;

  // Default on color, full green
  led->color_on.green=65535;
  led->color_on.red=0;
  led->color_on.blue=0;

  // Default off color, white
  led->color_off.green=65535;
  led->color_off.red=65535;
  led->color_off.blue=65535;

#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_has_window(GTK_WIDGET(led),TRUE);
#endif
}

GtkWidget *
giw_led_new(void) {
  GiwLed *led;

#if GTK_CHECK_VERSION(3,0,0)
  led = (GiwLed *)g_object_new(GIW_TYPE_LED, NULL);
#else
  led = (GiwLed *)gtk_type_new(giw_led_get_type());
#endif

  return GTK_WIDGET(led);
}

#if GTK_CHECK_VERSION(3,0,0)
static void giw_led_dispose(GObject *object) {
#else
static void giw_led_destroy(GtkObject *object) {
#endif
  g_return_if_fail(object != NULL);
  g_return_if_fail(GIW_IS_LED(object));

#if GTK_CHECK_VERSION(3,0,0)
  G_OBJECT_CLASS(giw_led_parent_class)->dispose(object);
#else
  if (LIVES_GUI_OBJECT_CLASS(parent_class)->destroy)
    (* LIVES_GUI_OBJECT_CLASS(parent_class)->destroy)(object);
#endif
}


static void
giw_led_realize(GtkWidget *widget) {
  GdkWindowAttr attributes;
  gint attributes_mask;

#if GTK_CHECK_VERSION(3,0,0)
  GtkStyleContext *stylecon;
#endif

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GIW_IS_LED(widget));

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_realized(widget,TRUE);
#else
  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
#endif


  attributes.x = lives_widget_get_allocation_x(widget);
  attributes.y = lives_widget_get_allocation_y(widget);
  attributes.width = lives_widget_get_allocation_width(widget);
  attributes.height = lives_widget_get_allocation_height(widget);
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events(widget) |
                          GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

#if !GTK_CHECK_VERSION(3,0,0)
  attributes.visual = gtk_widget_get_visual(widget);
  attributes_mask |= GDK_WA_COLORMAP | GDK_WA_VISUAL;
  attributes.colormap = gtk_widget_get_colormap(widget);
#endif

#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_window(widget,gdk_window_new(lives_widget_get_xwindow(lives_widget_get_parent(widget)), &attributes, attributes_mask));
#else
  widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);
#endif

#if GTK_CHECK_VERSION(3,0,0)
  stylecon=gtk_widget_get_style_context(widget);
  if (!stylecon) {
    stylecon=gtk_style_context_new();
    gtk_style_context_set_path(stylecon,gtk_widget_get_path(widget));
  }
  gtk_style_context_add_class(stylecon,"giwled");
  gtk_style_context_set_state(stylecon,GTK_STATE_FLAG_ACTIVE);
  gtk_style_context_set_background(stylecon,lives_widget_get_xwindow(widget));
#else
  widget->style = gtk_style_attach(widget->style, lives_widget_get_xwindow(widget));
  gtk_style_set_background(widget->style, lives_widget_get_xwindow(widget), GTK_STATE_ACTIVE);
#endif

  gdk_window_set_user_data(lives_widget_get_xwindow(widget), widget);

}




static void
giw_led_size_request(GtkWidget      *widget,
                     GtkRequisition *requisition) {
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GIW_IS_LED(widget));
  g_return_if_fail(requisition != NULL);

  requisition->width = LED_DEFAULT_SIZE;
  requisition->height = LED_DEFAULT_SIZE;
}

#if GTK_CHECK_VERSION(3,0,0)

static void
giw_led_get_preferred_width(GtkWidget *widget,
                            gint      *minimal_width,
                            gint      *natural_width) {
  GtkRequisition requisition;

  giw_led_size_request(widget, &requisition);

  *minimal_width = *natural_width = requisition.width;
}

static void
giw_led_get_preferred_height(GtkWidget *widget,
                             gint      *minimal_height,
                             gint      *natural_height) {
  GtkRequisition requisition;

  giw_led_size_request(widget, &requisition);

  *minimal_height = *natural_height = requisition.height;
}

#endif


static void
giw_led_size_allocate(GtkWidget     *widget,
                      GtkAllocation *allocation) {
  GiwLed *led;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GIW_IS_LED(widget));
  g_return_if_fail(allocation != NULL);

#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_allocation(widget,allocation);
#else
  widget->allocation = *allocation;
#endif

  led = GIW_LED(widget);

  if (lives_widget_is_realized(widget)) {
    gdk_window_move_resize(lives_widget_get_xwindow(widget),
                           allocation->x, allocation->y,
                           allocation->width, allocation->height);

  }

  // The size of the led will be the lower dimension of the widget
  if (lives_widget_get_allocation_width(widget) > lives_widget_get_allocation_height(widget)) {
    led->size=lives_widget_get_allocation_height(widget);
    led->radius=led->size-4;
    led->x=(lives_widget_get_allocation_width(widget)/2)-(led->size/2);
    led->y=0;
  } else {
    led->size=lives_widget_get_allocation_width(widget);
    led->radius=led->size-4;
    led->x=0;
    led->y=(lives_widget_get_allocation_height(widget)/2)-(led->size/2);
  }
}

#if GTK_CHECK_VERSION(3,0,0)
static gboolean giw_led_draw(GtkWidget *widget, cairo_t *cairo) {

#else

static gint
giw_led_expose(GtkWidget      *widget,
               GdkEventExpose *event) {
#endif
  GiwLed *led;
  GdkRectangle rect;

#if !GTK_CHECK_VERSION(3,0,0)
  GdkGC *gc; // To put the on and off colors

  g_return_val_if_fail(event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
#endif

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GIW_IS_LED(widget), FALSE);

  led=GIW_LED(widget);

  rect.x=0;
  rect.y=0;
  rect.width=lives_widget_get_allocation_width(widget);
  rect.height=lives_widget_get_allocation_height(widget);

  // Drawing background
#if GTK_CHECK_VERSION(3,0,0)
  cairo_set_line_width(cairo,1.);

  gtk_render_background(gtk_widget_get_style_context(widget),
                        cairo,
                        0,
                        0,
                        rect.width,
                        rect.height);

  if (led->on)
    cairo_set_source_rgb(cairo, 1., 1., 1.);
  else
    cairo_set_source_rgba(cairo,
                          led->color_off.red,
                          led->color_off.green,
                          led->color_off.blue,
                          led->color_off.alpha
                         );

  cairo_arc(cairo,
            rect.width/2+2,
            rect.height/2+2,
            led->radius/2,
            0.,
            2.*M_PI);

  if (led->on) cairo_set_source_rgb(cairo, 0., 0., 0.);

  cairo_arc(cairo,
            rect.width/2+1,
            rect.height/2+1,
            led->radius/2+1,
            -45./M_PI,
            57.5/M_PI);

  cairo_arc(cairo,
            rect.width/2,
            rect.height/2,
            led->radius/2+1.5,
            -32/M_PI,
            37.5/M_PI);

  if (led->on)
    cairo_set_source_rgba(cairo,
                          (double)(led->color_on.red),
                          (double)(led->color_on.green),
                          (double)(led->color_on.blue),
                          (double)(led->color_on.alpha)
                         );
  else
    cairo_set_source_rgba(cairo,
                          (double)(led->color_off.red),
                          (double)(led->color_off.green),
                          (double)(led->color_off.blue),
                          (double)(led->color_off.alpha)
                         );


  cairo_arc(cairo,
            rect.width/2+2,
            rect.height/2+2,
            (led->size-4)/2,
            0,
            2.*M_PI);

  cairo_fill(cairo);

#else
  gtk_paint_flat_box(widget->style,
                     widget->window,
                     (GtkStateType)(widget->parent==NULL?GTK_STATE_NORMAL:widget->parent->state),
                     GTK_SHADOW_NONE,
                     &rect,
                     widget,
                     NULL,
                     0,
                     0,
                     -1,
                     -1);

  gc=gdk_gc_new(widget->window); // Allocating memory
  gdk_gc_copy(gc, widget->style->fg_gc[widget->state]);

  if (led->on)
    gdk_gc_set_rgb_fg_color(gc, &(led->color_on));
  else
    gdk_gc_set_rgb_fg_color(gc, &(led->color_off));


  // The border
  gdk_draw_arc(widget->window,
               led->on?widget->style->white_gc:gc,
               FALSE,
               led->x+2,
               led->y+2,
               led->radius,
               led->radius,
               0,
               64*360);

  gdk_draw_arc(widget->window,
               widget->style->black_gc,
               FALSE,
               led->x+1,
               led->y+1,
               led->radius+2,
               led->radius+2,
               -64*90,
               64*115);

  gdk_draw_arc(widget->window,
               widget->style->black_gc,
               FALSE,
               led->x+1,
               led->y+1,
               led->radius+3,
               led->radius+3,
               -64*60,
               64*75);

  gdk_draw_arc(widget->window,
               gc,
               TRUE,
               led->x+2,
               led->y+2,
               led->size-4,
               led->size-4,
               0,
               64*360);

  g_object_unref(gc);
#endif

  return FALSE;
}


static gint
giw_led_button_press(GtkWidget      *widget,
                     GdkEventButton *event) {
  GiwLed *led;
  guint dx, dy, d;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GIW_IS_LED(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  led = GIW_LED(widget);

  if (led->enable_mouse==0) return (FALSE);

  dx = event->x - lives_widget_get_allocation_width(widget)/2;
  dy = lives_widget_get_allocation_height(widget)/2 - event->y;

  d=sqrt(dx*dx+dy*dy); // Distance between the pointer and the center

  if (d <= (led->size/2)) { // If it's inside the led
    if (led->on==FALSE)
      led->on=TRUE;
    else
      led->on=FALSE;

    g_signal_emit(G_OBJECT(led), giw_led_signals[MODE_CHANGED_SIGNAL], 0);
  }

  gtk_widget_queue_draw(GTK_WIDGET(led));

  return (FALSE);
}

/******************
* Users Functions *
******************/

void
giw_led_set_mode(GiwLed *led,
                 guint8 mode) {
  g_return_if_fail(led != NULL);
  g_return_if_fail(GIW_IS_LED(led));

  if (led->on!=mode) {
    led->on=mode;
    g_signal_emit(G_OBJECT(led), giw_led_signals[MODE_CHANGED_SIGNAL], 0);

    gtk_widget_queue_draw(GTK_WIDGET(led));
  }
}

guint8
giw_led_get_mode(GiwLed *led) {
  g_return_val_if_fail(led != NULL, 0);
  g_return_val_if_fail(GIW_IS_LED(led), 0);

  return (led->on);
}

#if GTK_CHECK_VERSION(3,0,0)
void
giw_led_set_rgba(GiwLed *led,
                 GdkRGBA on_color,
                 GdkRGBA off_color) {
  g_return_if_fail(led != NULL);
  g_return_if_fail(GIW_IS_LED(led));

  led->color_on=on_color;
  led->color_off=off_color;

  gtk_widget_queue_draw(GTK_WIDGET(led));
}
#endif
void
giw_led_set_colors(GiwLed *led,
                   GdkColor on_color,
                   GdkColor off_color) {
  g_return_if_fail(led != NULL);
  g_return_if_fail(GIW_IS_LED(led));

#if GTK_CHECK_VERSION(3,0,0)
  led->color_on.red=(gdouble)on_color.red/65535.;
  led->color_on.green=(gdouble)on_color.green/65535.;
  led->color_on.blue=(gdouble)on_color.blue/65535.;
  led->color_on.alpha=1.;
  led->color_off.red=(gdouble)off_color.red/65535.;
  led->color_off.green=(gdouble)off_color.green/65535.;
  led->color_off.blue=(gdouble)off_color.blue/65535.;
  led->color_off.alpha=1.;
#else
  led->color_on=on_color;
  led->color_off=off_color;
#endif
  gtk_widget_queue_draw(GTK_WIDGET(led));
}

void
giw_led_enable_mouse(GiwLed *led,
                     gboolean option) {
  g_return_if_fail(led != NULL);
  g_return_if_fail(GIW_IS_LED(led));

  led->enable_mouse=option;
}

/******************
* Local Functions *
******************/


