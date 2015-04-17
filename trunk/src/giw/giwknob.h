/* giwknob.h  -  GiwKnob widget's header
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



#ifndef __GIW_KNOB_H__
#define __GIW_KNOB_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIW_TYPE_KNOB                 (giw_knob_get_type ())
#define GIW_KNOB(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIW_TYPE_KNOB, GiwKnob))
#define GIW_KNOB_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GIW_TYPE_KNOB, GiwKnobClass))
#define GIW_IS_KNOB(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIW_TYPE_KNOB))
#define GIW_IS_KNOB_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GIW_TYPE_KNOB))
#define GIW_KNOB_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GIW_TYPE_KNOB, GiwKnobClass))

typedef enum {
  GIW_KNOB_MOUSE_DISABLED,
  GIW_KNOB_MOUSE_AUTOMATICALLY,
  GIW_KNOB_MOUSE_DELAYED
} GiwKnobMousePolicy;

typedef struct _GiwKnob        GiwKnob;
typedef struct _GiwKnobClass   GiwKnobClass;

struct _GiwKnob {
#if GTK_CHECK_VERSION(3,0,0)
  GtkWidget parent_instance;
#endif

  GtkWidget widget;

  // Dimensions of knob components
  gint radius;
  //gint pointer_width, pointer_size;

  // Current angle, relative to east position (in radians)
  gdouble angle;

  // Angle of the false-pointer, used in the delayed mouse policy
  gdouble false_angle;

  // Button currently pressed or 0 if none
  guint8 button;

  // Policy of mouse (GIW_KNOB_MOUSE_DISABLED, GIW_KNOB_MOUSE_AUTOMATICALLY, GIW_KNOB_MOUSE_DELAYED)
  gint mouse_policy;

  // The size and position relative tho the widget window of the circle
  guint x, y, size;

  // Number of major ticks, and how many minor ticks there will be between each major ticks
  guint major_ticks, minor_ticks;

  // Distance in radians between each major ticks, and between each minor ticks
  gdouble d_major_ticks, d_minor_ticks;

  // Size of the ticks
  gint major_ticks_size, minor_ticks_size;

  // Adjustment, that represents the value and his lower and upper value
  GtkAdjustment *adjustment;

  // Number of digits of the legends
  gint legends_digits;

  // Sizes of the legends
  gint legend_width, legend_height;

  // Distance between the center and the center of each legend layout
  guint legend_radius;

  // The layouts of the legends
  PangoLayout **legends;

  // The title's string
  gchar *title_str;

  // The title's layout
  PangoLayout *title;

  // The title's dimensions
  gint title_width, title_height;

  gulong chsig;
  gulong vchsig;

};

struct _GiwKnobClass {
  GtkWidgetClass parent_class;
};


GType          giw_knob_get_type(void) G_GNUC_CONST;
GtkWidget     *giw_knob_new(GtkAdjustment *adjustment);
GtkWidget     *giw_knob_new_with_adjustment(gdouble value, gdouble lower, gdouble upper);
gdouble        giw_knob_get_value(GiwKnob *knob);
void           giw_knob_set_value(GiwKnob *knob, gdouble value);
void           giw_knob_set_adjustment(GiwKnob *knob, GtkAdjustment *adjustment);
GtkAdjustment *giw_knob_get_adjustment(GiwKnob *knob);
void           giw_knob_set_legends_digits(GiwKnob *knob, guint digits_number);
void           giw_knob_set_ticks_number(GiwKnob *knob, guint major, guint minor);
void           giw_knob_set_mouse_policy(GiwKnob *knob, GiwKnobMousePolicy policy);
void           giw_knob_set_title(GiwKnob *knob, gchar *str);


G_END_DECLS

#endif /* __GIW_KNOB_H__ */
