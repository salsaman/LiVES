/* giwvslider.h  -  GiwVSlider widget's header
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



#ifndef __GIW_VSLIDER_H__
#define __GIW_VSLIDER_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIW_TYPE_VSLIDER                 (giw_vslider_get_type ())
#define GIW_VSLIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIW_TYPE_VSLIDER, GiwVSlider))
#define GIW_VSLIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GIW_TYPE_VSLIDER, GiwVSliderClass))
#define GIW_IS_VSLIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIW_TYPE_VSLIDER))
#define GIW_IS_VSLIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GIW_TYPE_VSLIDER))
#define GIW_VSLIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GIW_TYPE_VSLIDER, GiwVSliderClass))

typedef enum {
  GIW_VSLIDER_MOUSE_DISABLED,
  GIW_VSLIDER_MOUSE_AUTOMATICALLY,
  GIW_VSLIDER_MOUSE_DELAYED
} GiwVSliderMousePolicy;


typedef struct _GiwVSlider        GiwVSlider;
typedef struct _GiwVSliderClass   GiwVSliderClass;

struct _GiwVSlider {
#if GTK_CHECK_VERSION(3,0,0)
  GtkWidget parent_instance;
#endif
  GtkWidget widget;

  GtkAdjustment *adjustment;

  gint major_ticks; // Number of major ticks in the vslider
  gint minor_ticks; // Number of minor ticks between each major ticks
  gint legend_digits; //Number of digits of the legend (counting the "." and the signal "-" if exists)

  // Button currently pressed or 0 if none
  guint8 button;

  // Policy of mouse (GIW_VSLIDER_MOUSE_DISABLED, GIW_VSLIDER_MOUSE_AUTOMATICALLY, GIW_VSLIDER_MOUSE_DELAYED)
  gint mouse_policy;

  // phanton value, the phanton button's value when using the mouse delayed mouse policy
  gdouble phanton_value;

  // For drawing:
  gint height, width; // VSlider height and width
  gint x, y; // Position of the vslider image relative to te widget window corner
  gdouble major_dy; //Space between major ticks
  gdouble minor_dy; //Space between minor ticks
  gint legend_width, legend_height;
  gint button_x, button_y, button_w, button_h; // The button position and dimensions
  gint pbutton_x, pbutton_y, pbutton_w, pbutton_h; // The phanton button position and dimensions
  PangoLayout **legends; // All the layouts of all the legends

  // Types for drawing the button (they change when the button in pressed, or the pointer is over it)
  GtkStateType button_state;
  GtkShadowType button_shadow;

  gulong chsig;
  gulong vchsig;
};

struct _GiwVSliderClass {
  GtkWidgetClass parent_class;
};




GtkWidget     *giw_vslider_new(GtkAdjustment *adjustment);
GtkWidget     *giw_vslider_new_with_adjustment(gdouble value, gdouble lower, gdouble upper);
GType          giw_vslider_get_type(void) G_GNUC_CONST;
void           giw_vslider_set_value(GiwVSlider *vslider, gdouble value);
gdouble        giw_vslider_get_value(GiwVSlider *vslider);
GtkAdjustment *giw_vslider_get_adjustment(GiwVSlider *vslider);
void           giw_vslider_set_adjustment(GiwVSlider *vslider, GtkAdjustment *adjustment);
void           giw_vslider_set_legends_digits(GiwVSlider *vslider, gint digits);
void           giw_vslider_set_mouse_policy(GiwVSlider *vslider, GiwVSliderMousePolicy policy);
void           giw_vslider_set_major_ticks_number(GiwVSlider *vslider, gint number);
void           giw_vslider_set_minor_ticks_number(GiwVSlider *vslider, gint number);

G_END_DECLS

#endif /* __GIW_VSLIDER_H__ */
