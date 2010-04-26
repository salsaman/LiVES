/* giwvslider.c  -  GiwVSlider widget's source
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

// additional code G. Finch (salsaman@gmail.com) 2010 - 


#include <stdio.h>
#include <string.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include "giw/giwvslider.h" 




#define VSLIDER_DEFAULT_WIDTH 25
#define VSLIDER_DEFAULT_HEIGHT 50

/* Forward declarations */
static void giw_vslider_class_init               (GiwVSliderClass    *klass);
static void giw_vslider_init                     (GiwVSlider         *vslider);
static void giw_vslider_destroy                  (GtkObject        *object);
static void giw_vslider_realize                  (GtkWidget        *widget);
static void giw_vslider_size_request             (GtkWidget      *widget,
					       GtkRequisition *requisition);
static void giw_vslider_size_allocate            (GtkWidget     *widget,
					       GtkAllocation *allocation);
static gint giw_vslider_expose                   (GtkWidget        *widget,
						GdkEventExpose   *event);
static gint giw_vslider_button_press             (GtkWidget        *widget,
						GdkEventButton   *event);
static gint giw_vslider_button_release           (GtkWidget        *widget,
						GdkEventButton   *event);
static gint giw_vslider_motion_notify            (GtkWidget        *widget,
						GdkEventMotion   *event);
static void giw_vslider_adjustment_changed       (GtkAdjustment    *adjustment,
						gpointer          data);
static void giw_vslider_adjustment_value_changed (GtkAdjustment    *adjustment,
						gpointer          data);
static void giw_vslider_style_set	              (GtkWidget      *widget,
			                        GtkStyle       *previous_style);

/* Local data and functions */

static GtkWidgetClass *parent_class = NULL;

// A function that calculates distances and sizes for a later drawing..
void vslider_calculate_sizes(GiwVSlider *vslider); 

// A function that calculates position and size of the button
void vslider_calculate_button(GiwVSlider *vslider); 

// A function that calculates position and size of the phanton button
void vslider_calculate_phanton_button(GiwVSlider *vslider); 

// A function that creates the layout of legends and calculates it's sizes
void vslider_build_legends(GiwVSlider *vslider); 

// A function that frees the layout of legends 
void vslider_free_legends(GiwVSlider *vslider); 

// A function that calculates width and height of the legend's the layout
void vslider_calculate_legends_sizes(GiwVSlider *vslider); 

guint
giw_vslider_get_type ()
{
  static guint vslider_type = 0;

  if (!vslider_type)
    {
      GtkTypeInfo vslider_info =
      {
	"GiwVSlider",
	sizeof (GiwVSlider),
	sizeof (GiwVSliderClass),
	(GtkClassInitFunc) giw_vslider_class_init,
	(GtkObjectInitFunc) giw_vslider_init,
	/*(GtkArgSetFunc)*/ NULL,
	/*(GtkArgGetFunc)*/ NULL,
      };

      vslider_type = gtk_type_unique (gtk_widget_get_type (), &vslider_info);
    }

  return vslider_type;
}

static void
giw_vslider_class_init (GiwVSliderClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  object_class->destroy = giw_vslider_destroy;

  widget_class->realize = giw_vslider_realize;
  widget_class->expose_event = giw_vslider_expose;
  widget_class->size_request = giw_vslider_size_request;
  widget_class->size_allocate = giw_vslider_size_allocate;
  widget_class->button_press_event = giw_vslider_button_press;
  widget_class->button_release_event = giw_vslider_button_release;
  widget_class->motion_notify_event = giw_vslider_motion_notify;
  widget_class->style_set = giw_vslider_style_set;
}

static void
giw_vslider_init (GiwVSlider *vslider)
{
  //Defaults
  vslider->legend_digits=3;
  vslider->major_ticks=3;
  vslider->minor_ticks=1;  
  
  // Default mouse policy : automatically
  vslider->mouse_policy=GIW_VSLIDER_MOUSE_AUTOMATICALLY;
    
  vslider->button=FALSE;
  
  // Conditions for a unpressed button
  vslider->button_state=GTK_STATE_NORMAL;
  vslider->button_shadow=GTK_SHADOW_OUT;
}

