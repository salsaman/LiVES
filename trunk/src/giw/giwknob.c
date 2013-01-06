/* giwknob.c  -  GiwKnob widget's source    Version 0.1
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

// additional code G. Finch (salsaman@gmail.com) 2010 - 2012


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#include "giw/giwknob.h"


#define KNOB_DEFAULT_SIZE 150

/* Forward declarations */
static void giw_knob_class_init               (GiwKnobClass    *klass);
static void giw_knob_init                     (GiwKnob         *knob);
static void giw_knob_destroy                  (GtkObject        *object);
static void giw_knob_realize                  (GtkWidget        *widget);
static void giw_knob_size_request             (GtkWidget      *widget,
					       GtkRequisition *requisition);
static void giw_knob_size_allocate            (GtkWidget     *widget,
					       GtkAllocation *allocation);
static gint giw_knob_expose                   (GtkWidget        *widget,
						GdkEventExpose   *event);
static gint giw_knob_button_press             (GtkWidget        *widget,
						GdkEventButton   *event);
static gint giw_knob_button_release           (GtkWidget        *widget,
						GdkEventButton   *event);
static gint giw_knob_motion_notify            (GtkWidget        *widget,
						GdkEventMotion   *event);
static void giw_knob_style_set	              (GtkWidget      *widget,
			                        GtkStyle       *previous_style);
/* Local data */

static GtkWidgetClass *parent_class = NULL;

// Changes the value, by mouse position
void knob_update_mouse                             (GiwKnob *knob, gint x, gint y);

// Updates the false pointer's angle
void knob_update_false_mouse                       (GiwKnob *knob, gint x, gint y);

// Calculate the value, using the angle
gdouble knob_calculate_value_with_angle               (GiwKnob         *knob,
						gdouble angle);
// Calculate the angle, using the value
gdouble knob_calculate_angle_with_value               (GiwKnob         *knob,
						gdouble value); 
// Calculate all sizes 
static void knob_calculate_sizes                      (GiwKnob         *knob); 
// To make the changes needed when someno changes the lower ans upper fields of the adjustment
static void giw_knob_adjustment_changed       (GtkAdjustment    *adjustment,
						gpointer          data);
// To make the changes needed when someno changes the value of the adjustment
static void giw_knob_adjustment_value_changed (GtkAdjustment    *adjustment,
						gpointer          data);
// A not public knob_set_angle function, for internal using, it only sets the angle, nothing more
void knob_set_angle                                (GiwKnob *knob,
						gdouble angle);
// A not public knob_set_value function, for internal using, it only sets the value, nothing more
void knob_set_value                                (GiwKnob *knob,
						gdouble value);
// A function that creates the layout of legends and calculates it's sizes
void knob_build_legends(GiwKnob *knob); 

// A function that frees the layout of legends 
void knob_free_legends(GiwKnob *knob); 

// A function that creates the layout of the title
void knob_build_title(GiwKnob *knob);

// A function that calculates width and height of the legend's the layout
void knob_calculate_legends_sizes(GiwKnob *knob); 

// A function that calculates width and height of the title's the layout
void knob_calculate_title_sizes(GiwKnob *knob);  

/*********************
* Widget's Functions * 
*********************/

GType
giw_knob_get_type ()
{
  static GType knob_type = 0;

  if (!knob_type)
    {

#if GTK_VERSION_3
      static const GTypeInfo knob_info =
      {
	sizeof (GiwKnobClass),
	(GBaseInitFunc)NULL,
	(GBaseFinalizeFunc)NULL,
	(GClassInitFunc) giw_knob_class_init,
	(GClassFinalizeFunc)NULL,
	(gconstpointer)NULL,
	sizeof (GiwKnob),
	0, // n_preallocs
	(GInstanceInitFunc) giw_knob_init
	//(const GTypeValueTable *)NULL
      };

      knob_type = g_type_register_static (gtk_widget_get_type (), "GiwKnob", &knob_info, 0);

#else
      static const GtkTypeInfo knob_info =
      {
	"GiwKnob",
	sizeof (GiwKnob),
	sizeof (GiwKnobClass),
	(GtkClassInitFunc) giw_knob_class_init,
	(GtkObjectInitFunc) giw_knob_init,
	/*(GtkArgSetFunc)*/ NULL,
	/*(GtkArgGetFunc)*/ NULL,
	(GtkClassInitFunc) NULL,
      };

      knob_type = gtk_type_unique (gtk_widget_get_type (), &knob_info);
#endif

    }

  return knob_type;
}

