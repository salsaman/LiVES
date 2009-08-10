// LiVES
// framedraw.h
// (c) G. Finch (salsaman@xs4all.nl)
// see file COPYING for licensing details : released under the GNU GPL 3 or later

#ifndef _HAS_FRAMEDRAW_H
#define _HAS_FRAMEDRAW_H

// min and max frame sizes for framedraw preview
#define MIN_PRE_X 58
#define MIN_PRE_Y 32

#define MAX_PRE_X 320
#define MAX_PRE_Y 240


// call this to add framedraw widget to an hbox
void
widget_add_framedraw (GtkVBox *box, gint start, gint end, gboolean add_preview_button, gint width, gint height);

// redraw when exposed/frame number changes
void framedraw_redraw (lives_special_framedraw_rect_t *, gboolean reload_image, GdkPixbuf *);

// callback for widgets
void after_framedraw_widget_changed (GtkWidget *, lives_special_framedraw_rect_t *);


// activate the image for clicks and draws
void framedraw_connect(lives_special_framedraw_rect_t *, gint width, gint height, lives_rfx_t *);
// connect spinbutton to preview
void framedraw_connect_spinbutton(lives_special_framedraw_rect_t *, lives_rfx_t *);

// add "reset values" button
void framedraw_add_reset(GtkVBox *, lives_special_framedraw_rect_t *);

// add explanatory label
void framedraw_add_label(GtkVBox *box);


// reload and redraw the frame
void load_framedraw_image(GdkPixbuf *);
void load_rfx_preview(lives_rfx_t *rfx); // rfx preview

// just redraw the frame
void redraw_framedraw_image(void);

// change the frame number
void after_framedraw_frame_spinbutton_changed (GtkSpinButton *, lives_special_framedraw_rect_t *);

// reset button
void on_framedraw_reset_clicked (GtkButton *, lives_special_framedraw_rect_t *);


gboolean on_framedraw_mouse_start (GtkWidget *, GdkEventButton *, lives_special_framedraw_rect_t *);
gboolean on_framedraw_mouse_update (GtkWidget *, GdkEventButton *, lives_special_framedraw_rect_t *);
gboolean on_framedraw_mouse_reset (GtkWidget *, GdkEventButton *, lives_special_framedraw_rect_t *);


gboolean on_framedraw_leave (GtkWidget *, GdkEventCrossing *, lives_special_framedraw_rect_t *);
gboolean on_framedraw_enter (GtkWidget *, GdkEventCrossing *, lives_special_framedraw_rect_t *);

// graphics routines

void draw_rect_demask (GdkColor *col, gint x1, gint y1, gint x2, gint y2, gboolean filled);


#endif