GtkWidget*
giw_vslider_new (GtkAdjustment *adjustment)
{
  GiwVSlider *vslider;

  g_return_val_if_fail (adjustment != NULL, NULL);
  
  vslider = gtk_type_new (giw_vslider_get_type ());
  
  giw_vslider_set_adjustment(vslider, adjustment);
  
  return GTK_WIDGET (vslider);
}

GtkWidget*
giw_vslider_new_with_adjustment (gdouble value,
			 gdouble lower,
			 gdouble upper)
{
  GiwVSlider *vslider;
  
  vslider = gtk_type_new (giw_vslider_get_type ());
  
  giw_vslider_set_adjustment(vslider, (GtkAdjustment*) gtk_adjustment_new (value, lower, upper, 1.0, 1.0, 1.0));
  
  return GTK_WIDGET (vslider);
}

static void
giw_vslider_destroy (GtkObject *object)
{
  GiwVSlider *vslider;
  gint loop;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (object));

  vslider = GIW_VSLIDER (object);
    
  // Freeing all
  if (vslider->adjustment)
    g_object_unref (G_OBJECT (vslider->adjustment));
  vslider->adjustment=NULL;
  if (vslider->legends) {
    for (loop=0; loop<vslider->major_ticks; loop++){
      if (vslider->legends[loop]!=NULL) g_object_unref(G_OBJECT(vslider->legends[loop]));
    }
    g_free(vslider->legends);
    vslider->legends=NULL;
  }
    
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
giw_vslider_realize (GtkWidget *widget)
{
  GiwVSlider *vslider;
  GdkWindowAttr attributes;
  gint attributes_mask;
 
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  vslider = GIW_VSLIDER (widget);

  // Creating the widget->window
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget) | 
      GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
      GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

  // Creating the widget->style
  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_user_data (widget->window, widget);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);
  
  // Creating the legends
  vslider_build_legends(vslider);
}

static void 
giw_vslider_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  requisition->width = VSLIDER_DEFAULT_WIDTH;
  requisition->height = VSLIDER_DEFAULT_HEIGHT;
}

static void
giw_vslider_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GiwVSlider *vslider;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  vslider = GIW_VSLIDER (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {

      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

    }
    
  // If the window size has changed, all the distances need to be recalculated 
  vslider_calculate_sizes(vslider);
}