static void
giw_knob_class_init (GiwKnobClass *xclass)
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

  object_class->destroy = giw_knob_destroy;

  widget_class->realize = giw_knob_realize;
  widget_class->expose_event = giw_knob_expose;
  widget_class->size_request = giw_knob_size_request;
  widget_class->size_allocate = giw_knob_size_allocate;
  widget_class->button_press_event = giw_knob_button_press;
  widget_class->button_release_event = giw_knob_button_release;
  widget_class->motion_notify_event = giw_knob_motion_notify;
  widget_class->style_set = giw_knob_style_set;
}

static void
giw_knob_init (GiwKnob *knob)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));

  knob->button=0;
  knob->mouse_policy=GIW_KNOB_MOUSE_AUTOMATICALLY;
  knob->major_ticks=9;
  knob->minor_ticks=3;
  knob->major_ticks_size=5;
  knob->minor_ticks_size=3;
  knob->legends_digits=3;
  knob->title=NULL;
}

GtkWidget*
giw_knob_new (GtkAdjustment *adjustment)
{
  GiwKnob *knob;
  
  g_return_val_if_fail (adjustment != NULL, NULL);

#if GTK_VERSION_3
  knob = g_object_new (GIW_TYPE_KNOB, NULL);
#else
  knob = (GiwKnob *)gtk_type_new (giw_knob_get_type ());
#endif
  giw_knob_set_adjustment(knob, adjustment);
  
  // Without this, in the first draw, the pointer wouldn't be in the right value
  knob_set_angle(knob, knob_calculate_angle_with_value(knob, knob->adjustment->value));
    
  return GTK_WIDGET (knob);
}

GtkWidget*
giw_knob_new_with_adjustment (gdouble value, 
			gdouble lower, 
			gdouble upper)
{
  GiwKnob *knob;

#if GTK_VERSION_3
  knob = g_object_new (GIW_TYPE_KNOB, NULL);
#else
  knob = (GiwKnob *)gtk_type_new (giw_knob_get_type ());
#endif
  giw_knob_set_adjustment(knob, (GtkAdjustment*) gtk_adjustment_new (value, lower, upper, 1.0, 1.0, 1.0));
  
  // Without this, in the first draw, the pointer wouldn't be in the right value
  knob_set_angle(knob, knob_calculate_angle_with_value(knob, knob->adjustment->value));
  
  return GTK_WIDGET (knob);
}

static void
giw_knob_destroy (GtkObject *object)
{
  GiwKnob *knob;
  gint loop;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GIW_IS_KNOB (object));

  knob = GIW_KNOB (object);
  
  if (knob->adjustment)
    g_object_unref (G_OBJECT (knob->adjustment));
  knob->adjustment=NULL;

  if (knob->legends) {
    for (loop=0; loop<knob->major_ticks; loop++){
      g_object_unref(G_OBJECT(knob->legends[loop]));
    }
    g_free(knob->legends);
    knob->legends=NULL;
  }
  if (knob->title_str)
    g_free(knob->title_str);  
  if (knob->title)
    g_object_unref(G_OBJECT(knob->title));

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
giw_knob_realize (GtkWidget *widget)
{
  GiwKnob *knob;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_KNOB (widget));

#if GTK_CHECK_VERSION(2,20,0)
  gtk_widget_set_realized(widget,TRUE);
#else
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
#endif

  knob = GIW_KNOB (widget);

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
    
  // Create the initial legends
  knob_build_legends(knob);
}

static void 
giw_knob_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  requisition->width = KNOB_DEFAULT_SIZE;
  requisition->height = KNOB_DEFAULT_SIZE;
}

