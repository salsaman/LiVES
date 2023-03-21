// user-interface.h.
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _USER_INTERFACE_H
#define _USER_INTERFACE_H

// functions which deal with the overal look and feel of the interface, rather than individual widgets

void sensitize(void);
void sensitize_rfx(void);
void desensitize(void);
void procw_desensitize(void);

void set_drawing_area_from_pixbuf(LiVESWidget *darea, LiVESPixbuf *, lives_painter_surface_t *);

void lives_layer_draw(LiVESDrawingArea *widget, weed_layer_t *layer);

boolean set_palette_colours(boolean force_reload);

void set_main_title(const char *filename, int or_untitled_number);
void disp_main_title(void);

void get_gui_framesize(int *hsize, int *vsize);

#define MIN_MSGAREA_SCRNHEIGHT (GUI_SCREEN_HEIGHT - ((CE_TIMELINE_VSPACE * 1.01 + widget_opts.border_width * 2) \
						     / sqrt(widget_opts.scaleH) + vspace + by) \
				+ CE_TIMELINE_VSPACE + MIN_MSGBAR_HEIGHT)

boolean check_can_show_msg_area(void);

void reset_mainwin_size(void);

void resize(double scale);

boolean resize_message_area(livespointer data);

#endif