static gint
giw_vslider_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  GiwVSlider *vslider;
  gint loop, loop2;
  GdkRectangle rect;
      
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_VSLIDER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
  
  vslider = GIW_VSLIDER (widget);

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
        
  // Drawing all the ticks and legends
  for (loop=0; loop<vslider->major_ticks; loop++){
    // Legends
    if (vslider->legend_digits!=0)
      gtk_paint_layout (widget->style,
			widget->window,
			GTK_STATE_NORMAL,
			FALSE,
			&rect,
			widget,
			NULL,
                        0,
			vslider->y+(gint)(vslider->major_dy*(gdouble)loop)-(vslider->legend_height/2),
			vslider->legends[loop]);
    // Major ticks
    gdk_draw_line(widget->window, 
			widget->style->fg_gc[widget->state],
			vslider->legend_width-5, 
			vslider->y+(gint)(vslider->major_dy*(gdouble)loop),
			vslider->legend_width,
			vslider->y+(gint)(vslider->major_dy*(gdouble)loop));
    // Minor ticks
    for (loop2=1; loop2<vslider->minor_ticks+1; loop2++)
      if (loop!=vslider->major_ticks-1) // If it's not the last one
        gdk_draw_line(widget->window, 
			    widget->style->fg_gc[widget->state],
			    vslider->legend_width-3, 
			    vslider->y+(gint)(vslider->major_dy*(gdouble)loop)+vslider->minor_dy*(gdouble)loop2,
			    vslider->legend_width,
			    vslider->y+(gint)(vslider->major_dy*(gdouble)loop)+vslider->minor_dy*(gdouble)loop2);
  }
   
  //Drawing the back hole
  gdk_draw_rectangle (widget->window,
	                widget->style->black_gc,
			TRUE,
			vslider->x+vslider->width/2-2,
			vslider->y,
			4,
			vslider->height);
  gdk_draw_rectangle (widget->window,
	                widget->style->bg_gc[widget->state],
			FALSE,
			vslider->x+vslider->width/2-2,
			vslider->y,
			4,
			vslider->height);

  // The button
  /*  gtk_paint_slider (widget->style,
			widget->window,
		        vslider->button_state,
			vslider->button_shadow,
			&rect,
			widget,
			NULL,
			vslider->button_x,
			vslider->button_y,
			vslider->button_w,
			vslider->button_h,
			GTK_ORIENTATION_VERTICAL); */

  gdk_draw_rectangle (widget->window,
	                widget->style->fg_gc[widget->state],
			TRUE,
			vslider->button_x,
			vslider->button_y,
			vslider->button_w,
			vslider->button_h);

  // The phanton button
  if ((vslider->mouse_policy == GIW_VSLIDER_MOUSE_DELAYED) && (vslider->button !=0 ))
    gtk_paint_slider (widget->style,
			widget->window,
			GTK_STATE_ACTIVE,
			GTK_SHADOW_ETCHED_OUT,
			&rect,
			widget,
			NULL,
			vslider->pbutton_x,
			vslider->pbutton_y,
			vslider->pbutton_w,
			vslider->pbutton_h,
			GTK_ORIENTATION_VERTICAL);
 
  return(0);
}

static gint 
giw_vslider_button_press (GtkWidget        *widget,
			GdkEventButton   *event)
{
  GiwVSlider *vslider;
 
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_VSLIDER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
    
  vslider=GIW_VSLIDER(widget);
  
  if (vslider->mouse_policy==GIW_VSLIDER_MOUSE_DISABLED) return(FALSE);
  
  // Checking if the pointer is inside the button
  if (event->x > vslider->button_x && event->x < vslider->button_x+ vslider->button_w && event->y > vslider->button_y && event->y < vslider->button_y+vslider->button_h){
    if (vslider->button==0){
      vslider->button=event->button;
      vslider->button_state=GTK_STATE_ACTIVE;
      vslider->button_shadow=GTK_SHADOW_IN;
    }
  }
    
  gtk_widget_queue_draw(GTK_WIDGET(vslider));
     
  return(TRUE);
}

static gint 
giw_vslider_button_release (GtkWidget        *widget,
			GdkEventButton   *event)
{
  GiwVSlider *vslider;
  gint x, y;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_VSLIDER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
    
  vslider=GIW_VSLIDER(widget);
  
  if (vslider->mouse_policy==GIW_VSLIDER_MOUSE_DISABLED) return(FALSE);
  
  // Get the position inside the vslider
  x=event->x-vslider->x;
  y=event->y-vslider->y;
  
  if (vslider->button == event->button){
    // If the mouse policy is to update delayed, it's time to update the value (the button was released)
    if (vslider->mouse_policy==GIW_VSLIDER_MOUSE_DELAYED)
      giw_vslider_set_value(vslider, vslider->phanton_value);
    
    vslider->button=FALSE;
    
    // If the mouse stills inside the button, state is prelight, else state is normal
    if (event->x > vslider->button_x && event->x < vslider->button_x+ vslider->button_w && event->y >     vslider->button_y && event->y < vslider->button_y+vslider->button_h)
      vslider->button_state=GTK_STATE_PRELIGHT;
    else
      vslider->button_state=GTK_STATE_NORMAL;
      
    vslider->button_shadow=GTK_SHADOW_OUT;  // Return to normal shadow
  }
  
  gtk_widget_queue_draw(GTK_WIDGET(vslider));
    
  return(TRUE);
}