static void
giw_knob_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GiwKnob *knob;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_KNOB (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  knob = GIW_KNOB (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {

      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

    }
  knob_calculate_sizes(knob);
}

static gint
giw_knob_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  GiwKnob *knob;
  gdouble s,c;
  gint xc, yc;
  gdouble loop1;
  guint dx1, dy1, dx2, dy2;
  gint counter=0;
  GdkRectangle rect;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_KNOB (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
      
  knob=GIW_KNOB(widget);

  rect.x=0;rect.y=0;rect.width=widget->allocation.width;rect.height=widget->allocation.height;
      
  // Drawing backgorund
  gtk_paint_flat_box (widget->style,
			widget->window,
			GTK_STATE_NORMAL,
			GTK_SHADOW_NONE,
			&rect,
			widget,
			NULL, 
			0, 
			0, 
			-1,
			-1);			

  // The arc
  gdk_draw_arc (widget->window,
			widget->style->black_gc,
			TRUE,
			knob->x+((knob->size/2)-knob->radius),
			knob->y+((knob->size/2)-knob->radius),
			knob->radius*2,
			knob->radius*2,
			0,
			360*64);
 

 
  // The center
  xc = widget->allocation.width/2;
  yc = widget->allocation.height/2;
    
  // Draw the knob
  s = sin(knob->angle);
  c = cos(knob->angle);

  gdk_draw_line (widget->window,
			widget->style->white_gc,
			xc+c*((float)knob->radius*0.6),
			yc-s*((float)knob->radius*0.6),
			xc+c*knob->radius,
			yc-s*knob->radius);
  gdk_draw_arc (widget->window,
			widget->style->white_gc,
			TRUE,
			xc+c*((float)knob->radius*0.8)-knob->radius*0.1,
			yc-s*((float)knob->radius*0.8)-knob->radius*0.1,
			knob->radius*0.2,
			knob->radius*0.2,
			0,
			360*64);

 // Draw the false-pointer if the delayed policy of mouse is set and a button is pressed
  if ((knob->mouse_policy==GIW_KNOB_MOUSE_DELAYED) & (knob->button!=0)) {
    s = sin(knob->false_angle);
    c = cos(knob->false_angle);

    gdk_draw_line (widget->window,
			widget->style->fg_gc[widget->state],
			xc+c*((float)knob->radius*0.8),
			yc-s*((float)knob->radius*0.8),
			xc+c*knob->radius,
			yc-s*knob->radius);

    gdk_draw_arc (widget->window,
			widget->style->black_gc,
			FALSE,
			xc+c*((float)knob->radius*0.8)-knob->radius*0.1,
			yc-s*((float)knob->radius*0.8)-knob->radius*0.1,
			knob->radius*0.2,
			knob->radius*0.2,
			0,
			360*64);
  }
  
  // Now, draw the ticks
  // The major ticks (and legends)
  if (knob->major_ticks!=0)
    for (loop1=(3.0*M_PI/2.0); loop1>=-0.0001; loop1-=knob->d_major_ticks){ // -0.0001 (and not 0) to avoid rounding errors
      s=sin(loop1-M_PI/4.0);
      c=cos(loop1-M_PI/4.0);
      dx1=c*knob->radius;
      dy1=s*knob->radius;
      dx2=c*(knob->radius+knob->major_ticks_size);
      dy2=s*(knob->radius+knob->major_ticks_size);
      gdk_draw_line (widget->window,
			widget->style->fg_gc[widget->state],
			xc+dx1,
			yc-dy1,
			xc+dx2,
			yc-dy2);
      // Drawing the legends
      if (knob->legends_digits!=0)
        gtk_paint_layout (widget->style,
			widget->window,
			GTK_STATE_NORMAL,
			TRUE,
			&rect,
			widget,
			NULL,
                        xc+(c*knob->legend_radius)-(knob->legend_width/2),
			yc-(s*knob->legend_radius)-(knob->legend_height/2),
			knob->legends[counter]);
      counter++;
    }
  // The minor ticks
  if (knob->minor_ticks!=0)
    for (loop1=(3.0*M_PI/2.0); loop1>=0.0; loop1-=knob->d_minor_ticks){
      s=sin(loop1-M_PI/4.0);
      c=cos(loop1-M_PI/4.0);
      dx1=c*knob->radius;
      dy1=s*knob->radius;
      dx2=c*(knob->radius+knob->minor_ticks_size);
      dy2=s*(knob->radius+knob->minor_ticks_size);
      gdk_draw_line (widget->window,
			widget->style->fg_gc[widget->state],
			xc+dx1,
			yc-dy1,
			xc+dx2,
			yc-dy2); 
    }
    
  // Draw the title
  if (knob->title_str!=NULL) // font_str==NULL means no title
    gtk_paint_layout (widget->style,
			widget->window,
			GTK_STATE_NORMAL,
			TRUE,
			&rect,
			widget,
			NULL,
                        xc-knob->title_width/2,
			knob->size-knob->title_height-5, // 5 pixels to separate from the borders
			knob->title);
    
  return FALSE;
}

static gint
giw_knob_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GiwKnob *knob;
  gint xc, yc, dx, dy;
    
  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GIW_IS_KNOB (widget), TRUE);
  g_return_val_if_fail (event != NULL, TRUE);

  knob = GIW_KNOB (widget);
    
  if (knob->mouse_policy==GIW_KNOB_MOUSE_DISABLED) return TRUE;
  if (knob->button) return TRUE; // Some button is already pressed

  /* To verify if the pointer is in the knob, the distance between the pointer and the center
  of the circle is calculated, if it's less the the radius of the circle , it's in!!*/
   
  xc = widget->allocation.width/2;
  yc = widget->allocation.height/2;
  
  dx = abs((int)event->x - xc);
  dy = abs((int)event->y - yc);
      
  if (!knob->button & (dx<knob->radius) & (dy<knob->radius))
    knob->button = event->button;
  
  return FALSE;
}

