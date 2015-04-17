/* giwled.h  -  GiwLed widget's header
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



#ifndef __GIW_LED_H__
#define __GIW_LED_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIW_TYPE_LED                 (giw_led_get_type ())
#define GIW_LED(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIW_TYPE_LED, GiwLed))
#define GIW_LED_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GIW_TYPE_LED, GiwLedClass))
#define GIW_IS_LED(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIW_TYPE_LED))
#define GIW_IS_LED_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GIW_TYPE_LED))
#define GIW_LED_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GIW_TYPE_LED, GiwLedClass))

typedef struct _GiwLed        GiwLed;
typedef struct _GiwLedClass   GiwLedClass;

struct _GiwLed {
#if GTK_CHECK_VERSION(3,0,0)
  GtkWidget parent_instance;
#endif
  GtkWidget widget;

  gboolean on; //0 for false
#if GTK_CHECK_VERSION(3,0,0)
  GdkRGBA color_on, color_off;
#else
  GdkColor color_on, color_off;
#endif

  guint size; // Size of the led
  guint x, y; // Position inside the widget's window

  guint radius; // the led radius

  guint8 enable_mouse; // 0 for disable mouse using, other value to enable it
};

struct _GiwLedClass {
  GtkWidgetClass parent_class;

  void (* mode_changed)(GiwLed *led);  //Signal emited when the mode is chaged (on to off, or off to on)
};


GtkWidget     *giw_led_new(void);
GType          giw_led_get_type(void) G_GNUC_CONST;
void           giw_led_set_mode(GiwLed *led, guint8 mode);
guint8         giw_led_get_mode(GiwLed *led);
#if GTK_CHECK_VERSION(3,0,0)
void           giw_led_set_rgba(GiwLed *led, GdkRGBA on_color, GdkRGBA off_color);
#endif
void           giw_led_set_colors(GiwLed *led, GdkColor on_color, GdkColor off_color);
void           giw_led_enable_mouse(GiwLed *led, gboolean option);

G_END_DECLS

#endif /* __GIW_LED_H__ */