static gint 
giw_vslider_motion_notify (GtkWidget        *widget,
			GdkEventMotion   *event)
{
  GiwVSlider *vslider;
  gint x, y;
  gdouble new_value;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GIW_IS_VSLIDER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
    
  vslider=GIW_VSLIDER(widget);
  
  if (vslider->mouse_policy==GIW_VSLIDER_MOUSE_DISABLED) return(FALSE);
      
  // Get the position inside the vslider
  x=event->x-vslider->x;
  y=event->y-vslider->y;
  
  // If the mouse policy is to update automatically, change the value now if it's valid
  if (vslider->button!=0 && vslider->mouse_policy==GIW_VSLIDER_MOUSE_AUTOMATICALLY){
    new_value=vslider->adjustment->lower+((gdouble)(vslider->height-y))/((gdouble)vslider->height)*(vslider->adjustment->upper-vslider->adjustment->lower);
    // If it's a valid value, update it!
    if ( (new_value <= vslider->adjustment->upper) && 
         (new_value >= vslider->adjustment->lower) )
      giw_vslider_set_value(vslider, new_value);
  }
  
  // If the mouse policy is to update delayed, calculate the false value
  if (vslider->button!=0 && vslider->mouse_policy==GIW_VSLIDER_MOUSE_DELAYED){
    new_value=vslider->adjustment->lower+((gdouble)(vslider->height-y))/((gdouble)vslider->height)*(vslider->adjustment->upper-vslider->adjustment->lower);
  
    // If it's a valid value, update the false value
    if ( (new_value <= vslider->adjustment->upper) && 
         (new_value >= vslider->adjustment->lower) ){
      vslider->phanton_value=new_value;
      vslider_calculate_phanton_button(vslider);
      gtk_widget_queue_draw(GTK_WIDGET(vslider));
    }
  }
  
  // If the button is not pressed and the pointer is inside the button, set prelight, if the pointer is out  of the button, set normal
  if (vslider->button==0){
    if (event->x > vslider->button_x && event->x < vslider->button_x+ vslider->button_w && event->y > vslider->button_y && event->y < vslider->button_y+vslider->button_h)
      vslider->button_state=GTK_STATE_PRELIGHT;
    else
      vslider->button_state=GTK_STATE_NORMAL;
  }
  
  gtk_widget_queue_draw(GTK_WIDGET(vslider));
    
  return(TRUE);
}

static void
giw_vslider_style_set (GtkWidget *widget,
			GtkStyle *previous_style)
{
  GiwVSlider *vslider;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (widget));
  
  vslider = GIW_VSLIDER (widget);
  
  // The only thing to fo is recalculate the layout's sizes
  vslider_calculate_legends_sizes(vslider);
}

void
giw_vslider_set_value (GiwVSlider *vslider,
			gdouble value)
{
  g_return_if_fail (vslider != NULL); 
  g_return_if_fail (GIW_IS_VSLIDER (vslider));
  
  if (vslider->adjustment->value!=value){
    vslider->adjustment->value=value;
    
    gtk_adjustment_value_changed(vslider->adjustment);
  }
}

gdouble
giw_vslider_get_value (GiwVSlider *vslider)
{
   g_return_val_if_fail (vslider != NULL, 0.0); 
   g_return_val_if_fail (GIW_IS_VSLIDER (vslider), 0.0);
   g_return_val_if_fail (vslider->adjustment != NULL, 0.0);
 
   return(vslider->adjustment->value);
}

GtkAdjustment*
giw_vslider_get_adjustment (GiwVSlider *vslider)
{
  g_return_val_if_fail (vslider != NULL, NULL);
  g_return_val_if_fail (GIW_IS_VSLIDER (vslider), NULL);

  return(vslider->adjustment);
}