static gint
giw_knob_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GiwKnob *knob;
  gint x, y;
  
  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GIW_IS_KNOB (widget), TRUE);
  g_return_val_if_fail (event != NULL, TRUE);

  knob = GIW_KNOB (widget);
  
  if (knob->mouse_policy==GIW_KNOB_MOUSE_DISABLED) return TRUE;
	  
     // If the policy is delayed, now that the button was released (if it is), it's time to update the value
  if ((knob->mouse_policy == GIW_KNOB_MOUSE_DELAYED) && 
      (knob->button == event->button )){
    x = event->x;
    y = event->y;

    knob_update_mouse (knob, x,y);
  }


  if (knob->button == event->button)
    knob->button = 0;
  
  return FALSE;
}

static gint
giw_knob_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GiwKnob *knob;
  gint x, y;

  g_return_val_if_fail (widget != NULL,TRUE);
  g_return_val_if_fail (GIW_IS_KNOB (widget),TRUE);
  g_return_val_if_fail (event != NULL,TRUE);

  knob = GIW_KNOB (widget);
  
  if (knob->mouse_policy==GIW_KNOB_MOUSE_DISABLED) return TRUE;
	  
  // If the some button is pressed and the policy is set to update the value AUTOMATICALLY, update the knob's value 
  if ( (knob->button != 0) && (knob->mouse_policy == GIW_KNOB_MOUSE_AUTOMATICALLY)) {
    x = event->x;
    y = event->y;

    if (event->is_hint || (event->window != widget->window))
      gdk_window_get_pointer (widget->window, &x, &y, NULL);

    knob_update_mouse (knob, x, y);
  }
  
  // If the some button is pressed and the policy is set to update the value delayed, update the knob's false pointer's angle 
  if ( (knob->button != 0) && (knob->mouse_policy==GIW_KNOB_MOUSE_DELAYED)){
    x = event->x;
    y = event->y;

    if (event->is_hint || (event->window != widget->window))
      gdk_window_get_pointer (widget->window, &x, &y, NULL);

    knob_update_false_mouse (knob, x, y);
  }

  /*if (knob->button != 0)
    {
      x = event->x;
      y = event->y;

      if (event->is_hint || (event->window != widget->window))
        gdk_window_get_pointer (widget->window, &x, &y, NULL);

      knob_update_mouse (knob, x,y);
    }*/
   
  return FALSE;
}

static void
giw_knob_style_set (GtkWidget *widget,
			GtkStyle *previous_style)
{
  GiwKnob *knob;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_KNOB (widget));
  
  knob = GIW_KNOB (widget);
  
  // The only thing to fo is recalculate the layout's sizes
  knob_calculate_legends_sizes(knob);
  knob_calculate_title_sizes(knob);
}

/******************
* Users Functions * 
******************/

gdouble
giw_knob_get_value (GiwKnob *knob)
{
  g_return_val_if_fail (knob != NULL, 0.0);
  g_return_val_if_fail (GIW_IS_KNOB (knob), 0.0);

  return(knob->adjustment->value);
}

