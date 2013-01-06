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

static void giw_led_class_init               (GiwLedClass    *klass);
static void giw_led_init                     (GiwLed         *led);
static void giw_led_destroy                  (GtkObject        *object);
static void giw_led_realize                  (GtkWidget        *widget);
static void giw_led_size_request             (GtkWidget      *widget,
					       GtkRequisition *requisition);
static void giw_led_size_allocate            (GtkWidget     *widget,
					       GtkAllocation *allocation);
static gint giw_led_expose                   (GtkWidget        *widget,
						GdkEventExpose   *event);
static gint giw_led_button_press             (GtkWidget        *widget,
						GdkEventButton   *event);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint giw_led_signals[LAST_SIGNAL] = { 0 };

/*********************
* Widget's Functions *
*********************/

GType
giw_led_get_type ()
{
  static GType led_type = 0;

  if (!led_type)
    {
#if GTK_VERSION_3
      static const GTypeInfo led_info =
      {
	sizeof (GiwLedClass),
	(GBaseInitFunc)NULL,
	(GBaseFinalizeFunc)NULL,
	(GClassInitFunc) giw_led_class_init,
	(GClassFinalizeFunc)NULL,
	(gconstpointer)NULL,
	sizeof (GiwLed),
	0, // n_preallocs
	(GInstanceInitFunc) giw_led_init
	//(const GTypeValueTable *)NULL
      };

      led_type = g_type_register_static (gtk_widget_get_type (), "GiwLed", &led_info, 0);

#else
      static const GtkTypeInfo led_info =
      {
	"GiwLed",
	sizeof (GiwLed),
	sizeof (GiwLedClass),
	(GtkClassInitFunc) giw_led_class_init,
	(GtkObjectInitFunc) giw_led_init,
	/*(GtkArgSetFunc)*/ NULL,
	/*(GtkArgGetFunc)*/ NULL,
	(GtkClassInitFunc) NULL,
      };

      led_type = gtk_type_unique (gtk_widget_get_type() , &led_info);

#endif

    }

  return led_type;
}

static void
giw_led_class_init (GiwLedClass *xclass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) xclass;
  widget_class = (GtkWidgetClass*) xclass;

#if GTK_VERSION_3
  // parent_class is set in g_type_register_static()
#else
  parent_class = (GtkWidgetClass *)gtk_type_class (gtk_widget_get_type ());
#endif

  object_class->destroy = giw_led_destroy;

  widget_class->realize = giw_led_realize;
  widget_class->expose_event = giw_led_expose;
  widget_class->size_request = giw_led_size_request;
  widget_class->size_allocate = giw_led_size_allocate;
  widget_class->button_press_event = giw_led_button_press;
  
  giw_led_signals[MODE_CHANGED_SIGNAL] = g_signal_new ("mode_changed",
					 G_TYPE_FROM_CLASS (xclass),
						       (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
	                                 G_STRUCT_OFFSET (GiwLedClass, mode_changed),
                                         NULL, 
                                         NULL,                
					 g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE, 0);
}

static void
giw_led_init (GiwLed *led)
{
  g_return_if_fail (led != NULL);
  g_return_if_fail (GIW_IS_LED (led));

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
}

GtkWidget*
giw_led_new (void)
{
  GiwLed *led;

#if GTK_VERSION_3
  led = g_object_new (GIW_TYPE_LED, NULL);
#else
  led = (GiwLed *)gtk_type_new (giw_led_get_type ());
#endif
  
  return GTK_WIDGET (led);
}

static void
giw_led_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GIW_IS_LED (object));
    
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
giw_led_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_LED (widget));

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_realized(widget,TRUE);
#else
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
#endif

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;

  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget) | 
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
    GDK_POINTER_MOTION_HINT_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_user_data (widget->window, widget);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);
}

static void 
giw_led_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_LED (widget));
  g_return_if_fail (requisition != NULL);

  requisition->width = LED_DEFAULT_SIZE;
  requisition->height = LED_DEFAULT_SIZE;
}