void
giw_vslider_set_adjustment (GiwVSlider *vslider,
			 GtkAdjustment *adjustment)
{
  g_return_if_fail (vslider != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (vslider));
  g_return_if_fail (adjustment != NULL);

  // Freeing the last one
  if (vslider->adjustment){
    gtk_signal_disconnect_by_data (GTK_OBJECT (vslider->adjustment), (gpointer) vslider);
    g_object_unref (G_OBJECT (vslider->adjustment));
    vslider->adjustment=NULL;
  }
   
  vslider->adjustment = adjustment;
  g_object_ref (GTK_OBJECT (vslider->adjustment));
  
  g_signal_connect (GTK_OBJECT (adjustment), "changed",
		      (GtkSignalFunc) giw_vslider_adjustment_changed,
		      (gpointer) vslider);
  g_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      (GtkSignalFunc) giw_vslider_adjustment_value_changed,
		      (gpointer) vslider);

  gtk_adjustment_value_changed(vslider->adjustment);
    
  gtk_adjustment_changed(vslider->adjustment);
}

void 
giw_vslider_set_legends_digits (GiwVSlider *vslider,
			gint digits)
{
  g_return_if_fail (vslider != NULL);  
  g_return_if_fail (GIW_IS_VSLIDER (vslider));

  if (vslider->legend_digits!=digits){
    vslider_free_legends(vslider);

    vslider->legend_digits=digits;
    vslider_build_legends(vslider);
    vslider_calculate_sizes(vslider);
    
    gtk_widget_queue_draw(GTK_WIDGET(vslider));
  }
}

void
giw_vslider_set_mouse_policy (GiwVSlider *vslider,
			GiwVSliderMousePolicy policy)
{
  g_return_if_fail (vslider != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (vslider));

  vslider->mouse_policy=policy;
}

void
giw_vslider_set_major_ticks_number (GiwVSlider *vslider,
					 gint number)
{
  g_return_if_fail (vslider != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (vslider));

  if (number!=vslider->major_ticks){
    // Freeing the old legends, because they will be changed
    vslider_free_legends(vslider);
  
    vslider->major_ticks=number;
  
    // Now, there are diferent legends, so, we have to create them
    vslider_build_legends(vslider);
    
    vslider_calculate_sizes(vslider);
    gtk_widget_queue_draw(GTK_WIDGET(vslider));
  }
}

void
giw_vslider_set_minor_ticks_number (GiwVSlider *vslider,
					gint number)
{
  g_return_if_fail (vslider != NULL);
  g_return_if_fail (GIW_IS_VSLIDER (vslider));

  vslider->minor_ticks=number;
  
  vslider_calculate_sizes(vslider);
  gtk_widget_queue_draw(GTK_WIDGET(vslider));
}

static void
giw_vslider_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GiwVSlider *vslider;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  vslider = GIW_VSLIDER (data);
  
  // The range  has changed, so the legends changed too
  vslider_free_legends(vslider);
  vslider_build_legends(vslider);
        
  // Recalculating everything
  vslider_calculate_sizes(vslider);
  gtk_widget_queue_draw(GTK_WIDGET(vslider));
}

static void
giw_vslider_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GiwVSlider *vslider;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GIW_IS_VSLIDER(data));

  vslider = GIW_VSLIDER (data);
  
  // Recalculating the button position
  vslider_calculate_button(vslider);
  gtk_widget_queue_draw(GTK_WIDGET(vslider));
}

void 
vslider_calculate_sizes(GiwVSlider *vslider)
{
  GtkWidget *widget;
  
  g_return_if_fail (vslider != NULL);
  
  widget=GTK_WIDGET(vslider);
          
  // Calculating the sizes and distances...
  vslider->width=widget->allocation.width-vslider->legend_width;
  vslider->height=widget->allocation.height-vslider->legend_height;
  vslider->x=vslider->legend_width;
  vslider->y=vslider->legend_height/2;

  // If the legends are too small, or inexistant, it has to be some space left fot the button (when it's on he edge)
  if (vslider->y < vslider->button_h/2){
    vslider->height = widget->allocation.height-vslider->button_h;
    vslider->y = vslider->button_h/2;
  }

  /* The height of the vslider is the widget's height less a half of legend height (beacause a half of the legends are drawn outside the vslider's area .
     If there are no legends the temometer width is the widget's width, is there are legends, the width of the vslider is the space left by the legends.*/
    
  vslider->major_dy=(gdouble)vslider->height/(gdouble)(vslider->major_ticks-1);
  vslider->minor_dy=vslider->major_dy/(vslider->minor_ticks+1);

  vslider_calculate_button(vslider);
  vslider_calculate_phanton_button(vslider);
}