void 
giw_knob_set_value (GiwKnob *knob, 
			gdouble value)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  
  
  if (value!=knob->adjustment->value){
    knob_set_value(knob, value);  
    gtk_adjustment_value_changed(knob->adjustment);
  }
}

void
giw_knob_set_adjustment (GiwKnob *knob,
			 GtkAdjustment *adjustment)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  g_return_if_fail (adjustment != NULL);

  // Freeing the last one
  if (knob->adjustment){
    gtk_signal_disconnect_by_data (GTK_OBJECT (knob->adjustment), (gpointer) knob);
    g_object_unref (GTK_OBJECT (knob->adjustment));
  }
   
  knob->adjustment = adjustment;
  gtk_object_ref (GTK_OBJECT (knob->adjustment));
  
  g_signal_connect (GTK_OBJECT (adjustment), "changed",
		    (GCallback) giw_knob_adjustment_changed,
		    (gpointer) knob);
  g_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		    (GCallback) giw_knob_adjustment_value_changed,
		    (gpointer) knob);

  gtk_adjustment_value_changed(knob->adjustment);
    
  gtk_adjustment_changed(knob->adjustment);
}

GtkAdjustment* 
giw_knob_get_adjustment (GiwKnob *knob)
{
  g_return_val_if_fail (knob != NULL, NULL);
  g_return_val_if_fail (GIW_IS_KNOB (knob), NULL);

  return(knob->adjustment);
}

void
giw_knob_set_legends_digits (GiwKnob *knob,
			guint digits_number)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  
  if (digits_number != knob->legends_digits){
    knob_free_legends(knob);
  
    knob->legends_digits=digits_number;
  
    knob_build_legends(knob);
    knob_calculate_sizes(knob);
    gtk_widget_queue_draw(GTK_WIDGET(knob));  
  }
}

void
giw_knob_set_ticks_number (GiwKnob *knob,
			guint major, guint minor)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  
  if ((major != knob->major_ticks) || (minor != knob->minor_ticks)){
    knob_free_legends(knob); 
  
    knob->major_ticks=major;
     
    if (knob->major_ticks==0)
      knob->minor_ticks=0;          // It's impossible to have minor ticks without major ticks
    else
      knob->minor_ticks=minor;
  
    knob_build_legends(knob);
    knob_calculate_sizes(knob);
    gtk_widget_queue_draw(GTK_WIDGET(knob));  
  }
}

void
giw_knob_set_mouse_policy (GiwKnob *knob,
			GiwKnobMousePolicy policy)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  
  if (knob->button==0) // The policy can only be change when there is no button pressed
    knob->mouse_policy=policy;
}

static void
giw_knob_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GiwKnob *knob;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  knob = GIW_KNOB (data);
        
  knob_free_legends(knob);
  knob_build_legends(knob);
  knob_calculate_sizes(knob);
  gtk_widget_queue_draw(GTK_WIDGET(knob));
}

static void
giw_knob_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GiwKnob *knob;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  knob = GIW_KNOB (data);
   
  knob_set_angle(knob, knob_calculate_angle_with_value(knob, adjustment->value));
    
  gtk_widget_queue_draw(GTK_WIDGET(knob));
}

void           
giw_knob_set_title (GiwKnob *knob, gchar *str)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
    
  knob->title_str=g_strdup(str); // Duplicate the string, after this, str can be freed
  
  knob_build_title(knob);
  knob_calculate_sizes(knob);
  gtk_widget_queue_draw(GTK_WIDGET(knob));
}

/******************
* Local Functions * 
******************/

void
knob_update_mouse (GiwKnob *knob, gint x, gint y)
{
  gint xc, yc;
  
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));

  xc = GTK_WIDGET(knob)->allocation.width / 2;
  yc = GTK_WIDGET(knob)->allocation.height / 2;

  // Calculating the new angle
  if (knob->angle != atan2(yc-y, x-xc)){
    knob_set_value(knob, knob_calculate_value_with_angle(knob, atan2(yc-y, x-xc)));
    gtk_adjustment_value_changed(knob->adjustment);
  }
}

