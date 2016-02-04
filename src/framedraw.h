// LiVES
// framedraw.h
// (c) G. Finch (salsaman@gmail.com) 2002 - 2012
// see file COPYING for licensing details : released under the GNU GPL 3 or later

#ifndef HAS_LIVES_FRAMEDRAW_H
#define HAS_LIVES_FRAMEDRAW_H

// min and max frame sizes for framedraw preview
#define MIN_PRE_X 58
#define MIN_PRE_Y 32

#define MAX_PRE_X 320
#define MAX_PRE_Y 240

// internal padding in pixels for LiVESFrame
#define FD_HT_ADJ 12


/// call this to add framedraw widget to an hbox
void widget_add_framedraw(LiVESVBox *, int start, int end, boolean add_preview_button, int width, int height);

/// redraw when exposed/frame number changes
void framedraw_redraw(lives_special_framedraw_rect_t *, boolean reload_image, LiVESPixbuf *);

/// callback for widgets
void after_framedraw_widget_changed(LiVESWidget *, lives_special_framedraw_rect_t *);


/// activate the image for clicks and draws
void framedraw_connect(lives_special_framedraw_rect_t *, int width, int height, lives_rfx_t *);

/// connect spinbutton to preview
void framedraw_connect_spinbutton(lives_special_framedraw_rect_t *, lives_rfx_t *);

/// add "reset values" button
void framedraw_add_reset(LiVESVBox *, lives_special_framedraw_rect_t *);

/// add explanatory label
void framedraw_add_label(LiVESVBox *box);


/// reload and redraw the frame
void load_framedraw_image(LiVESPixbuf *);
void load_rfx_preview(lives_rfx_t *rfx); ///< rfx preview

/// just redraw the frame
void redraw_framedraw_image(void);

/// change the frame number
void after_framedraw_frame_spinbutton_changed(LiVESSpinButton *, lives_special_framedraw_rect_t *);

/// reset button
void on_framedraw_reset_clicked(LiVESButton *, lives_special_framedraw_rect_t *);


boolean on_framedraw_mouse_start(LiVESWidget *, LiVESXEventButton *, lives_special_framedraw_rect_t *);
boolean on_framedraw_mouse_update(LiVESWidget *, LiVESXEventMotion *, lives_special_framedraw_rect_t *);
boolean on_framedraw_mouse_reset(LiVESWidget *, LiVESXEventButton *, lives_special_framedraw_rect_t *);


boolean on_framedraw_leave(LiVESWidget *, LiVESXEventCrossing *, lives_special_framedraw_rect_t *);
boolean on_framedraw_enter(LiVESWidget *, LiVESXEventCrossing *, lives_special_framedraw_rect_t *);

// graphics routines

void draw_rect_demask(lives_colRGBA64_t *col, int x1, int y1, int x2, int y2, boolean filled);


#endif