void
vslider_calculate_button (GiwVSlider *vslider)
{
  g_return_if_fail (vslider != NULL);

  // Getting button's positon and dimensions
  vslider->button_x=vslider->x+5;
  vslider->button_y=vslider->y+vslider->height-vslider->height*((vslider->adjustment->value-vslider->adjustment->lower)/(vslider->adjustment->upper-vslider->adjustment->lower))-5;
  vslider->button_w=vslider->width-10;
  vslider->button_h=10;
} 

void
vslider_calculate_phanton_button(GiwVSlider *vslider)
{
  g_return_if_fail (vslider != NULL);

  // Getting phanton button's positon and dimensions
  vslider->pbutton_x=vslider->x+5;
  vslider->pbutton_y=vslider->y+vslider->height-vslider->height*((vslider->phanton_value-vslider->adjustment->lower)/(vslider->adjustment->upper-vslider->adjustment->lower))-5;
  vslider->pbutton_w=vslider->width-10;
  vslider->pbutton_h=10;
}

void 
vslider_build_legends(GiwVSlider *vslider)
{
  GtkWidget *widget;
  gint loop;
  gchar *str;

  g_return_if_fail (vslider != NULL);
  
  widget=GTK_WIDGET(vslider);
  
  if (vslider->major_ticks==0)  // Preventing from bugs
    return;
    
  // Creating the legend's layouts
  if (vslider->legend_digits!=0){
    vslider->legends=g_new(PangoLayout*, vslider->major_ticks);
    str=g_new(gchar, vslider->legend_digits+1); // +1 for the '/0'
    for (loop=0; loop<vslider->major_ticks; loop++){
      snprintf(str,vslider->legend_digits+1,"%f",vslider->adjustment->lower+(vslider->major_ticks-1-loop)*(vslider->adjustment->upper-vslider->adjustment->lower)/(vslider->major_ticks-1)); // Creating the legends string
      vslider->legends[loop]=gtk_widget_create_pango_layout (widget, str); 
      pango_layout_set_font_description(vslider->legends[loop], widget->style->font_desc); // Setting te correct font  
    }
    g_free(str);
  
    // Calculating sizes
    vslider_calculate_legends_sizes(vslider);
  }else{ // If there are no legends (0 digits), the size is the major ticks size (5)
      vslider->legend_width=5;
      vslider->legend_height=0;
  }
}

void 
vslider_free_legends(GiwVSlider *vslider)
{
  gint loop;

  g_return_if_fail (vslider != NULL);
  
  if (vslider->legends!=NULL){
    for (loop=0; loop<vslider->major_ticks; loop++) {
      if (vslider->legends[loop]!=NULL) {
        g_object_unref(G_OBJECT(vslider->legends[loop]));
      }
    }
    g_free(vslider->legends);
    vslider->legends=NULL;
  }
}

void
vslider_calculate_legends_sizes(GiwVSlider *vslider)
{
  GtkWidget *widget;
    
  g_return_if_fail (vslider != NULL);

  widget=GTK_WIDGET(vslider);

  if (vslider->legends!=NULL){
    pango_layout_set_font_description (vslider->legends[0], widget->style->font_desc);  
    pango_layout_get_size(vslider->legends[0], &(vslider->legend_width), &(vslider->legend_height));
    vslider->legend_width/=PANGO_SCALE;
    vslider->legend_height/=PANGO_SCALE;
    vslider->legend_width+=5;  // Five pixels for the ticks
  

    vslider_calculate_sizes(vslider);
  }
} 