void
knob_update_false_mouse (GiwKnob *knob, gint x, gint y)
{
  gint xc, yc;
  
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));

  xc = GTK_WIDGET(knob)->allocation.width / 2;
  yc = GTK_WIDGET(knob)->allocation.height / 2;

  // Calculating the new angle
  knob->false_angle = atan2(yc-y, x-xc);
  
  // Putting the angle between 0 and 2PI, because the atan2 returns the angle between PI and -PI
  while (knob->false_angle<0)
    knob->false_angle+=(2.0*M_PI);
  
  // Taking out of the "forbideen" region
  if ( (knob->false_angle <= (3.0*M_PI/2.0)) && 
       (knob->false_angle > (5.0*M_PI/4.0)) )
    knob->false_angle=5.0*M_PI/4.0;
  if ( (knob->false_angle < (7.0*M_PI/4.0)) && 
       (knob->false_angle >= (3.0*M_PI/2.0)))
    knob->false_angle=7.0*M_PI/4.0; 
  
  gtk_widget_queue_draw(GTK_WIDGET(knob));  
}

static void 
knob_calculate_sizes (GiwKnob *knob)
{
  GtkWidget *widget;
  
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
   
  widget=GTK_WIDGET(knob);
  
  // Getting the radius and size
  if (widget->allocation.width < widget->allocation.height){
    knob->size=widget->allocation.width;
    knob->x=0;
    knob->y=widget->allocation.height/2-knob->size/2;
  }
  else{
    knob->size=widget->allocation.height;
    knob->y=0;
    knob->x=widget->allocation.width/2-knob->size/2;
  }
    
  // The distance between the radius and the widget limits is the bigger dimension of the legends plus the major_ticks_size, so it's the half of size, less the bigger dimension of the legends less the major_ticks size (wich depends of the radius), them, with some algebra, it results in this equation:
  knob->radius=8*((knob->size/2)-sqrt(knob->legend_width*knob->legend_width+knob->legend_height*knob->legend_height))/9; 
   
  knob->d_major_ticks=(3.0*M_PI/2.0)/(knob->major_ticks-1);
  knob->d_minor_ticks=knob->d_major_ticks/(knob->minor_ticks+1);  
  
  knob->major_ticks_size=knob->radius/8.0;
  knob->minor_ticks_size=knob->radius/16.0;
  
  // The legend will in the middle of the inside (plus the major_ticks_size) and outside circle 
  knob->legend_radius=((knob->radius+knob->major_ticks_size+(knob->size/2))/2);
}

gdouble
knob_calculate_value_with_angle (GiwKnob *knob, gdouble angle)
{
  gdouble d_angle = 0.0; // How many the pointer is far from the lower angle (5PI/4)

  g_return_val_if_fail (knob != NULL, 0.0);
  g_return_val_if_fail (GIW_IS_KNOB (knob), 0.0);
  
   // Putting the angle between 0 and 2PI, because the atan2 returns the angle between PI and -PI
  while (angle<0)
    angle=angle+(2.0*M_PI);
  
  // Taking out of the "forbideen" region
  if ((angle <= (3.0*M_PI/2.0)) && (angle  > (5.0*M_PI/4.0))) angle=5.0*M_PI/4.0;
  if ((angle  < (7.0*M_PI/4.0)) && (angle >= (3.0*M_PI/2.0))) angle=7.0*M_PI/4.0; 
   
  // Calculating the distance (in radians) between the pointer and the lower angle   
  if (angle<=(5.0*M_PI/4.0)) d_angle=(5.0*M_PI/4.0)-angle;
  if (angle>=(7.0*M_PI/4.0)) d_angle=(13.0*M_PI/4.0)-angle;

  return(knob->adjustment->lower+fabs(knob->adjustment->upper-knob->adjustment->lower)*d_angle/(3.0*M_PI/2.0));
}

gdouble 
knob_calculate_angle_with_value (GiwKnob *knob, gdouble value)
{
  gdouble angle;
  
  g_return_val_if_fail (knob != NULL, 0.0);
  g_return_val_if_fail (GIW_IS_KNOB (knob), 0.0);
  
  angle=(value-knob->adjustment->lower)*(3.0*M_PI/2.0)/fabs(knob->adjustment->upper-knob->adjustment->lower);
    
    // Now, the angle is relative to the 3 o'clock position, and need to be changed in order to be ralative to the initial angle ((5.0*M_PI/4.0)
  angle=(5.0*M_PI/4.0)-angle; 
  
  return(angle);    
}