static void
giw_led_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GiwLed *led;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_LED (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  led = GIW_LED (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {

      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

    }
  
  // The size of the led will be the lower dimension of the widget  
  if (widget->allocation.width > widget->allocation.height){
    led->size=widget->allocation.height;
    led->radius=led->size-4;
    led->x=(widget->allocation.width/2)-(led->size/2);
    led->y=0;
  }
  else{
    led->size=widget->allocation.width;
    led->radius=led->size-4;
    led->x=0;
    led->y=(widget->allocation.height/2)-(led->size/2);
  }
}

static gint
giw_led_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{  
  GiwLed *led;
  GdkRectangle rect;
  GdkGC *gc; // To put the on and off colors
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_LED (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
    
  led=GIW_LED(widget);

  rect.x=0;rect.y=0;rect.width=widget->allocation.width;rect.height=widget->allocation.height;
      
  // Drawing backgorund
  gtk_paint_flat_box (widget->style,
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
    gdk_gc_set_rgb_fg_color (gc, &(led->color_on));
  else
    gdk_gc_set_rgb_fg_color (gc, &(led->color_off));
    
  
  // The border  
  gdk_draw_arc (widget->window,
                led->on?widget->style->white_gc:gc,
		FALSE,
		led->x+2,
                led->y+2,
		led->radius,
		led->radius,
		0,
                64*360);

  gdk_draw_arc (widget->window,
                widget->style->black_gc,
		FALSE,
		led->x+1,
                led->y+1,
		led->radius+2,
		led->radius+2,
		-64*90,
                64*115);

  gdk_draw_arc (widget->window,
                widget->style->black_gc,
		FALSE,
		led->x+1,
                led->y+1,
		led->radius+3,
		led->radius+3,
		-64*60,
                64*75);

  gdk_draw_arc (widget->window,
                gc,
		TRUE,
		led->x+2,
                led->y+2,
		led->size-4,
		led->size-4,
		0,
                64*360);

  g_object_unref(gc);
  
  return FALSE;
}

static gint
giw_led_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GiwLed *led;
  guint dx, dy, d;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_LED (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  led = GIW_LED (widget);
    
  if (led->enable_mouse==0) return(FALSE);
  
  dx = event->x - widget->allocation.width/2;
  dy = widget->allocation.height/2 - event->y;
  
  d=sqrt(dx*dx+dy*dy); // Distance between the pointer and the center
  
  if (d <= (led->size/2)){ // If it's inside the led
     if (led->on==FALSE)
       led->on=TRUE;
     else
       led->on=FALSE;
       
     g_signal_emit (G_OBJECT (led), giw_led_signals[MODE_CHANGED_SIGNAL], 0);
  }
  
  gtk_widget_queue_draw(GTK_WIDGET(led));
 
  return(FALSE);
}

/******************
* Users Functions *
******************/

void
giw_led_set_mode(GiwLed *led,
			guint8 mode)
{
  g_return_if_fail (led != NULL);
  g_return_if_fail (GIW_IS_LED (led));

  if (led->on!=mode){
    led->on=mode;
    g_signal_emit (G_OBJECT (led), giw_led_signals[MODE_CHANGED_SIGNAL], 0);
  
    gtk_widget_queue_draw(GTK_WIDGET(led));
  }
}

guint8         
giw_led_get_mode (GiwLed *led)
{
  g_return_val_if_fail (led != NULL, 0);
  g_return_val_if_fail (GIW_IS_LED (led), 0);
  
  return (led->on);
}

void
giw_led_set_colors (GiwLed *led,
			GdkColor on_color,
			GdkColor off_color)
{
  g_return_if_fail (led != NULL);
  g_return_if_fail (GIW_IS_LED (led));

  led->color_on=on_color;
  led->color_off=off_color;
  gtk_widget_queue_draw(GTK_WIDGET(led));
}

void
giw_led_enable_mouse (GiwLed *led,
			 gboolean option)
{
  g_return_if_fail (led != NULL);
  g_return_if_fail (GIW_IS_LED (led));
  
  led->enable_mouse=option;
}

/******************
* Local Functions *
******************/