void 
knob_set_angle (GiwKnob *knob,
			gdouble angle)
{

  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  
  // Putting the angle between 0 and 2PI(360�)
  while (angle > 2.0*M_PI)
    angle=angle-(2.0*M_PI);
  
  while (angle<0)
    angle=angle+(2.0*M_PI);
    
  if (knob->angle != angle){
    // Taking out of the "forbideen" region
    if ((angle <= (3.0*M_PI/2.0)) && (angle > (5.0*M_PI/4.0))) angle=5.0*M_PI/4.0;
    if ((angle  < (7.0*M_PI/4.0)) && (angle >= (3.0*M_PI/2.0))) angle=7.0*M_PI/4.0; 
            
    knob->angle=angle;
  }
}

void
knob_set_value (GiwKnob *knob,
			gdouble value)
{
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GIW_IS_KNOB (knob));
  
  knob->adjustment->value=value;
}

void
knob_build_legends(GiwKnob *knob)
{
  GtkWidget *widget;
  gint loop;
  gchar *str;

  g_return_if_fail (knob != NULL);
  
  widget=GTK_WIDGET(knob);
  
  if (knob->major_ticks==0)  // Preventing from bugs
    return;
     
  // Creating the legend's layouts
  if (knob->legends_digits!=0){
    knob->legends=g_new(PangoLayout*, knob->major_ticks);
    str=g_new(gchar, knob->legends_digits+1); // +1 for the '/0'
    for (loop=0; loop<knob->major_ticks; loop++){
      snprintf(str,knob->legends_digits+1,"%f",knob->adjustment->lower+loop*(knob->adjustment->upper-knob->adjustment->lower)/(knob->major_ticks-1)); // Creating the legends string
      knob->legends[loop]=gtk_widget_create_pango_layout (widget, str); 
    }
    g_free(str);
    
    // Getting the size of the legends 
    knob_calculate_legends_sizes(knob);
  }else{ // If there are no legends (0 digits), the size is the major ticks size (5)
      knob->legend_width=0;
      knob->legend_height=0;
  }
}

void
knob_free_legends(GiwKnob *knob)
{
  gint loop;

  g_return_if_fail (knob != NULL);
    
  if (knob->legends!=NULL){
    for (loop=0; loop<knob->major_ticks; loop++)
      if (knob->legends[loop]!=NULL)
        g_object_unref(G_OBJECT(knob->legends[loop]));
    g_free(knob->legends);
    knob->legends=NULL;    
  }
}

void
knob_build_title (GiwKnob *knob)
{
  GtkWidget *widget;
  
  g_return_if_fail (knob != NULL);
    
  widget=GTK_WIDGET(knob);
  
  if (knob->title_str==NULL) // Return if there is no title (the layout will be keeped, but not drawed)
    return;
        
  if (knob->title) 
      pango_layout_set_text(knob->title, knob->title_str, strlen(knob->title_str));
  else // If the title hasn't been created yet..
      knob->title=gtk_widget_create_pango_layout(widget, knob->title_str);
  
  // Calculating new size
  knob_calculate_title_sizes(knob);
}

void
knob_calculate_legends_sizes(GiwKnob *knob)
{
  GtkWidget *widget;
    
  g_return_if_fail (knob != NULL);
  
  widget=GTK_WIDGET(knob);
  
  if (knob->legends!=NULL){
    pango_layout_set_font_description (knob->legends[0], widget->style->font_desc);  
    pango_layout_get_size(knob->legends[0], &(knob->legend_width), &(knob->legend_height));
    knob->legend_width/=PANGO_SCALE;
    knob->legend_height/=PANGO_SCALE;
  }
} 

void
knob_calculate_title_sizes(GiwKnob *knob) 
{
  GtkWidget *widget;
    
  g_return_if_fail (knob != NULL);
  
  if (knob->title == NULL) return;

  widget=GTK_WIDGET(knob);

  pango_layout_set_font_description (knob->title, widget->style->font_desc);  
  pango_layout_get_size(knob->title, &(knob->title_width), &(knob->title_height));
  knob->title_width/=PANGO_SCALE;
  knob->title_height/=PANGO_SCALE;

  knob_calculate_sizes(knob);
}
