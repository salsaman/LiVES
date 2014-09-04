// widget-helper.c
// LiVES
// (c) G. Finch 2012 - 2014 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details



// The idea here is to replace toolkit specific functions with generic ones

// TODO - replace as much code in the other files with these functions as possible

// TODO - add for other toolkits, e.g. qt


#include "main.h"

// basic functions



////////////////////////////////////////////////////
//lives_painter functions

LIVES_INLINE lives_painter_t *lives_painter_create(lives_painter_surface_t *target) {
  lives_painter_t *cr=NULL;
#ifdef PAINTER_CAIRO
  cr=cairo_create(target);
#endif
  return cr;

}

LIVES_INLINE lives_painter_t *lives_painter_create_from_widget(LiVESWidget *widget) {
  lives_painter_t *cr=NULL;
#ifdef PAINTER_CAIRO
#ifdef GUI_GTK
  LiVESXWindow *window=lives_widget_get_xwindow(widget);
  if (window!=NULL) {
    cr=gdk_cairo_create(window);
  }
#endif
#endif
  return cr;
}


LIVES_INLINE void lives_painter_set_source_pixbuf (lives_painter_t *cr, const LiVESPixbuf *pixbuf, double pixbuf_x, double pixbuf_y) {
#ifdef PAINTER_CAIRO
  gdk_cairo_set_source_pixbuf(cr,pixbuf,pixbuf_x,pixbuf_y);
#endif

}


LIVES_INLINE void lives_painter_set_source_surface (lives_painter_t *cr, lives_painter_surface_t *surface, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_set_source_surface(cr,surface,x,y);
#endif

}

LIVES_INLINE void lives_painter_paint(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_paint(cr);
#endif
}

LIVES_INLINE void lives_painter_fill(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_fill(cr);
#endif
}

LIVES_INLINE void lives_painter_stroke(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_stroke(cr);
#endif
}

LIVES_INLINE void lives_painter_clip(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_clip(cr);
#endif
}

LIVES_INLINE void lives_painter_destroy(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_destroy(cr);
#endif
}

LIVES_INLINE void lives_painter_surface_destroy(lives_painter_surface_t *surf) {
#ifdef PAINTER_CAIRO
  cairo_surface_destroy(surf);
#endif
}

LIVES_INLINE void lives_painter_new_path(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_new_path(cr);
#endif
}


LIVES_INLINE void lives_painter_translate(lives_painter_t *cr, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_translate(cr,x,y);
#endif

}


LIVES_INLINE void lives_painter_set_line_width(lives_painter_t *cr, double width) {
#ifdef PAINTER_CAIRO
  cairo_set_line_width(cr,width);
#endif

}


LIVES_INLINE void lives_painter_move_to(lives_painter_t *cr, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_move_to(cr,x,y);
#endif

}

LIVES_INLINE void lives_painter_line_to(lives_painter_t *cr, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_line_to(cr,x,y);
#endif

}

LIVES_INLINE void lives_painter_rectangle(lives_painter_t *cr, double x, double y, double width, double height) {
#ifdef PAINTER_CAIRO
  cairo_rectangle(cr,x,y,width,height);
#endif

}

LIVES_INLINE void lives_painter_arc(lives_painter_t *cr, double xc, double yc, double radius, double angle1, double angle2) {
#ifdef PAINTER_CAIRO
  cairo_arc(cr,xc,yc,radius,angle1,angle2);
#endif

}
 
LIVES_INLINE boolean lives_painter_set_operator(lives_painter_t *cr, lives_painter_operator_t op) {
  // if op was not LIVES_PAINTER_OPERATOR_DEFAULT, and FALSE is returned, then the operation failed,
  // and op was set to the default
#ifdef PAINTER_CAIRO
  cairo_set_operator(cr,op);
  if (op==LIVES_PAINTER_OPERATOR_UNKNOWN) return FALSE;
  return TRUE;
#endif
  return FALSE;
}

LIVES_INLINE void lives_painter_set_source_rgb(lives_painter_t *cr, double red, double green, double blue) {
#ifdef PAINTER_CAIRO
  cairo_set_source_rgb(cr,red,green,blue);
#endif
}

LIVES_INLINE void lives_painter_set_source_rgba(lives_painter_t *cr, double red, double green, double blue, double alpha) {
#ifdef PAINTER_CAIRO
   cairo_set_source_rgba(cr,red,green,blue,alpha);
#endif
}

LIVES_INLINE void lives_painter_set_fill_rule(lives_painter_t *cr, lives_painter_fill_rule_t fill_rule) {
#ifdef PAINTER_CAIRO
  cairo_set_fill_rule(cr,fill_rule);
#endif
}


LIVES_INLINE void lives_painter_surface_flush(lives_painter_surface_t *surf) {
#ifdef PAINTER_CAIRO
  cairo_surface_flush(surf);
#endif
}



LIVES_INLINE lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data, lives_painter_format_t format, 
										  int width, int height, int stride) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  surf=cairo_image_surface_create_for_data(data,format,width,height,stride);
#endif
  return surf;
}


LIVES_INLINE lives_painter_surface_t *lives_painter_surface_create_from_widget(LiVESWidget *widget, lives_painter_content_t format, 
									       int width, int height) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  LiVESXWindow *window=lives_widget_get_xwindow(widget);
  if (window!=NULL) {
#if G_ENCODE_VERSION(GDK_MAJOR_VERSION,GDK_MINOR_VERSION) >= G_ENCODE_VERSION(2,22)
    surf=gdk_window_create_similar_surface(window,format,width,height);
#else 
    surf=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
#endif
}
#endif
  return surf;
}


LIVES_INLINE lives_painter_surface_t *lives_painter_image_surface_create(lives_painter_format_t format, int width, int height) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  surf=cairo_image_surface_create(format,width,height);
#endif
  return surf;
}



////////////////////////// painter info funcs

lives_painter_surface_t *lives_painter_get_target(lives_painter_t *cr) {

 lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  surf=cairo_get_target(cr);
#endif  
  return surf;

}


int lives_painter_format_stride_for_width(lives_painter_format_t form, int width) {
  int stride=-1;
#ifdef PAINTER_CAIRO
  stride=cairo_format_stride_for_width(form,width);
#endif
  return stride;
}


uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *surf) {
  uint8_t *data=NULL;
#ifdef PAINTER_CAIRO
  data=cairo_image_surface_get_data(surf);
#endif
  return data;
}


int lives_painter_image_surface_get_width(lives_painter_surface_t *surf) {
  int width=0;
#ifdef PAINTER_CAIRO
  width=cairo_image_surface_get_width(surf);
#endif
  return width;
}


int lives_painter_image_surface_get_height(lives_painter_surface_t *surf) {
  int height=0;
#ifdef PAINTER_CAIRO
  height=cairo_image_surface_get_height(surf);
#endif
  return height;
}


int lives_painter_image_surface_get_stride(lives_painter_surface_t *surf) {
  int stride=0;
#ifdef PAINTER_CAIRO
  stride=cairo_image_surface_get_stride(surf);
#endif
  return stride;
}


lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *surf) {
  lives_painter_format_t format=(lives_painter_format_t)0;
#ifdef PAINTER_CAIRO
  format=cairo_image_surface_get_format(surf);
#endif
  return format;
}


////////////////////////////////////////////////////////

static void set_label_state(LiVESWidget *widget, LiVESWidgetState state, livespointer labelp) {
  LiVESWidget *label=(LiVESWidget *)labelp;
  if (lives_widget_get_sensitive(widget)&&!lives_widget_get_sensitive(label)) {
    lives_widget_set_sensitive(label,TRUE);
  }
  if (!lives_widget_get_sensitive(widget)&&lives_widget_get_sensitive(label)) {
    lives_widget_set_sensitive(label,FALSE);
  }
}

LIVES_INLINE void lives_object_unref(livespointer object) {
#ifdef GUI_GTK
  g_object_unref(object);
#endif
}


LIVES_INLINE void lives_widget_set_sensitive(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
  gtk_widget_set_sensitive(widget,state);
#endif
}


LIVES_INLINE boolean lives_widget_get_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_sensitive(widget);
#endif
}


LIVES_INLINE void lives_widget_show(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show(widget);
#endif
}


LIVES_INLINE void lives_widget_hide(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_hide(widget);
#endif
}


LIVES_INLINE void lives_widget_show_all(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show_all(widget);
#endif
}


LIVES_INLINE void lives_widget_destroy(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_destroy(widget);
#endif
}


LIVES_INLINE void lives_widget_queue_draw(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_queue_draw(widget);
#endif
}


LIVES_INLINE void lives_widget_queue_draw_area(LiVESWidget *widget, int x, int y, int width, int height) {
#ifdef GUI_GTK
  gtk_widget_queue_draw_area(widget,x,y,width,height);
#endif
}


LIVES_INLINE void lives_widget_queue_resize(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_queue_resize(widget);
#endif
}



LIVES_INLINE void lives_widget_set_size_request(LiVESWidget *widget, int width, int height) {
#ifdef GUI_GTK
  gtk_widget_set_size_request(widget,width,height);
#endif
}


LIVES_INLINE void lives_widget_reparent(LiVESWidget *widget, LiVESWidget *new_parent) {
#ifdef GUI_GTK
  gtk_widget_reparent(widget,new_parent);
#endif
}


LIVES_INLINE void lives_widget_set_app_paintable(LiVESWidget *widget, boolean paintable) {
#ifdef GUI_GTK
  gtk_widget_set_app_paintable(widget,paintable);
#endif
}



LIVES_INLINE int lives_dialog_run(LiVESDialog *dialog) {
#ifdef GUI_GTK
  return gtk_dialog_run(dialog);
#endif
}


LIVES_INLINE void lives_widget_set_bg_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_override_background_color(widget,state,color);
#else
  gtk_widget_modify_bg(widget,state,color);
#endif
#endif
}


LIVES_INLINE void lives_widget_set_fg_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_override_color(widget,state,color);
#else
  gtk_widget_modify_fg(widget,state,color);
#endif
#endif
}


LIVES_INLINE void lives_widget_set_text_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_override_color(widget,state,color);
#else
  gtk_widget_modify_text(widget,state,color);
#endif
#endif
}


LIVES_INLINE void lives_widget_set_base_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_override_background_color(widget,state,color);
#else
  gtk_widget_modify_base(widget,state,color);
#endif
#endif
}


LIVES_INLINE void lives_widget_get_bg_state_color(LiVESWidget *widget, LiVESWidgetState state, LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_style_context_get_background_color (gtk_widget_get_style_context (widget), state, color);
#else
  lives_widget_color_copy(color,&gtk_widget_get_style(widget)->bg[state]);
#endif
#endif
}


LIVES_INLINE void lives_widget_get_fg_state_color(LiVESWidget *widget, LiVESWidgetState state, LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_style_context_get_color (gtk_widget_get_style_context (widget), LIVES_WIDGET_STATE_NORMAL, color);
#else
  lives_widget_color_copy(color,&gtk_widget_get_style(widget)->fg[LIVES_WIDGET_STATE_NORMAL]);
#endif
#endif
}


LIVES_INLINE LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1, const LiVESWidgetColor *c2) {
  LiVESWidgetColor *c0=NULL;
#ifdef GUI_GTK

  if (c1!=NULL) {
  c1->red=c2->red;
  c1->green=c2->green;
  c1->blue=c2->blue;
#if GTK_CHECK_VERSION(3,0,0)
  c1->alpha=c2->alpha;
#else
  c1->pixel=c2->pixel;
#endif
}
  else {
#if GTK_CHECK_VERSION(3,0,0)
  c0=gdk_rgba_copy(c2);
#else
  c0=gdk_color_copy(c2);
#endif
}

#endif
return c0;
}

LIVES_INLINE LiVESWidget *lives_event_box_new(void) {
  LiVESWidget *eventbox=NULL;
#ifdef GUI_GTK
  eventbox=gtk_event_box_new();
#endif
  return eventbox;
}


LIVES_INLINE LiVESWidget *lives_image_new(void) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
  image=gtk_image_new();
#endif
  return image;
}



LIVES_INLINE LiVESWidget *lives_image_new_from_stock(const char *stock_id, LiVESIconSize size) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
    if (lives_has_icon(stock_id,size)) {
#if GTK_CHECK_VERSION(3,10,0)
      image=gtk_image_new_from_icon_name(stock_id, size);
#else
      image=gtk_image_new_from_stock(stock_id,size);
#endif
  }
  else {
#if GTK_CHECK_VERSION(3,10,0)
    image=gtk_image_new_from_icon_name(LIVES_STOCK_MISSING_IMAGE, size);
    if (image==NULL) image=gtk_image_new_from_icon_name(LIVES_STOCK_CLOSE,size);
#else
    image=gtk_image_new_from_stock(LIVES_STOCK_MISSING_IMAGE,size);
    if (image==NULL) image=gtk_image_new_from_stock(GTK_STOCK_NO,size);
    if (image==NULL) image=gtk_image_new_from_stock(LIVES_STOCK_CLOSE,size);
#endif
  }
#endif
  return image;
}


LIVES_INLINE LiVESWidget *lives_image_new_from_file(const char *filename) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
  image=gtk_image_new_from_file(filename);
#endif
  return image;
}


LIVES_INLINE LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf *pixbuf) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
  image=gtk_image_new_from_pixbuf(pixbuf);
#endif
  return image;
}



LIVES_INLINE void lives_image_set_from_pixbuf(LiVESImage *image, LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  gtk_image_set_from_pixbuf(image,pixbuf);
#endif
}


LIVES_INLINE LiVESPixbuf *lives_image_get_pixbuf(LiVESImage *image) {
  LiVESPixbuf *pixbuf=NULL;
#ifdef GUI_GTK
  pixbuf=gtk_image_get_pixbuf(image);
#endif
  return pixbuf;
}



LIVES_INLINE boolean lives_color_parse(const char *spec, LiVESWidgetColor *color) {
  boolean retval=FALSE;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  retval=gdk_rgba_parse(color,spec);
#else
  retval=gdk_color_parse(spec,color);
#endif
#endif
  return retval;
}



LIVES_INLINE LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_content_area(LIVES_DIALOG(dialog));
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif

#endif
}



LIVES_INLINE LiVESWidget *lives_dialog_get_action_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_action_area(LIVES_DIALOG(dialog));
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif

#endif
}



LIVES_INLINE void lives_dialog_add_action_widget(LiVESDialog *dialog, LiVESWidget *widget, int response) {
#ifdef GUI_GTK
  gtk_dialog_add_action_widget(dialog,widget,response);
#endif
}


LIVES_INLINE LiVESWidget *lives_window_new(LiVESWindowType wintype) {
  LiVESWidget *window=NULL;
#ifdef GUI_GTK
  window=gtk_window_new(wintype);
#endif
  return window;
}


LIVES_INLINE boolean lives_window_set_title(LiVESWindow *window, const char *title) {
#ifdef GUI_GTK
  gtk_window_set_title(window,title);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_transient_for(LiVESWindow *window, LiVESWindow *parent) {
#ifdef GUI_GTK
  gtk_window_set_transient_for(window,parent);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_modal(LiVESWindow *window, boolean modal) {
#ifdef GUI_GTK
  gtk_window_set_modal(window,modal);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_deletable(LiVESWindow *window, boolean deletable) {
#ifdef GUI_GTK
  gtk_window_set_deletable(window,deletable);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_resizable(LiVESWindow *window, boolean resizable) {
#ifdef GUI_GTK
  gtk_window_set_resizable(window,resizable);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_keep_below(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_keep_below(window,set);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_decorated(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_decorated(window,set);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_auto_startup_notification(boolean set) {
#ifdef GUI_GTK
  gtk_window_set_auto_startup_notification(set);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_window_set_screen(LiVESWindow *window, LiVESXScreen *screen) {
#ifdef GUI_GTK
  gtk_window_set_screen(window,screen);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_default_size(LiVESWindow *window, int width, int height) {
#ifdef GUI_GTK
  gtk_window_set_default_size(window,width,height);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE const char *lives_window_get_title(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_get_title(window);
#endif
  return NULL;
}


LIVES_INLINE boolean lives_window_move(LiVESWindow *window, int x, int y) {
#ifdef GUI_GTK
  gtk_window_move(window,x,y);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_get_position(LiVESWindow *window, int *x, int *y) {
#ifdef GUI_GTK
  gtk_window_get_position(window,x,y);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_position(LiVESWindow *window, LiVESWindowPosition pos) {
#ifdef GUI_GTK
  gtk_window_set_position(window,pos);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_hide_titlebar_when_maximized(LiVESWindow *window, boolean setting) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,4,0)
  gtk_window_set_hide_titlebar_when_maximized(window,setting);
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_resize(LiVESWindow *window, int width, int height) {
#ifdef GUI_GTK
  gtk_window_resize(window,width,height);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_present(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_present(window);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_fullscreen(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_fullscreen(window);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_unfullscreen(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_unfullscreen(window);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_maximize(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_maximize(window);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_unmaximize(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_unmaximize(window);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_add_accel_group(LiVESWindow *window, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_window_add_accel_group(window,group);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_remove_accel_group(LiVESWindow *window, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_window_remove_accel_group(window,group);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_menu_set_accel_group(LiVESMenu *menu, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_menu_set_accel_group(menu,group);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_window_has_toplevel_focus(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_has_toplevel_focus(window);
#endif
  return FALSE;
}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height) {

#ifdef GUI_GTK
  // alpha fmt is RGBA post mult
  return gdk_pixbuf_new(GDK_COLORSPACE_RGB,has_alpha,8,width,height);
#endif
			
#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  enum fmt;
  if (!has_alpha) fmt=QImage::Format_RGB888;
  else {
    fmt=QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  return new QImage(width, height, fmt);
#endif
}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_data (const unsigned char *buf, boolean has_alpha, int width, int height, 
						      int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn, 
						      livespointer destroy_fn_data) {

#ifdef GUI_GTK
  return gdk_pixbuf_new_from_data ((const guchar *)buf, GDK_COLORSPACE_RGB, has_alpha, 8, width, height, rowstride, 
				   lives_free_buffer_fn, 
				   destroy_fn_data);
#endif


#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  enum fmt;
  if (!has_alpha) fmt=QImage::Format_RGB888;
  else {
    fmt=QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  // on destruct, we need to call lives_free_buffer_fn(uchar *pixels, gpointer destroy_fn_data)
  LIVES_ERROR("Need to set destructor fn for QImage");
  return new QImage((uchar *)buf, width, height, rowstride, fmt);
#endif

}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file(filename, error);
#endif

#ifdef GUI_QT
  QImage image = new QImage();
  if (!image.load(filename)) {
    // do something with error
    LIVES_WARN("QImage not loaded");
    ~image();
    return NULL;
  }
  return image;
}

#endif
}




LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, 
							      boolean preserve_aspect_ratio,
							      LiVESError **error) {

#ifdef GUI_GTK
  return lives_pixbuf_new_from_file_at_scale(filename, width, height, preserve_aspect_ratio, error);
#endif

#ifdef GUI_QT
  QImage image = QImage();
  QImage image2;
  if (!image.load(filename)) {
    // do something with error
    LIVES_WARN("QImage not loaded");
    return NULL;
  }
  if (preserve_aspect_ratio) asp=Qt::KeepAspectRatio;
  else asp=Qt::IgnoreAspectRatio;
  image2 = new image.scaled(width, height, asp,  Qt::SmoothTransformation);
  if (!image2) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return image2;
}

#endif
}



LIVES_INLINE int lives_pixbuf_get_rowstride(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_rowstride(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.bytesPerLine();
#endif
}


LIVES_INLINE int lives_pixbuf_get_width(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_width(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.width();
#endif
}


LIVES_INLINE int lives_pixbuf_get_height(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_height(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.height();
#endif
}


LIVES_INLINE int lives_pixbuf_get_n_channels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_n_channels(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.depth()>>3;
#endif
}



LIVES_INLINE unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.bits();
#endif
}


LIVES_INLINE const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *pixbuf) {

#ifdef GUI_GTK
  return (const guchar *)gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return (const uchar *)pixbuf.bits();
#endif
}



LIVES_INLINE boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_has_alpha(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.hasAlphaChannel();
#endif
}


LIVES_INLINE LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height, 
						    LiVESInterpType interp_type) {
  
#ifdef GUI_GTK
  return gdk_pixbuf_scale_simple(src, dest_width, dest_height, interp_type);
#endif


#ifdef GUI_QT
  QImage *image = new src.scaled(dest_width, dest_height, Qt::IgnoreAspectRatio,  interp_type);
  if (!image) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return image;

#endif

}

LIVES_INLINE boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf *src, LiVESPixbuf *dest, float saturation, boolean pixilate) {

#ifdef GUI_GTK
  gdk_pixbuf_saturate_and_pixelate(src, dest, saturation, pixilate);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE LiVESAdjustment *lives_adjustment_new(double value, double lower, double upper, 
						   double step_increment, double page_increment, double page_size) {
  LiVESAdjustment *adj=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  adj=gtk_adjustment_new(value,lower,upper,step_increment,page_increment,page_size);
#else
  adj=GTK_ADJUSTMENT(gtk_adjustment_new(value,lower,upper,step_increment,page_increment,page_size));
#endif
#endif
  return adj;
}


LIVES_INLINE void lives_box_set_homogeneous(LiVESBox *box, boolean homogenous) {
#ifdef GUI_GTK
  gtk_box_set_homogeneous(box,homogenous);
#endif
}


LIVES_INLINE void lives_box_reorder_child(LiVESBox *box, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_box_reorder_child(box,child,pos);
#endif
}


LIVES_INLINE void lives_box_set_spacing(LiVESBox *box, int spacing) {
#ifdef GUI_GTK
  gtk_box_set_spacing(box,spacing);
#endif
}


LIVES_INLINE LiVESWidget *lives_hbox_new(boolean homogeneous, int spacing) {
  LiVESWidget *hbox=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hbox=gtk_box_new(LIVES_ORIENTATION_HORIZONTAL,spacing);
  lives_box_set_homogeneous(LIVES_BOX(hbox),homogeneous);
#else
  hbox=gtk_hbox_new(homogeneous,spacing);
#endif
#endif
  return hbox;
}


LIVES_INLINE LiVESWidget *lives_vbox_new(boolean homogeneous, int spacing) {
  LiVESWidget *vbox=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  vbox=gtk_box_new(LIVES_ORIENTATION_VERTICAL,spacing);
  lives_box_set_homogeneous(LIVES_BOX(vbox),homogeneous);
#else
  vbox=gtk_vbox_new(homogeneous,spacing);
#endif
#endif
  return vbox;
}



LIVES_INLINE void lives_box_pack_start(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_start(box,child,expand,fill,padding);
#endif
}



LIVES_INLINE void lives_box_pack_end(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_end(box,child,expand,fill,padding);
#endif
}




LIVES_INLINE LiVESWidget *lives_hseparator_new(void) {
  LiVESWidget *hsep=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hsep=gtk_separator_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  hsep=gtk_hseparator_new();
#endif
#endif
  return hsep;
}


LIVES_INLINE LiVESWidget *lives_vseparator_new(void) {
  LiVESWidget *vsep=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  vsep=gtk_separator_new(LIVES_ORIENTATION_VERTICAL);
#else
  vsep=gtk_vseparator_new();
#endif
#endif
  return vsep;
}


LIVES_INLINE LiVESWidget *lives_hbutton_box_new(void) {
  LiVESWidget *bbox=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  bbox=gtk_button_box_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  bbox=gtk_hbutton_box_new();
#endif
#endif
  return bbox;
}


LIVES_INLINE LiVESWidget *lives_vbutton_box_new(void) {
  LiVESWidget *bbox=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  bbox=gtk_button_box_new(LIVES_ORIENTATION_VERTICAL);
#else
  bbox=gtk_vbutton_box_new();
#endif
#endif
  return bbox;
}


LIVES_INLINE LiVESWidget *lives_hscale_new(LiVESAdjustment *adj) {
  LiVESWidget *hscale=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hscale=gtk_scale_new(LIVES_ORIENTATION_HORIZONTAL,adj);
#else
  hscale=gtk_hscale_new(adj);
#endif
#endif
  return hscale;
}


LIVES_INLINE LiVESWidget *lives_vscale_new(LiVESAdjustment *adj) {
  LiVESWidget *vscale=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  vscale=gtk_scale_new(LIVES_ORIENTATION_VERTICAL,adj);
#else
  vscale=gtk_vscale_new(adj);
#endif
#endif
  return vscale;
}


LIVES_INLINE LiVESWidget *lives_hpaned_new(void) {
  LiVESWidget *hpaned=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hpaned=gtk_paned_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  hpaned=gtk_hpaned_new();
#endif
#endif
  return hpaned;
}


LIVES_INLINE LiVESWidget *lives_vpaned_new(void) {
  LiVESWidget *vpaned=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  vpaned=gtk_paned_new(LIVES_ORIENTATION_VERTICAL);
#else
  vpaned=gtk_vpaned_new();
#endif
#endif
  return vpaned;
}


LIVES_INLINE LiVESWidget *lives_hscrollbar_new(LiVESAdjustment *adj) {
  LiVESWidget *hscrollbar=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hscrollbar=gtk_scrollbar_new(LIVES_ORIENTATION_HORIZONTAL,adj);
#else
  hscrollbar=gtk_hscrollbar_new(adj);
#endif
#endif
  return hscrollbar;
}


LIVES_INLINE LiVESWidget *lives_vscrollbar_new(LiVESAdjustment *adj) {
  LiVESWidget *vscrollbar=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  vscrollbar=gtk_scrollbar_new(LIVES_ORIENTATION_VERTICAL,adj);
#else
  vscrollbar=gtk_vscrollbar_new(adj);
#endif
#endif
  return vscrollbar;
}


LIVES_INLINE LiVESWidget *lives_label_new(const char *text) {
  LiVESWidget *label=NULL;
#ifdef GUI_GTK

  label=gtk_label_new(text);

  gtk_label_set_justify (LIVES_LABEL (label), widget_opts.justify);

  gtk_label_set_line_wrap (LIVES_LABEL (label), widget_opts.line_wrap);
#endif

  return label;
}


LIVES_INLINE LiVESWidget *lives_arrow_new(LiVESArrowType arrow_type, LiVESShadowType shadow_type) {
  LiVESWidget *arrow=NULL;
#ifdef GUI_GTK
  arrow=gtk_arrow_new(arrow_type,shadow_type);
#endif
  return arrow;
}


LIVES_INLINE LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale) {
  LiVESWidget *alignment=NULL;
#ifdef GUI_GTK
  alignment=gtk_alignment_new(xalign,yalign,xscale,yscale);
#endif
  return alignment;
}


LIVES_INLINE void lives_alignment_set(LiVESAlignment *alignment, float xalign, float yalign, float xscale, float yscale) {
#ifdef GUI_GTK
  gtk_alignment_set(alignment,xalign,yalign,xscale,yscale);
#endif
}


LIVES_INLINE LiVESWidget *lives_combo_new(void) {
  LiVESWidget *combo=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  combo = gtk_combo_box_text_new_with_entry ();
#else
  combo = gtk_combo_box_entry_new_text ();
#endif
#endif
  return combo;
}


LIVES_INLINE LiVESWidget *lives_combo_new_with_model (LiVESTreeModel *model) {
  LiVESWidget *combo=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  combo = gtk_combo_box_new_with_model_and_entry (model);
#else
  combo = gtk_combo_box_entry_new ();
  gtk_combo_box_set_model(GTK_COMBO_BOX(combo),model);
#endif
#endif
  return combo;

}


LIVES_INLINE LiVESTreeModel *lives_combo_get_model(LiVESCombo *combo) {
  LiVESTreeModel *model=NULL;
#ifdef GUI_GTK
  model=gtk_combo_box_get_model(combo);
#endif
  return model;
}



LIVES_INLINE void lives_combo_append_text(LiVESCombo *combo, const char *text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),text);
#else
  gtk_combo_box_append_text(GTK_COMBO(combo),text);
#endif
#endif
}


#ifdef GUI_GTK
static void lives_combo_remove_all_text(LiVESCombo *combo) {
#if GTK_CHECK_VERSION(3,0,0)
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
#else
  register int count = lives_tree_model_iter_n_children(lives_combo_get_model(combo),NULL);
  while (count-- > 0) gtk_combo_box_remove_text(combo,0);
#endif
}
#endif



LIVES_INLINE void lives_combo_set_entry_text_column(LiVESCombo *combo, int column) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo),column);
#else
  gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(combo),column);
#endif
#endif
}


LIVES_INLINE char *lives_combo_get_active_text(LiVESCombo *combo) {
  // return value should be freed
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
#else
  return gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
#endif
#endif
}


LIVES_INLINE void lives_combo_set_active_index(LiVESCombo *combo, int index) {
#ifdef GUI_GTK
  gtk_combo_box_set_active(combo,index);
#endif

}


LIVES_INLINE void lives_combo_set_active_iter(LiVESCombo *combo, LiVESTreeIter *iter) {
#ifdef GUI_GTK
  gtk_combo_box_set_active_iter(combo,iter);
#endif

}


LIVES_INLINE boolean lives_combo_get_active_iter(LiVESCombo *combo, LiVESTreeIter *iter) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active_iter(combo,iter);
#endif
  return FALSE;
}


LIVES_INLINE int lives_combo_get_active(LiVESCombo *combo) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active(combo);
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_text_view_new(void) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_text_view_new();
#endif
  return tview;
}



LIVES_INLINE LiVESWidget *lives_text_view_new_with_buffer(LiVESTextBuffer *tbuff) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_text_view_new_with_buffer(tbuff);
#endif
  return tview;
}


LIVES_INLINE LiVESTextBuffer *lives_text_view_get_buffer(LiVESTextView *tview) {
  LiVESTextBuffer *tbuff=NULL;
#ifdef GUI_GTK
  tbuff=gtk_text_view_get_buffer(tview);
#endif
  return tbuff;
}


LIVES_INLINE boolean lives_text_view_set_editable(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_editable(tview, setting);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_view_set_accepts_tab(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_accepts_tab(tview, setting);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_view_set_cursor_visible(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_cursor_visible(tview, setting);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_view_set_wrap_mode(LiVESTextView *tview, LiVESWrapMode wrapmode) {
#ifdef GUI_GTK
  gtk_text_view_set_wrap_mode(tview, wrapmode);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_view_set_justification(LiVESTextView *tview, LiVESJustification justify) {
#ifdef GUI_GTK
  gtk_text_view_set_justification(tview, justify);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_view_scroll_mark_onscreen(LiVESTextView *tview, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_view_scroll_mark_onscreen(tview, mark);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESTextBuffer *lives_text_buffer_new(void) {
  LiVESTextBuffer *tbuff=NULL;
#ifdef GUI_GTK
  tbuff=gtk_text_buffer_new(NULL);
#endif
  return tbuff;
}



LIVES_INLINE boolean lives_text_buffer_insert(LiVESTextBuffer *tbuff, LiVESTextIter *iter, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert(tbuff, iter, text, len);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer *tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert_at_cursor(tbuff, text, len);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_set_text(LiVESTextBuffer *tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_set_text(tbuff, text, len);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE char *lives_text_buffer_get_text(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end, boolean inc_hidden_chars) {
#ifdef GUI_GTK
  return gtk_text_buffer_get_text(tbuff,start,end,inc_hidden_chars);
#endif
  return NULL;
}


LIVES_INLINE boolean lives_text_buffer_get_start_iter(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_start_iter(tbuff, iter);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_buffer_get_end_iter(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_end_iter(tbuff, iter);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_place_cursor(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_place_cursor(tbuff, iter);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE LiVESTextMark *lives_text_buffer_create_mark(LiVESTextBuffer *tbuff, const char *mark_name, 
							  const LiVESTextIter *where, boolean left_gravity) {
  LiVESTextMark *tmark;
#ifdef GUI_GTK
  tmark=gtk_text_buffer_create_mark(tbuff, mark_name, where, left_gravity);
#endif
  return tmark;
}



LIVES_INLINE boolean lives_text_buffer_delete_mark(LiVESTextBuffer *tbuff, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_buffer_delete_mark(tbuff, mark);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_delete(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end) {
#ifdef GUI_GTK
  gtk_text_buffer_delete(tbuff, start, end);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_get_iter_at_mark(LiVESTextBuffer *tbuff, LiVESTextIter *iter, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_buffer_get_iter_at_mark(tbuff, iter, mark);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_button_new(void) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_button_new();
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_button_new_with_mnemonic(const char *label) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_button_new_with_mnemonic(label);
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_button_new_with_label(const char *label) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_button_new_with_label(label);
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_button_new_from_stock(const char *stock_id) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,6,0)
#if GTK_CHECK_VERSION(3,10,0)
  char *xstock_id=g_strdup(stock_id); // need to back this up as we will use translation functions
  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_CANCEL)||!strcmp(xstock_id,LIVES_STOCK_LABEL_OK)) {
    button=gtk_button_new_with_mnemonic(xstock_id);
    g_free(xstock_id);
    return button;
  }
  {
#else
  if (!strcmp(stock_id,LIVES_STOCK_ADD) || 
      !strcmp(stock_id,LIVES_STOCK_REMOVE)
      ) {
#endif
    LiVESWidget *image=gtk_image_new_from_icon_name(stock_id,GTK_ICON_SIZE_BUTTON);
    button=gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(button),image);
#if GTK_CHECK_VERSION(3,6,0)
    gtk_button_set_always_show_image(GTK_BUTTON(button),TRUE);
#endif
  }
#if !GTK_CHECK_VERSION(3,10,0)
  else {
    button=gtk_button_new_from_stock(stock_id);
  }
#endif

#else
  button=gtk_button_new_from_stock(stock_id);
#endif

#endif
  return button;
}



LIVES_INLINE boolean lives_button_set_label(LiVESButton *button, const char *label) {
#ifdef GUI_GTK
  gtk_button_set_label(button,label);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE boolean lives_toggle_button_get_active(LiVESToggleButton *button) {
#ifdef GUI_GTK
  return gtk_toggle_button_get_active(button);
#endif
  return FALSE;
}


LIVES_INLINE void lives_toggle_button_set_active(LiVESToggleButton *button, boolean active) {
#ifdef GUI_GTK
  gtk_toggle_button_set_active(button,active);
#endif
}


LIVES_INLINE void lives_widget_set_tooltip_text(LiVESWidget *widget, const char *tip_text) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(widget,tip_text);
#else
  GtkTooltips *tips;
  tips = gtk_tooltips_new ();
  gtk_tooltips_set_tip(tips,widget,tip_text,NULL);
#endif
#endif
}


LIVES_INLINE boolean lives_widget_grab_focus(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_grab_focus(widget);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_grab_default(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_grab_default(widget);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESSList *lives_radio_button_get_group(LiVESRadioButton *rbutton) {
#ifdef GUI_GTK
  return gtk_radio_button_get_group(rbutton);
#endif
  return NULL;
}


LIVES_INLINE LiVESSList *lives_radio_menu_item_get_group(LiVESRadioMenuItem *rmenuitem) {
#ifdef GUI_GTK
  return gtk_radio_menu_item_get_group(rmenuitem);
#endif
  return NULL;
}


LIVES_INLINE LiVESWidget *lives_widget_get_parent(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_parent(widget);
#endif
  return NULL;
}



LIVES_INLINE LiVESWidget *lives_widget_get_toplevel(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_toplevel(widget);
#endif
  return NULL;
}


LIVES_INLINE LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  return gtk_widget_get_window(widget);
#else
  return GDK_WINDOW(widget->window);
#endif
#endif
  return NULL;
}

LIVES_INLINE boolean lives_widget_set_can_focus(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_focus(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_can_default(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_default(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_DEFAULT);
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_add_events(LiVESWidget *widget, int events) {
#ifdef GUI_GTK
  gtk_widget_add_events(widget,events);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_events(LiVESWidget *widget, int events) {
#ifdef GUI_GTK
  gtk_widget_set_events(widget,events);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_remove_accelerator(LiVESWidget *widget, LiVESAccelGroup *acgroup, 
						     uint32_t accel_key, LiVESModifierType accel_mods) {
#ifdef GUI_GTK
  return gtk_widget_remove_accelerator(widget,acgroup,accel_key,accel_mods);
#endif
  return FALSE;
}


boolean lives_widget_get_preferred_size(LiVESWidget *widget, LiVESRequisition *min_size, LiVESRequisition *nat_size) {
#ifdef GUI_GTK
  gtk_widget_get_preferred_size(widget,min_size,nat_size);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_is_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_is_sensitive(widget);
#else
  return GTK_WIDGET_IS_SENSITIVE (widget);
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_is_visible(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_visible(widget);
#else
  return GTK_WIDGET_VISIBLE (widget);
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_is_realized(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_realized(widget);
#else
  return GTK_WIDGET_REALIZED (widget);
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_container_add(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_add(container,widget);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_container_remove(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_remove(container,widget);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_container_set_border_width(LiVESContainer *container, uint32_t width) {
  // sets border OUTSIDE container
#ifdef GUI_GTK
  gtk_container_set_border_width(container,width);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE double lives_spin_button_get_value(LiVESSpinButton *button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value(button);
#endif
  return 0.;
}


LIVES_INLINE int lives_spin_button_get_value_as_int(LiVESSpinButton *button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value_as_int(button);
#endif
  return 0.;
}


LIVES_INLINE boolean lives_spin_button_set_value(LiVESSpinButton *button, double value) {
#ifdef GUI_GTK
  gtk_spin_button_set_value(button,value);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_set_range(LiVESSpinButton *button, double min, double max) {
#ifdef GUI_GTK
  gtk_spin_button_set_range(button,min,max);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_set_wrap(LiVESSpinButton *button, boolean wrap) {
#ifdef GUI_GTK
  gtk_spin_button_set_wrap(button,wrap);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_set_digits(LiVESSpinButton *button, uint32_t digits) {
#ifdef GUI_GTK
  gtk_spin_button_set_digits(button,digits);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_update(LiVESSpinButton *button) {
#ifdef GUI_GTK
  gtk_spin_button_update(button);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESToolItem *lives_tool_button_new(LiVESWidget *icon_widget, const char *label) {
  LiVESToolItem *button=NULL;
#ifdef GUI_GTK
  button=gtk_tool_button_new(icon_widget,label);
#endif
  return button;
}


LIVES_INLINE boolean lives_tool_button_set_icon_widget(LiVESToolButton *button, LiVESWidget *icon) {
#ifdef GUI_GTK
  gtk_tool_button_set_icon_widget(button,icon);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tool_button_set_label_widget(LiVESToolButton *button, LiVESWidget *label) {
#ifdef GUI_GTK
  gtk_tool_button_set_label_widget(button,label);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tool_button_set_use_underline(LiVESToolButton *button, boolean use_underline) {
#ifdef GUI_GTK
  gtk_tool_button_set_use_underline(button,use_underline);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_button_set_use_stock(LiVESButton *button, boolean use_stock) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,10,0)
  gtk_button_set_use_stock(button,use_stock);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE void lives_ruler_set_range(LiVESRuler *ruler, double lower, double upper, double position, double max_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_range_set_range(GTK_RANGE(ruler),lower,upper);
  gtk_range_set_value(GTK_RANGE(ruler),position);
#else
  gtk_ruler_set_range(ruler,lower,upper,position,max_size);
#endif
#endif
}


LIVES_INLINE double lives_ruler_get_value(LiVESRuler *ruler) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  return gtk_range_get_value(GTK_RANGE(ruler));
#else
  return ruler->position;
#endif
#endif
}


LIVES_INLINE double lives_ruler_set_value(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_range_set_value(GTK_RANGE(ruler),value);
#else
  ruler->position=value;
#endif
#endif
  return value;
}


LIVES_INLINE double lives_ruler_set_upper(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_adjustment_set_upper(gtk_range_get_adjustment(GTK_RANGE(ruler)),value);
#else
  ruler->upper=value;
#endif
#endif
  return value;
}


LIVES_INLINE double lives_ruler_set_lower(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_adjustment_set_lower(gtk_range_get_adjustment(GTK_RANGE(ruler)),value);
#else
  ruler->lower=value;
#endif
#endif
  return value;
}


LIVES_INLINE LiVESWidget *lives_toolbar_new(void) {
  LiVESWidget *toolbar=NULL;
#ifdef GUI_GTK
  toolbar=gtk_toolbar_new();
#endif
  return toolbar;
}



LIVES_INLINE boolean lives_toolbar_insert(LiVESToolbar *toolbar, LiVESToolItem *item, int pos) {
#ifdef GUI_GTK
  gtk_toolbar_insert(toolbar,item,pos);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_toolbar_set_show_arrow(LiVESToolbar *toolbar, boolean show) {
#ifdef GUI_GTK
  gtk_toolbar_set_show_arrow(toolbar,show);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESIconSize lives_toolbar_get_icon_size(LiVESToolbar *toolbar) {
#ifdef GUI_GTK
  return gtk_toolbar_get_icon_size(toolbar);
#endif
  return LIVES_ICON_SIZE_INVALID;
}



LIVES_INLINE boolean lives_toolbar_set_icon_size(LiVESToolbar *toolbar, LiVESIconSize icon_size) {
#ifdef GUI_GTK
  gtk_toolbar_set_icon_size(toolbar,icon_size);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_toolbar_set_style(LiVESToolbar *toolbar, LiVESToolbarStyle style) {
#ifdef GUI_GTK
  gtk_toolbar_set_style(toolbar,style);
  return TRUE;
#endif
  return FALSE;
}





LIVES_INLINE int lives_widget_get_allocation_x(LiVESWidget *widget) {
  int x=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  x=alloc.x;
#else
  x=widget->allocation.x;
#endif
#endif
  return x;
}


LIVES_INLINE int lives_widget_get_allocation_y(LiVESWidget *widget) {
  int y=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  y=alloc.y;
#else
  y=widget->allocation.y;
#endif
#endif
  return y;
}


LIVES_INLINE int lives_widget_get_allocation_width(LiVESWidget *widget) {
  int width=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  width=alloc.width;
#else
  width=widget->allocation.width;
#endif
#endif
  return width;
}


LIVES_INLINE int lives_widget_get_allocation_height(LiVESWidget *widget) {
  int height=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  height=alloc.height;
#else
  height=widget->allocation.height;
#endif
#endif
  return height;
}


LIVES_INLINE void lives_widget_set_state(LiVESWidget *widget, LiVESWidgetState state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_set_state_flags(widget,state,TRUE);
#else
  gtk_widget_set_state(widget,state);
#endif
#endif
}


LIVES_INLINE LiVESWidgetState lives_widget_get_state(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  return gtk_widget_get_state_flags(widget);
#else
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_state(widget);
#else
  return GTK_WIDGET_STATE(widget);
#endif
#endif
#endif
}



LIVES_INLINE LiVESWidget *lives_bin_get_child(LiVESBin *bin) {
  LiVESWidget *child=NULL;
#ifdef GUI_GTK
  child=gtk_bin_get_child(bin);
#endif
  return child;
}



LIVES_INLINE double lives_adjustment_get_upper(LiVESAdjustment *adj) {
  double upper=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  upper=gtk_adjustment_get_upper(adj);
#else
  upper=adj->upper;
#endif
#endif
  return upper;
}


LIVES_INLINE double lives_adjustment_get_lower(LiVESAdjustment *adj) {
  double lower=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  lower=gtk_adjustment_get_lower(adj);
#else
  lower=adj->lower;
#endif
#endif
  return lower;
}


LIVES_INLINE double lives_adjustment_get_page_size(LiVESAdjustment *adj) {
  double page_size=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  page_size=gtk_adjustment_get_page_size(adj);
#else
  page_size=adj->page_size;
#endif
#endif
  return page_size;
}



LIVES_INLINE void lives_adjustment_set_upper(LiVESAdjustment *adj, double upper) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_set_upper(adj,upper);
#else
  adj->upper=upper;
#endif
#endif
}


LIVES_INLINE void lives_adjustment_set_lower(LiVESAdjustment *adj, double lower) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_set_lower(adj,lower);
#else
  adj->lower=lower;
#endif
#endif
}


LIVES_INLINE void lives_adjustment_set_page_size(LiVESAdjustment *adj, double page_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_set_page_size(adj,page_size);
#else
  adj->page_size=page_size;
#endif
#endif
}



LIVES_INLINE boolean lives_tree_model_get(LiVESTreeModel *tmod, LiVESTreeIter *titer, ...) {
  boolean res=FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_model_get_valist(tmod,titer,argList);
  res=TRUE;
#endif
  va_end(argList);
  return res;
}


LIVES_INLINE boolean lives_tree_model_get_iter(LiVESTreeModel *tmod, LiVESTreeIter *titer, LiVESTreePath *tpath) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter(tmod,titer,tpath);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tree_model_get_iter_first(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter_first(tmod,titer);
#endif
  return FALSE;
}


LIVES_INLINE LiVESTreePath *lives_tree_model_get_path(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
  LiVESTreePath *tpath=NULL;
#ifdef GUI_GTK
  tpath=gtk_tree_model_get_path(tmod,titer);
#endif
  return tpath;
}


 LIVES_INLINE boolean lives_tree_model_iter_children(LiVESTreeModel *tmod, LiVESTreeIter *titer, LiVESTreeIter *parent) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_children(tmod,titer,parent);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tree_model_iter_n_children(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_n_children(tmod,titer);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tree_model_iter_next(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_next(tmod,titer);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tree_path_free(LiVESTreePath *tpath) {
#ifdef GUI_GTK
  gtk_tree_path_free(tpath);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESTreePath *lives_tree_path_new_from_string(const char *path) {
  LiVESTreePath *tpath=NULL;
#ifdef GUI_GTK
  tpath=gtk_tree_path_new_from_string(path);
#endif
  return tpath;
}



LIVES_INLINE int lives_tree_path_get_depth(LiVESTreePath *tpath) {
  int depth=-1;
#ifdef GUI_GTK
  depth=gtk_tree_path_get_depth(tpath);
#endif
  return depth;
}



LIVES_INLINE int *lives_tree_path_get_indices(LiVESTreePath *tpath) {
  int *indices=NULL;
#ifdef GUI_GTK
  indices=gtk_tree_path_get_indices(tpath);
#endif
  return indices;
}



LIVES_INLINE LiVESTreeStore *lives_tree_store_new(int ncols, ...) {
  LiVESTreeStore *tstore=NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols>0) {
    GType *types = (GType *)g_malloc(ncols*sizeof(GType));
    register int i;
    for (i=0;i<ncols;i++) {
      types[i]=va_arg(argList,int);
    }
    tstore=gtk_tree_store_newv(ncols,types);
    g_free(types);
  }
#endif
  va_end(argList);
  return tstore;
}



LIVES_INLINE boolean lives_tree_store_append(LiVESTreeStore *tstore, LiVESTreeIter *titer, LiVESTreeIter *parent) {
#ifdef GUI_GTK
  gtk_tree_store_append(tstore,titer,parent);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_store_set(LiVESTreeStore *tstore, LiVESTreeIter *titer, ...) {
  boolean res=FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_store_set_valist(tstore,titer,argList);
  res=TRUE;
#endif
  va_end(argList);
  return res;
}



LIVES_INLINE LiVESWidget *lives_tree_view_new_with_model(LiVESTreeModel *tmod) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_tree_view_new_with_model(tmod);
#endif
  return tview;
}



LIVES_INLINE LiVESWidget *lives_tree_view_new(void) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_tree_view_new();
#endif
  return tview;
}





LIVES_INLINE boolean lives_tree_view_set_model(LiVESTreeView *tview, LiVESTreeModel *tmod) {
#ifdef GUI_GTK
  gtk_tree_view_set_model(tview,tmod);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESTreeModel *lives_tree_view_get_model(LiVESTreeView *tview) {
  LiVESTreeModel *tmod=NULL;
#ifdef GUI_GTK
  tmod=gtk_tree_view_get_model(tview);
#endif
  return tmod;
}



LIVES_INLINE LiVESTreeSelection *lives_tree_view_get_selection(LiVESTreeView *tview) {
  LiVESTreeSelection *tsel=NULL;
#ifdef GUI_GTK
  tsel=gtk_tree_view_get_selection(tview);
#endif
  return tsel;
}



LIVES_INLINE int lives_tree_view_append_column(LiVESTreeView *tview, LiVESTreeViewColumn *tvcol) {
#ifdef GUI_GTK
  gtk_tree_view_append_column(tview,tvcol);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_view_set_headers_visible(LiVESTreeView *tview, boolean vis) {
#ifdef GUI_GTK
  gtk_tree_view_set_headers_visible(tview,vis);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE LiVESAdjustment *lives_tree_view_get_hadjustment(LiVESTreeView *tview) {
  LiVESAdjustment *adj=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  adj=gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(tview));
#else
  adj=gtk_tree_view_get_hadjustment(tview);
#endif
#endif
  return adj;
}




LIVES_INLINE LiVESTreeViewColumn *lives_tree_view_column_new_with_attributes(const char *title, LiVESCellRenderer *crend, ...) {
  LiVESTreeViewColumn *tvcol=NULL;
  va_list args;
  va_start(args, crend);
#ifdef GUI_GTK
  int column;
  char *attribute;
  boolean expand=FALSE;

  tvcol=gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(tvcol,title);
  gtk_tree_view_column_pack_start(tvcol,crend,expand);

  attribute=va_arg(args, char *);

  while (attribute != NULL) {
    column = va_arg (args, int);
    gtk_tree_view_column_add_attribute (tvcol, crend, attribute, column);
    attribute = va_arg (args, char *);
  }

#endif
  va_end(args);
  return tvcol;
}




LIVES_INLINE boolean lives_tree_view_column_set_sizing(LiVESTreeViewColumn *tvcol, LiVESTreeViewColumnSizing type) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_sizing(tvcol,type);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_view_column_set_fixed_width(LiVESTreeViewColumn *tvcol, int fwidth) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_fixed_width(tvcol,fwidth);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_selection_get_selected(LiVESTreeSelection *tsel, LiVESTreeModel **tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_selection_get_selected(tsel,tmod,titer);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_selection_set_mode(LiVESTreeSelection *tsel, LiVESSelectionMode tselmod) {
#ifdef GUI_GTK
  gtk_tree_selection_set_mode(tsel,tselmod);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_selection_select_iter(LiVESTreeSelection *tsel, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  gtk_tree_selection_select_iter(tsel,titer);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE const char *lives_label_get_text(LiVESLabel *label) {
#ifdef GUI_GTK
  return gtk_label_get_text(label);
#endif
  return NULL;
}


LIVES_INLINE boolean lives_label_set_text(LiVESLabel *label, const char *text) {
#ifdef GUI_GTK
  gtk_label_set_text(label,text);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_label_set_text_with_mnemonic(LiVESLabel *label, const char *text) {
#ifdef GUI_GTK
  gtk_label_set_text_with_mnemonic(label,text);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_entry_new(void) {
  LiVESWidget *entry=NULL;
#ifdef GUI_GTK
  entry=gtk_entry_new();
#endif
  return entry;
}




LIVES_INLINE boolean lives_entry_set_max_length(LiVESEntry *entry, int len) {
#ifdef GUI_GTK
  gtk_entry_set_max_length(entry,len);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_activates_default(LiVESEntry *entry, boolean act) {
#ifdef GUI_GTK
  gtk_entry_set_activates_default(entry,act);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_visibility(LiVESEntry *entry, boolean vis) {
#ifdef GUI_GTK
  gtk_entry_set_visibility(entry,vis);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_has_frame(LiVESEntry *entry, boolean has) {
#ifdef GUI_GTK
  gtk_entry_set_has_frame(entry,has);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_editable(LiVESEntry *entry, boolean editable) {
#ifdef GUI_GTK
  gtk_editable_set_editable(GTK_EDITABLE(entry),editable);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE const char *lives_entry_get_text(LiVESEntry *entry) {
#ifdef GUI_GTK
  return gtk_entry_get_text(entry);
#endif
  return NULL;
}


LIVES_INLINE boolean lives_entry_set_text(LiVESEntry *entry, const char *text) {
#ifdef GUI_GTK
  gtk_entry_set_text(entry,text);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_width_chars(LiVESEntry *entry, int nchars) {
#ifdef GUI_GTK
  gtk_entry_set_width_chars(entry,nchars);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_scrolled_window_set_policy(LiVESScrolledWindow *scrolledwindow, LiVESPolicyType hpolicy, 
						      LiVESPolicyType vpolicy) {
#ifdef GUI_GTK
  gtk_scrolled_window_set_policy (scrolledwindow, hpolicy, vpolicy);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_scrolled_window_add_with_viewport(LiVESScrolledWindow *scrolledwindow, LiVESWidget *child) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,8,0)
  gtk_scrolled_window_add_with_viewport(scrolledwindow, child);
#else
  lives_container_add(LIVES_CONTAINER(scrolledwindow),child);
#endif
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_xwindow_raise(LiVESXWindow *xwin) {
#ifdef GUI_GTK
  gdk_window_raise(xwin);
  return TRUE;
#endif
  return FALSE;
}


 LIVES_INLINE boolean lives_xwindow_set_cursor(LiVESXWindow *xwin, LiVESXCursor *cursor) {
#ifdef GUI_GTK
   gdk_window_set_cursor(xwin,cursor);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_dialog_set_has_separator(LiVESDialog *dialog, boolean has) {
  // return TRUE if implemented

#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
  gtk_dialog_set_has_separator(dialog,has);
  return TRUE;
#endif
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_widget_set_hexpand(LiVESWidget *widget, boolean state) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_set_hexpand(widget,state);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_vexpand(LiVESWidget *widget, boolean state) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_set_vexpand(widget,state);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_menu_new(void) {
  LiVESWidget *menu=NULL;
#ifdef GUI_GTK
  menu=gtk_menu_new();
#endif
  return menu;
}


LIVES_INLINE LiVESWidget *lives_menu_item_new(void) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_menu_item_new();
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}



LIVES_INLINE LiVESWidget *lives_menu_item_new_with_mnemonic(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_menu_item_new_with_mnemonic(label);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}



LIVES_INLINE LiVESWidget *lives_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_menu_item_new_with_label(label);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESWidget *lives_image_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  menuitem=gtk_menu_item_new_with_label(label);
#else
  menuitem=gtk_image_menu_item_new_with_label(label);
#endif
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESWidget *lives_image_menu_item_new_with_mnemonic(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  menuitem=gtk_menu_item_new_with_mnemonic(label);
#else
  menuitem=gtk_image_menu_item_new_with_mnemonic(label);
#endif
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESWidget *lives_radio_menu_item_new_with_label(LiVESSList *group, const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_radio_menu_item_new_with_label(group,label);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESWidget *lives_check_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_check_menu_item_new_with_label(label);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESWidget *lives_check_menu_item_new_with_mnemonic(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  // TODO - deprecated
  menuitem=gtk_check_menu_item_new_with_mnemonic(label);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  char *xstock_id=g_strdup(stock_id); // need to back this up as we will use translation functions
  menuitem=gtk_menu_item_new_with_mnemonic(xstock_id);
  
  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_SAVE)) {
    gtk_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem),"<LiVES>/save");
  }

  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_QUIT)) {
    gtk_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem),"<LiVES>/quit");
  }
  g_free(xstock_id);
#else
  menuitem=gtk_image_menu_item_new_from_stock(stock_id,accel_group);
#endif
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE void lives_menu_item_set_submenu(LiVESMenuItem *menuitem, LiVESWidget *menu) {
#ifdef GUI_GTK
  gtk_menu_item_set_submenu(menuitem,menu);
#endif
}


LIVES_INLINE void lives_check_menu_item_set_active(LiVESCheckMenuItem *item, boolean state) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_active(item,state);
#endif
}


LIVES_INLINE boolean lives_check_menu_item_get_active(LiVESCheckMenuItem *item) {
#ifdef GUI_GTK
  return gtk_check_menu_item_get_active(item);
#endif
  return FALSE;
}


#if !GTK_CHECK_VERSION(3,10,0)

LIVES_INLINE void lives_image_menu_item_set_image(LiVESImageMenuItem *item, LiVESWidget *image) {
#ifdef GUI_GTK
  gtk_image_menu_item_set_image(item,image);
#endif
}

#endif

LIVES_INLINE boolean lives_menu_set_title(LiVESMenu *menu, const char *title) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,10,0)
  gtk_menu_set_title(menu,title);
  return TRUE;
#endif
  return FALSE;
#endif
}



LIVES_INLINE void lives_menu_popup(LiVESMenu *menu, LiVESXEventButton *event) {
#ifdef GUI_GTK
  gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
#endif
}



LIVES_INLINE boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem *item, boolean show) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,16,0)
#if !GTK_CHECK_VERSION(3,10,0)
  gtk_image_menu_item_set_always_show_image(item,show);
#endif
  return TRUE;
#endif
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_scale_button_set_orientation(LiVESScaleButton *scale, LiVESOrientation orientation) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_orientable_set_orientation(GTK_ORIENTABLE(scale),orientation);
  return TRUE;
#else
#if GTK_CHECK_VERSION(2,14,0) 
  gtk_scale_button_set_orientation (scale,orientation);
  return TRUE;
#endif
#endif
#endif
  return FALSE;
}


LIVES_INLINE double lives_scale_button_get_value(LiVESScaleButton *scale) {
  double value=0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  value=gtk_scale_button_get_value(scale);
#else
  value=gtk_adjustment_get_value(gtk_range_get_adjustment(scale));
#endif
#endif
  return value;
}


LIVES_INLINE char *lives_file_chooser_get_filename(LiVESFileChooser *chooser) {
  char *fname=NULL;
#ifdef GUI_GTK
  fname=gtk_file_chooser_get_filename(chooser);
#endif
  return fname;
}


LIVES_INLINE LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser *chooser) {
  LiVESSList *fnlist=NULL;
#ifdef GUI_GTK
  fnlist=gtk_file_chooser_get_filenames(chooser);
#endif
  return fnlist;
}

LIVES_INLINE LiVESWidget *lives_grid_new(void) {
  LiVESWidget *grid=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  grid=gtk_grid_new();
#endif
#endif
  return grid;
}


LIVES_INLINE boolean lives_grid_set_row_spacing(LiVESGrid *grid, uint32_t spacing) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_set_row_spacing(grid,spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_grid_set_column_spacing(LiVESGrid *grid, uint32_t spacing) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_set_column_spacing(grid,spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_grid_remove_row(LiVESGrid *grid, int posn) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  gtk_grid_remove_row(grid,posn);
  return TRUE;
#endif
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_grid_insert_row(LiVESGrid *grid, int posn) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  gtk_grid_insert_row(grid,posn);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_grid_attach_next_to(LiVESGrid *grid, LiVESWidget *child, LiVESWidget *sibling, 
					       LiVESPositionType side, int width, int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
  gtk_grid_attach_next_to(grid,child,sibling,side,width,height);
  return TRUE;
#endif
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_frame_new(const char *label) {
  LiVESWidget *frame=NULL;
#ifdef GUI_GTK
  frame=gtk_frame_new(label);
#endif
  return frame;
}



LIVES_INLINE boolean lives_frame_set_label(LiVESFrame *frame, const char *label) {
#ifdef GUI_GTK
  gtk_frame_set_label(frame,label);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_frame_set_label_widget(LiVESFrame *frame, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_frame_set_label_widget(frame,widget);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_frame_get_label_widget(LiVESFrame *frame) {
  LiVESWidget *widget=NULL;
#ifdef GUI_GTK
  widget=gtk_frame_get_label_widget(frame);
#endif
  return widget;
}



LIVES_INLINE boolean lives_frame_set_shadow_type(LiVESFrame *frame, LiVESShadowType stype) {
#ifdef GUI_GTK
  gtk_frame_set_shadow_type(frame,stype);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE LiVESWidget *lives_table_new(uint32_t rows, uint32_t cols, boolean homogeneous) {
  LiVESWidget *table=NULL;
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  register int i;
  GtkGrid *grid=(GtkGrid *)lives_grid_new();
  gtk_grid_set_row_homogeneous(grid,homogeneous);
  gtk_grid_set_column_homogeneous(grid,homogeneous);

  for (i=0;i<rows;i++) {
    gtk_grid_insert_row(grid,0);
  }

  for (i=0;i<cols;i++) {
    gtk_grid_insert_column(grid,0);
  }

  g_object_set_data(G_OBJECT(grid),"rows",LIVES_INT_TO_POINTER(rows));
  g_object_set_data(G_OBJECT(grid),"cols",LIVES_INT_TO_POINTER(cols));
  table=(LiVESWidget *)grid;
#else
  table=gtk_table_new(rows,cols,homogeneous);
#endif
#endif
  return table;
}


LIVES_INLINE boolean lives_table_set_row_spacings(LiVESTable *table, uint32_t spacing) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  lives_grid_set_row_spacing(table,spacing);
#else
  gtk_table_set_row_spacings(table,spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_table_set_col_spacings(LiVESTable *table, uint32_t spacing) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  lives_grid_set_column_spacing(table,spacing);
#else
  gtk_table_set_col_spacings(table,spacing);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_table_resize(LiVESTable *table, uint32_t rows, uint32_t cols) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  register int i;

  for (i=LIVES_POINTER_TO_INT(g_object_get_data(G_OBJECT(table),"rows"));i<rows;i++) {
    gtk_grid_insert_row(table,i);
  }

  for (i=LIVES_POINTER_TO_INT(g_object_get_data(G_OBJECT(table),"cols"));i<cols;i++) {
    gtk_grid_insert_column(table,i);
  }

  g_object_set_data(G_OBJECT(table),"rows",LIVES_INT_TO_POINTER(rows));
  g_object_set_data(G_OBJECT(table),"cols",LIVES_INT_TO_POINTER(cols));

#else
  gtk_table_resize(table,rows,cols);
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE void lives_table_attach(LiVESTable *table, LiVESWidget *child, uint32_t left, uint32_t right, 
				     uint32_t top, uint32_t bottom, LiVESAttachOptions xoptions, LiVESAttachOptions yoptions,
				     uint32_t xpad, uint32_t ypad) {

#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  gtk_grid_attach(table,child,left,top,right-left,bottom-top);
  if (xoptions&LIVES_EXPAND) 
    lives_widget_set_hexpand(child,TRUE);
  else
    lives_widget_set_hexpand(child,FALSE);
  if (yoptions&LIVES_EXPAND) 
    lives_widget_set_vexpand(child,TRUE);
  else
    lives_widget_set_vexpand(child,FALSE);

  gtk_widget_set_margin_left(child,xpad);
  gtk_widget_set_margin_right(child,xpad);

  gtk_widget_set_margin_top(child,ypad);
  gtk_widget_set_margin_bottom(child,ypad);
#else
  gtk_table_attach(table,child,left,right,top,bottom,xoptions,yoptions,xpad,ypad);
#endif
#endif
}


LIVES_INLINE LiVESWidget *lives_color_button_new_with_color(const LiVESWidgetColor *color) {
  LiVESWidget *cbutton=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  cbutton=gtk_color_button_new_with_rgba(color);
#else
  cbutton=gtk_color_button_new_with_color(color);
#endif
#endif
  return cbutton;
}


LIVES_INLINE boolean lives_color_button_get_color(LiVESColorButton *button, LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,4,0)
  gtk_color_chooser_get_rgba((GtkColorChooser *)button,color);
#else
#if GTK_CHECK_VERSION(3,0,0)
  gtk_color_button_get_rgba((GtkColorChooser *)button,color);
#else
  gtk_color_button_get_color(button,color);
#endif
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_color_button_set_color(LiVESColorButton *button, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,4,0)
  gtk_color_chooser_set_rgba((GtkColorChooser *)button,color);
#else
#if GTK_CHECK_VERSION(3,0,0)
  gtk_color_button_set_rgba((GtkColorChooser *)button,color);
#else
  gtk_color_button_set_color(button,color);
#endif
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_color_button_set_title(LiVESColorButton *button, const char *title) {
#ifdef GUI_GTK
  gtk_color_button_set_title(button,title);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE boolean lives_color_button_set_use_alpha(LiVESColorButton *button, boolean use_alpha) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,4,0)
  gtk_color_chooser_set_use_alpha((GtkColorChooser *)button,use_alpha);
#else
#if GTK_CHECK_VERSION(3,0,0)
  gtk_color_button_set_use_alpha((GtkColorChooser *)button,use_alpha);
#else
  gtk_color_button_set_use_alpha(button,use_alpha);
#endif
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESAccelGroup *lives_accel_group_new(void) {
  LiVESAccelGroup *group=NULL;
#ifdef GUI_GTK
  group=gtk_accel_group_new();
#endif
  return group;
}


LIVES_INLINE void lives_accel_group_connect(LiVESAccelGroup *group, uint32_t key, LiVESModifierType mod, 
					    LiVESAccelFlags flags, LiVESWidgetClosure *closure) {

#ifdef GUI_GTK
  gtk_accel_group_connect(group,key,mod,flags,closure);
#endif

}



LIVES_INLINE void lives_accel_group_disconnect(LiVESAccelGroup *group, LiVESWidgetClosure *closure) {

#ifdef GUI_GTK
  gtk_accel_group_disconnect(group,closure);
#endif

}




LIVES_INLINE void lives_widget_add_accelerator(LiVESWidget *widget, const char *accel_signal, LiVESAccelGroup *accel_group,
					       uint32_t accel_key, LiVESModifierType accel_mods, LiVESAccelFlags accel_flags) {
#ifdef GUI_GTK
  gtk_widget_add_accelerator(widget,accel_signal,accel_group,accel_key,accel_mods,accel_flags);
#endif
}



LIVES_INLINE void lives_accel_groups_activate(LiVESObject *object, uint32_t key, LiVESModifierType mod) {
#ifdef GUI_GTK
  gtk_accel_groups_activate(object,key,mod);
#endif

}





LIVES_INLINE void lives_widget_get_pointer(LiVESXDevice *device, LiVESWidget *widget, int *x, int *y) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  // try: gdk_event_get_device (event)
  LiVESXWindow *xwin;
  if (widget==NULL) xwin=gdk_get_default_root_window ();
  else xwin=lives_widget_get_xwindow(widget);
  if (xwin==NULL) {
    LIVES_ERROR("Tried to get pointer for windowless widget");
    return;
  }
  gdk_window_get_device_position (xwin,device,x,y,NULL);
#else 
  gtk_widget_get_pointer(widget,x,y);
#endif
#endif

}


LIVES_INLINE LiVESXDisplay *lives_widget_get_display(LiVESWidget *widget) {
  LiVESXDisplay *disp=NULL;
#ifdef GUI_GTK
  disp=gtk_widget_get_display(widget);
#endif
  return disp;
}


LIVES_INLINE LiVESXWindow *lives_display_get_window_at_pointer 
(LiVESXDevice *device, LiVESXDisplay *display, int *win_x, int *win_y) {
  LiVESXWindow *xwindow=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (device==NULL) return NULL;
  xwindow=gdk_device_get_window_at_position (device,win_x,win_y);
#else
  xwindow=gdk_display_get_window_at_pointer(display,win_x,win_y);
#endif
#endif
  return xwindow;
}


LIVES_INLINE void lives_display_get_pointer 
(LiVESXDevice *device, LiVESXDisplay *display, LiVESXScreen **screen, int *x, int *y, LiVESModifierType *mask) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (device==NULL) return;
  gdk_device_get_position (device,screen,x,y);
#else
  gdk_display_get_pointer(display,screen,x,y,mask);
#endif
#endif
}


LIVES_INLINE void lives_display_warp_pointer 
(LiVESXDevice *device, LiVESXDisplay *display, LiVESXScreen *screen, int x, int y) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (device==NULL) return;
  gdk_device_warp (device,screen,x,y);
#else
#if GLIB_CHECK_VERSION(2,8,0)
  gdk_display_warp_pointer(display,screen,x,y);
#endif
#endif
#endif
}


LIVES_INLINE lives_display_t lives_widget_get_display_type(LiVESWidget *widget) {
  lives_display_t dtype=LIVES_DISPLAY_TYPE_UNKNOWN;
#ifdef GUI_GTK
  LiVESXDisplay *display=gtk_widget_get_display(widget);
  display=display; // stop compiler complaining
  if (GDK_IS_X11_DISPLAY(display)) dtype=LIVES_DISPLAY_TYPE_X11;
  else if (GDK_IS_WIN32_DISPLAY(display)) dtype=LIVES_DISPLAY_TYPE_WIN32;
#endif
  return dtype;
}


LIVES_INLINE uint64_t lives_widget_get_xwinid(LiVESWidget *widget, const gchar *msg) {
  uint64_t xwin=-1;
#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
  if (lives_widget_get_display_type(widget)==LIVES_DISPLAY_TYPE_X11)
    xwin=(uint64_t)GDK_WINDOW_XID (lives_widget_get_xwindow(widget));
  else
#endif
#ifdef GDK_WINDOWING_WIN32
    if (lives_widget_get_display_type(widget)==LIVES_DISPLAY_TYPE_WIN32)
      xwin=(uint64_t)gdk_win32_drawable_get_handle (lives_widget_get_xwindow(widget));
    else 
#endif
#endif
      if (msg!=NULL) LIVES_WARN(msg);

  return xwin;
}


// compound functions

void lives_painter_set_source_to_bg(lives_painter_t *cr, LiVESWidget *widget) {
  LiVESWidgetColor color;
  lives_widget_get_bg_color(widget, &color);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  lives_painter_set_source_rgba (cr, 
				 LIVES_WIDGET_COLOR_SCALE(color.red), 
				 LIVES_WIDGET_COLOR_SCALE(color.green), 
				 LIVES_WIDGET_COLOR_SCALE(color.blue), 
				 LIVES_WIDGET_COLOR_SCALE(color.alpha));
#else
  lives_painter_set_source_rgb (cr, 
				LIVES_WIDGET_COLOR_SCALE(color.red), 
				LIVES_WIDGET_COLOR_SCALE(color.green), 
				LIVES_WIDGET_COLOR_SCALE(color.blue));
#endif
}



void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  gchar *text=gtk_widget_get_tooltip_text(source);
  lives_widget_set_tooltip_text(dest,text);
  g_free(text);
#else
  GtkTooltipsData *td=gtk_tooltips_data_get(source);
  if (td==NULL) return;
  gtk_tooltips_set_tip (td->tooltips, dest, td->tip_text, td->tip_private);
#endif
#endif
}


void lives_combo_populate(LiVESCombo *combo, LiVESList *list) {
  // remove any current list
  lives_combo_set_active_index(combo,-1);
  lives_combo_remove_all_text(combo);

  // add the new list
  while (list!=NULL) {
    lives_combo_append_text(LIVES_COMBO(combo),(const char *)list->data);
    list=list->next;
  }
}

///// lives compounds


LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment *adj, double volume) {
  LiVESWidget *volume_scale=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0) 
  volume_scale=gtk_volume_button_new();
  gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_scale),volume);
  lives_scale_button_set_orientation (LIVES_SCALE_BUTTON(volume_scale),orientation);
#else
  if (orientation==LIVES_ORIENTATION_HORIZONTAL)
    volume_scale=lives_hscale_new(adj);
  else 
    volume_scale=lives_vscale_new(adj);

  gtk_scale_set_draw_value(GTK_SCALE(volume_scale),FALSE);
#endif
#endif
  return volume_scale;
}


LiVESWidget *lives_standard_label_new(const char *text) {
  LiVESWidget *label=NULL;

  label=lives_label_new(text);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
  }

  return label;
}


LiVESWidget *lives_standard_label_new_with_mnemonic(const char *text, LiVESWidget *mnemonic_widget) {
  LiVESWidget *label=NULL;
#ifdef GUI_GTK

  label=gtk_label_new_with_mnemonic(text);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
  }
  gtk_label_set_justify (LIVES_LABEL (label), widget_opts.justify);
  gtk_label_set_line_wrap (LIVES_LABEL (label), widget_opts.line_wrap);

  if (mnemonic_widget!=NULL) gtk_label_set_mnemonic_widget (LIVES_LABEL(label),mnemonic_widget);
#endif

  return label;
}


LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, 
					     const char *tooltip) {
  LiVESWidget *checkbutton=NULL;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;

  checkbutton = gtk_check_button_new ();
  if (tooltip!=NULL) lives_widget_set_tooltip_text(checkbutton, tooltip);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    eventbox=lives_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,checkbutton);

    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,checkbutton);
    }
    else label=lives_standard_label_new (labeltext);

    lives_container_add(LIVES_CONTAINER(eventbox),label);

    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      checkbutton);
  
    widget_opts.last_label=label;

    if (widget_opts.apply_theme) {
      lives_widget_apply_theme(eventbox,LIVES_WIDGET_STATE_NORMAL);
    }
  }
  
  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      if (!widget_opts.no_gui) {
	lives_widget_show(hbox);
      }
      lives_box_pack_start (LIVES_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }
    
    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);
    
    
    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    
    lives_box_pack_start (LIVES_BOX (hbox), checkbutton, widget_opts.expand==LIVES_EXPAND_EXTRA, FALSE, eventbox==NULL?0:widget_opts.packing_width);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

  }

  if (label!=NULL) {
#if GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after (GTK_OBJECT (checkbutton), "state_flags_changed",
			    G_CALLBACK (set_label_state),
			    label);
#else
    g_signal_connect_after (GTK_OBJECT (checkbutton), "state_changed",
			    G_CALLBACK (set_label_state),
			    label);
#endif
  }

#endif

  return checkbutton;
}




LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *box, const char *tooltip) {
  LiVESWidget *radiobutton=NULL;

  // pack a themed check button into box



#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;

  radiobutton = gtk_radio_button_new (rbgroup);

  if (tooltip!=NULL) lives_widget_set_tooltip_text(radiobutton, tooltip);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,radiobutton);
    }
    else label=lives_standard_label_new (labeltext);

    widget_opts.last_label=label;
    
    eventbox=lives_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,radiobutton);
    lives_container_add(LIVES_CONTAINER(eventbox),label);

    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton);
    
    if (widget_opts.apply_theme) {
      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    }
  }


  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      if (!widget_opts.no_gui) {
	lives_widget_show(hbox);
      }
      lives_box_pack_start (LIVES_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }
    
    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);
    
    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

    lives_box_pack_start (LIVES_BOX (hbox), radiobutton, widget_opts.expand==LIVES_EXPAND_EXTRA, FALSE, eventbox==NULL?0:widget_opts.packing_width);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

  if (label!=NULL) {
#if GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after (GTK_OBJECT (radiobutton), "state_flags_changed",
			    G_CALLBACK (set_label_state),
			    label);
#else
    g_signal_connect_after (GTK_OBJECT (radiobutton), "state_changed",
			    G_CALLBACK (set_label_state),
			    label);
#endif
  }

#endif

  return radiobutton;
}


size_t calc_spin_button_width(double min, double max, int dp) {
  char *txt=g_strdup_printf ("%d",(int)max);
  size_t maxlen=strlen (txt);
  g_free (txt);
  txt=g_strdup_printf ("%d",(int)min);
  if (strlen (txt)>maxlen) maxlen=strlen (txt);
  g_free (txt);
  if (dp>0) maxlen+=3;
  return maxlen;
}


LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *box, 
					    const char *tooltip) {
  LiVESWidget *spinbutton=NULL;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;
  LiVESAdjustment *adj;

  boolean expand=FALSE;

  int maxlen;

  adj = lives_adjustment_new (val, min, max, step, page, 0.);
  spinbutton = gtk_spin_button_new (adj, 1, dp);
  if (tooltip!=NULL) lives_widget_set_tooltip_text(spinbutton, tooltip);

  maxlen=calc_spin_button_width(min,max,dp);
  lives_entry_set_width_chars (LIVES_ENTRY (spinbutton),maxlen);

  lives_entry_set_activates_default (LIVES_ENTRY (spinbutton), TRUE);
  gtk_spin_button_set_update_policy (LIVES_SPIN_BUTTON (spinbutton),GTK_UPDATE_ALWAYS);
  gtk_spin_button_set_numeric (LIVES_SPIN_BUTTON (spinbutton),TRUE);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,spinbutton);
    }
    else label=lives_standard_label_new (labeltext);
    
    widget_opts.last_label=label;

    eventbox=lives_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,spinbutton);
    lives_container_add(LIVES_CONTAINER(eventbox),label);
    
    if (widget_opts.apply_theme) {
      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    }
  }

  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      if (!widget_opts.no_gui) {
	lives_widget_show(hbox);
      }
      lives_box_pack_start (LIVES_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_height);
      expand=widget_opts.expand!=LIVES_EXPAND_NONE;
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    
    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start (LIVES_BOX (hbox), spinbutton, widget_opts.expand==LIVES_EXPAND_EXTRA, FALSE, widget_opts.packing_width);
    
    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

  if (label!=NULL) {
#if GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after (GTK_OBJECT (spinbutton), "state_flags_changed",
			    G_CALLBACK (set_label_state),
			    label);
#else
    g_signal_connect_after (GTK_OBJECT (spinbutton), "state_changed",
			    G_CALLBACK (set_label_state),
			    label);
#endif
  }

#endif

  return spinbutton;
}






LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip) {
  LiVESWidget *combo=NULL;

  // pack a themed combo box into box

  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;
  LiVESEntry *entry;

  combo=lives_combo_new();
  if (tooltip!=NULL) lives_widget_set_tooltip_text(combo, tooltip);

  entry=(LiVESEntry *)lives_combo_get_entry(LIVES_COMBO(combo));

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic (labeltext,LIVES_WIDGET(entry));
    }
    else label = lives_standard_label_new (labeltext);

    widget_opts.last_label=label;
    
    eventbox=lives_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,combo);
    lives_container_add(LIVES_CONTAINER(eventbox),label);
    
    if (widget_opts.apply_theme) {
      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    }
  }

  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      lives_box_pack_start (LIVES_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (widget_opts.expand==LIVES_EXPAND_DEFAULT) {
      LiVESWidget *label=lives_standard_label_new("");
      lives_box_pack_start (LIVES_BOX (hbox), label, TRUE, FALSE, 0);
    }
    
    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    lives_box_pack_start (LIVES_BOX (hbox), combo, widget_opts.expand!=LIVES_EXPAND_NONE, 
			widget_opts.expand==LIVES_EXPAND_EXTRA, eventbox==NULL?0:widget_opts.packing_width);
    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);


    if (widget_opts.expand==LIVES_EXPAND_DEFAULT) {
      LiVESWidget *label=lives_standard_label_new("");
      lives_box_pack_start (LIVES_BOX (hbox), label, TRUE, FALSE, 0);
    }


  }

  lives_entry_set_editable (LIVES_ENTRY(entry),FALSE);
  lives_entry_set_activates_default(entry,TRUE);

  if (list!=NULL) {
    lives_combo_populate(LIVES_COMBO(combo),list);
    if (list!=NULL) lives_combo_set_active_index(LIVES_COMBO(combo),0);
  }

  if (label!=NULL) {
#if GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after (GTK_OBJECT (combo), "state_flags_changed",
			    G_CALLBACK (set_label_state),
			    label);
#else
    g_signal_connect_after (GTK_OBJECT (combo), "state_changed",
			    G_CALLBACK (set_label_state),
			    label);
#endif
  }


  return combo;
}


LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, const char *txt, int dispwidth, int maxchars, LiVESBox *box, 
					     const char *tooltip) {

  LiVESWidget *entry=NULL;

  LiVESWidget *label=NULL;

  LiVESWidget *hbox=NULL;

  entry=lives_entry_new();

  if (tooltip!=NULL) lives_widget_set_tooltip_text(entry, tooltip);

  if (txt!=NULL)
    lives_entry_set_text (LIVES_ENTRY (entry),txt);

  if (dispwidth!=-1) lives_entry_set_width_chars (LIVES_ENTRY (entry),dispwidth);
  if (maxchars!=-1) lives_entry_set_max_length(LIVES_ENTRY (entry),maxchars);

  lives_entry_set_activates_default (LIVES_ENTRY (entry), TRUE);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic (labeltext,entry);
    }
    else label = lives_standard_label_new (labeltext);

    widget_opts.last_label=label;

    if (tooltip!=NULL) lives_tooltips_copy(label,entry);
  }


  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      if (!widget_opts.no_gui) {
	lives_widget_show(hbox);
      }
      lives_box_pack_start (LIVES_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }
    
    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (!widget_opts.swap_label&&label!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
    
    lives_box_pack_start (LIVES_BOX (hbox), entry, widget_opts.expand!=LIVES_EXPAND_NONE, dispwidth==-1, widget_opts.packing_width);
    
    if (widget_opts.swap_label&&label!=NULL)
      lives_box_pack_start (LIVES_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
  }

  if (label!=NULL) {
#if GTK_CHECK_VERSION(3,0,0)
    g_signal_connect_after (GTK_OBJECT (entry), "state_flags_changed",
			    G_CALLBACK (set_label_state),
			    label);
#else
    g_signal_connect_after (GTK_OBJECT (entry), "state_changed",
			    G_CALLBACK (set_label_state),
			    label);
#endif
  }


  return entry;
}



LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons) {
  LiVESWidget *dialog=NULL;

#ifdef GUI_GTK

  dialog = gtk_dialog_new ();

  if (title!=NULL)
    lives_window_set_title (LIVES_WINDOW (dialog), title);

  lives_window_set_deletable(LIVES_WINDOW(dialog), FALSE);
  lives_window_set_resizable (LIVES_WINDOW (dialog), FALSE);

  lives_widget_set_hexpand(dialog,TRUE);
  lives_widget_set_vexpand(dialog,TRUE);

  lives_container_set_border_width (LIVES_CONTAINER (dialog), widget_opts.border_width*2);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(dialog, LIVES_WIDGET_STATE_NORMAL);

#if !GTK_CHECK_VERSION(3,0,0)
    lives_dialog_set_has_separator(LIVES_DIALOG(dialog),FALSE);
#endif
  }

  // do this before widget_show(), then call lives_window_center() afterwards
  lives_window_set_position(LIVES_WINDOW(dialog),LIVES_WIN_POS_CENTER_ALWAYS);

  if (add_std_buttons) {
    LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new ());
    LiVESWidget *cancelbutton = lives_button_new_from_stock ("gtk-cancel");
    LiVESWidget *okbutton = lives_button_new_from_stock ("gtk-ok");

    lives_window_add_accel_group (LIVES_WINDOW (dialog), accel_group);

    lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

    lives_widget_add_accelerator (cancelbutton, "activate", accel_group,
				LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

    lives_widget_set_can_focus_and_default(cancelbutton);

    lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);

    lives_widget_set_can_focus_and_default(okbutton);
    lives_widget_grab_default (okbutton);
  }

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  // must do this before setting modal !
  if (!widget_opts.no_gui) {
    lives_widget_show(dialog);
    lives_window_center(LIVES_WINDOW(dialog));
  }

  if (!widget_opts.non_modal)
    lives_window_set_modal (LIVES_WINDOW (dialog), TRUE);

#endif

  return dialog;

}



LiVESWidget *lives_standard_hruler_new(void) {
  LiVESWidget *hruler=NULL;

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hruler=gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,NULL);
  gtk_scale_set_draw_value(GTK_SCALE(hruler),FALSE);
#if GTK_CHECK_VERSION(3,4,0)
  gtk_scale_set_has_origin(GTK_SCALE(hruler),FALSE);
#endif
  gtk_scale_set_digits(GTK_SCALE(hruler),8);
#else
  hruler=gtk_hruler_new();
#endif

#endif

  return hruler;
}



LiVESWidget *lives_standard_scrolled_window_new(int width, int height, LiVESWidget *child) {
  LiVESWidget *scrolledwindow=NULL;

  LiVESWidget *swchild;

#ifdef GUI_GTK
  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  lives_scrolled_window_set_policy (LIVES_SCROLLED_WINDOW (scrolledwindow), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);

  if (widget_opts.apply_theme) {
    lives_widget_set_hexpand(scrolledwindow,TRUE);
    lives_widget_set_vexpand(scrolledwindow,TRUE);
    lives_container_set_border_width (LIVES_CONTAINER (scrolledwindow), widget_opts.border_width);
  }

  if (child!=NULL) {
#if GTK_CHECK_VERSION(3,0,0)
    if (!LIVES_IS_SCROLLABLE(child))
#else
    if (!LIVES_IS_TEXT_VIEW(child))
#endif 
      {
	lives_scrolled_window_add_with_viewport (LIVES_SCROLLED_WINDOW (scrolledwindow), child);
      }
    else {
      if (widget_opts.expand!=LIVES_EXPAND_NONE) {
	LiVESWidget *align;
	align=lives_alignment_new(.5,0.,0.,0.);
	lives_container_add (LIVES_CONTAINER (align), child);
	lives_scrolled_window_add_with_viewport (LIVES_SCROLLED_WINDOW (scrolledwindow), align);
      }
      else {
	lives_scrolled_window_add_with_viewport (LIVES_SCROLLED_WINDOW (scrolledwindow), child);
      }
    }
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (lives_bin_get_child (LIVES_BIN (scrolledwindow))),LIVES_SHADOW_IN);
  }

  swchild=lives_bin_get_child(LIVES_BIN(scrolledwindow));

  if (width!=0&&height!=0) {
#if !GTK_CHECK_VERSION(3,0,0)
    lives_widget_set_size_request (scrolledwindow, width, height);
#else
    if (height!=-1) gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolledwindow),height);
    if (width!=-1) gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrolledwindow),width);
#endif
  }

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(swchild, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_set_hexpand(swchild,TRUE);
    lives_widget_set_vexpand(swchild,TRUE);
    if (LIVES_IS_CONTAINER(child)) lives_container_set_border_width (LIVES_CONTAINER (child), widget_opts.border_width>>1);
  }
#endif

  return scrolledwindow;
}



LiVESWidget *lives_standard_expander_new(const char *ltext, boolean use_mnemonic, LiVESBox *parent, LiVESWidget *child) {
  LiVESWidget *expander=NULL;

#ifdef GUI_GTK

  if (use_mnemonic)
    expander=gtk_expander_new_with_mnemonic(ltext);
  else 
    expander=gtk_expander_new(ltext);

  if (widget_opts.apply_theme) {
    LiVESWidget *label=gtk_expander_get_label_widget(GTK_EXPANDER(expander));
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_PRELIGHT);
    lives_widget_apply_theme(expander, LIVES_WIDGET_STATE_PRELIGHT);
    lives_widget_apply_theme(expander, LIVES_WIDGET_STATE_NORMAL);
  }
  
  gtk_container_forall(LIVES_CONTAINER(expander),set_child_colour,GINT_TO_POINTER(TRUE));

  lives_box_pack_start (parent, expander, FALSE, FALSE, widget_opts.packing_height);

  lives_container_add (LIVES_CONTAINER (expander), child);
#endif

  return expander;
}




LIVES_INLINE LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir) {
  LiVESWidget *fbutton=NULL;
#ifdef GUI_GTK
  LiVESWidget *image = lives_image_new_from_stock ("gtk-open", LIVES_ICON_SIZE_BUTTON);
  fbutton = lives_button_new ();
  g_object_set_data(G_OBJECT(fbutton),"is_dir",GINT_TO_POINTER(is_dir));
  if (def_dir!=NULL) g_object_set_data(G_OBJECT(fbutton),"def_dir",(livespointer)def_dir);
  lives_container_add (LIVES_CONTAINER (fbutton), image);
#endif
  return fbutton;
}


// utils

void widget_helper_init(void) {
  widget_opts = def_widget_opts;
  widget_opts.border_width*=widget_opts.scale;
  widget_opts.packing_width*=widget_opts.scale;
  widget_opts.packing_height*=widget_opts.scale;
  widget_opts.filler_len*=widget_opts.scale;

#ifdef GUI_GTK
  gtk_accel_map_add_entry("<LiVES>/save",LIVES_KEY_s,LIVES_CONTROL_MASK);
  gtk_accel_map_add_entry("<LiVES>/quit",LIVES_KEY_q,LIVES_CONTROL_MASK);
#endif
}


boolean lives_has_icon(const char *stock_id, LiVESIconSize size)  {
  boolean has_icon=FALSE;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  GtkIconInfo *iset=gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), stock_id, size, GTK_ICON_LOOKUP_USE_BUILTIN);
#else
  GtkIconSet *iset=gtk_icon_factory_lookup_default(stock_id);
#endif
  has_icon=(iset!=NULL);
#endif
  return has_icon;
}




LIVES_INLINE void lives_cursor_unref(LiVESXCursor *cursor) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  g_object_unref(GTK_OBJECT(cursor));
#else
  gdk_cursor_unref(cursor);
#endif
#endif
}


void lives_widget_apply_theme(LiVESWidget *widget, LiVESWidgetState state) {
  lives_widget_set_fg_color(widget, state, &palette->normal_fore);
  lives_widget_set_bg_color(widget, state, &palette->normal_back);
}


void lives_widget_apply_theme2(LiVESWidget *widget, LiVESWidgetState state) {
  //lives_widget_set_fg_color(widget, state, &palette->normal_fore);
  lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
}



boolean lives_entry_set_completion_from_list(LiVESEntry *entry, LiVESList *xlist) {
#ifdef GUI_GTK
  GtkListStore *store;
  LiVESEntryCompletion *completion;
  store = gtk_list_store_new (1, G_TYPE_STRING);

  while (xlist != NULL) {
    LiVESTreeIter iter;
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0, (gchar *)xlist->data, -1);
    xlist=xlist->next;
  }
    
  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (completion, (GtkTreeModel *)store);
  gtk_entry_completion_set_text_column (completion, 0);
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_completion_set_popup_set_width (completion, TRUE);
  gtk_entry_completion_set_popup_completion (completion, TRUE);
  gtk_entry_completion_set_popup_single_match(completion,FALSE);
  gtk_entry_set_completion (entry, completion);
  return TRUE;
#endif

}


void lives_window_center(LiVESWindow *window) {
  if (prefs->show_gui) {
    if (prefs->gui_monitor>0) {
      int xcen,ycen;
      lives_window_set_screen(LIVES_WINDOW(window),mainw->mgeom[prefs->gui_monitor-1].screen);

      xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-
						 lives_widget_get_allocation_width(LIVES_WIDGET(window)))/2;
      ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-
						 lives_widget_get_allocation_height(LIVES_WIDGET(window)))/2;
      lives_window_move(LIVES_WINDOW(window),xcen,ycen);
    }
    else lives_window_set_position(LIVES_WINDOW(window),LIVES_WIN_POS_CENTER_ALWAYS);
  }
}



void lives_widget_get_bg_color(LiVESWidget *widget, LiVESWidgetColor *color) {
  lives_widget_get_bg_state_color(widget,LIVES_WIDGET_STATE_NORMAL,color);
}

void lives_widget_get_fg_color(LiVESWidget *widget, LiVESWidgetColor *color) {
  lives_widget_get_fg_state_color(widget,LIVES_WIDGET_STATE_NORMAL,color);
}

void lives_widget_unparent(LiVESWidget *widget) {
  lives_container_remove(LIVES_CONTAINER(lives_widget_get_parent(widget)),widget);
}

boolean label_act_toggle (LiVESWidget *widget, LiVESXEventButton *event, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active (togglebutton, !lives_toggle_button_get_active(togglebutton));
  return FALSE;
}

boolean widget_act_toggle (LiVESWidget *widget, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active (togglebutton, TRUE);
  return FALSE;
}

LIVES_INLINE void toggle_button_toggle (LiVESToggleButton *tbutton) {
  if (lives_toggle_button_get_active(tbutton)) lives_toggle_button_set_active(tbutton,FALSE);
  else lives_toggle_button_set_active(tbutton,FALSE);
}


void set_child_colour(LiVESWidget *widget, livespointer set_allx) {
  boolean set_all=LIVES_POINTER_TO_INT(set_allx);

  if (!set_all&&LIVES_IS_BUTTON(widget)) return;
  if (LIVES_IS_CONTAINER(widget)) {
    gtk_container_forall(LIVES_CONTAINER(widget),set_child_colour,set_allx);
    return;
  }

  if (set_all||LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_NORMAL);
  }
}


void set_button_width(LiVESWidget *buttonbox, LiVESWidget *button, int width) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
  gtk_button_box_set_child_size (GTK_BUTTON_BOX(buttonbox), width, -1);
#else
  lives_widget_set_size_request(button,width*4,-1);
#endif
  gtk_button_box_set_layout (GTK_BUTTON_BOX(buttonbox), GTK_BUTTONBOX_SPREAD);
#endif
}


char *lives_text_view_get_text(LiVESTextView *textview) {
  LiVESTextIter siter,eiter;
  LiVESTextBuffer *textbuf=lives_text_view_get_buffer (textview);
  lives_text_buffer_get_start_iter(textbuf,&siter);
  lives_text_buffer_get_end_iter(textbuf,&eiter);

  return lives_text_buffer_get_text(textbuf,&siter,&eiter,FALSE);
}


boolean lives_text_view_set_text(LiVESTextView *textview, const gchar *text, int len) {
  LiVESTextBuffer *textbuf=lives_text_view_get_buffer (textview);
  if (textbuf!=NULL)
    return lives_text_buffer_set_text(textbuf,text,len);
  return FALSE;
}


 
boolean lives_text_buffer_insert_at_end(LiVESTextBuffer *tbuff, const char *text) {
 LiVESTextIter xiter;
 if (lives_text_buffer_get_end_iter(tbuff,&xiter))
   return lives_text_buffer_insert(tbuff,&xiter,text,-1);
 return FALSE;
}



boolean lives_text_view_scroll_onscreen(LiVESTextView *tview) {
  LiVESTextIter iter;
  LiVESTextMark *mark;
  LiVESTextBuffer *tbuf;

  tbuf=lives_text_view_get_buffer (tview);
  if (tbuf!=NULL) {
    lives_text_buffer_get_end_iter(tbuf,&iter);
    mark=lives_text_buffer_create_mark(tbuf,NULL,&iter,FALSE);
    if (mark!=NULL) {
      if (lives_text_view_scroll_mark_onscreen(tview,mark)) 
	return lives_text_buffer_delete_mark (tbuf,mark);
    }
  }
  return FALSE;
}




int get_box_child_index (LiVESBox *box, LiVESWidget *tchild) {
#ifdef GUI_GTK
  GList *list=gtk_container_get_children(LIVES_CONTAINER(box));
  LiVESWidget *child;
  register int i=0;

  while (list!=NULL) {
    child=(LiVESWidget *)list->data;
    if (child==tchild) return i;
    list=list->next;
    i++;
  }
  #endif
  return -1;
}


void lives_spin_button_configure(LiVESSpinButton *spinbutton,
				 double value,
				 double lower,
				 double upper,
				 double step_increment,
				 double page_increment) {
#ifdef GUI_GTK
  GtkAdjustment *adjustment=gtk_spin_button_get_adjustment(spinbutton);
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_configure(adjustment,value,lower,upper,step_increment,page_increment,0.);
#else
  g_object_freeze_notify (G_OBJECT(adjustment));
  adjustment->upper=upper;
  adjustment->lower=lower;
  adjustment->value=value;
  adjustment->step_increment=step_increment;
  adjustment->page_increment=page_increment;
  g_object_thaw_notify (G_OBJECT(adjustment));
#endif
#endif

}



///// lives specific functions

#include "rte_window.h"
#include "ce_thumbs.h"

static void lives_widget_context_update_real(boolean all) {
#ifdef GUI_GTK
  boolean mt_needs_idlefunc=FALSE;

  if (pthread_mutex_trylock(&mainw->gtk_mutex)) return;

  if (mainw->multitrack!=NULL&&mainw->multitrack->idlefunc>0) {
    g_source_remove(mainw->multitrack->idlefunc);
    mainw->multitrack->idlefunc=0;
    mt_needs_idlefunc=TRUE;
  }

  if (!mainw->is_exiting) {
    if (rte_window!=NULL) ret_set_key_check_state();
    if (mainw->ce_thumbs) {
      ce_thumbs_set_key_check_state();
      ce_thumbs_apply_liberation();
      if (mainw->ce_upd_clip) {
	ce_thumbs_highlight_current_clip();
	mainw->ce_upd_clip=FALSE;
      }
    }
    if (all) while (!mainw->is_exiting&&g_main_context_iteration(NULL,FALSE));
    else g_main_context_iteration(NULL,FALSE);

  }

  if (!mainw->is_exiting&&mt_needs_idlefunc) mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);

  pthread_mutex_unlock(&mainw->gtk_mutex);

#endif
}

void lives_widget_context_update(void) {
  lives_widget_context_update_real(TRUE);
}


void lives_widget_context_update_one(void) {
  lives_widget_context_update_real(FALSE);
}



LiVESWidget *lives_menu_add_separator(LiVESMenu *menu) {
  LiVESWidget *separatormenuitem = lives_menu_item_new ();
  if (separatormenuitem!=NULL) {
    lives_container_add (LIVES_CONTAINER (menu), separatormenuitem);
    lives_widget_set_sensitive (separatormenuitem, FALSE);
  }
  return separatormenuitem;
}


LIVES_INLINE int lives_display_get_n_screens(LiVESXDisplay *disp) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  return 1;
#else
  return gdk_display_get_n_screens(disp);
#endif
#endif
  return 1;
}


void lives_set_cursor_style(lives_cursor_t cstyle, LiVESWidget *widget) {
#ifdef GUI_GTK
  LiVESXWindow *window;
  GdkCursor *cursor=NULL;
  GdkDisplay *disp;
  GdkCursorType ctype=GDK_X_CURSOR;

  if (widget==NULL) {
    if (mainw->multitrack==NULL&&mainw->is_ready) {
      if (cstyle!=LIVES_CURSOR_NORMAL&&mainw->cursor_style==cstyle) return;
      window=lives_widget_get_xwindow(mainw->LiVES);
    }
    else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) {
      if (cstyle!=LIVES_CURSOR_NORMAL&&mainw->multitrack->cursor_style==cstyle) return;
      window=lives_widget_get_xwindow(mainw->multitrack->window);
    }
    else return;
  }
  else window=lives_widget_get_xwindow(widget);

  if (!GDK_IS_WINDOW(window)) return;

  switch(cstyle) {
  case LIVES_CURSOR_NORMAL:
    break;
  case LIVES_CURSOR_BUSY:
    ctype=GDK_WATCH;
    break;
  case LIVES_CURSOR_CENTER_PTR:
    ctype=GDK_CENTER_PTR;
    break;
  case LIVES_CURSOR_HAND2:
    ctype=GDK_HAND2;
    break;
  case LIVES_CURSOR_SB_H_DOUBLE_ARROW:
    ctype=GDK_SB_H_DOUBLE_ARROW;
    break;
  case LIVES_CURSOR_CROSSHAIR:
    ctype=GDK_CROSSHAIR;
    break;
  case LIVES_CURSOR_TOP_LEFT_CORNER:
    ctype=GDK_TOP_LEFT_CORNER;
    break;
  case LIVES_CURSOR_BOTTOM_RIGHT_CORNER:
    ctype=GDK_BOTTOM_RIGHT_CORNER;
    break;
  default:
    return;
  }
  if (widget==NULL) {
    if (mainw->multitrack!=NULL) mainw->multitrack->cursor_style=cstyle;
    else mainw->cursor_style=cstyle;
  }
#if GTK_CHECK_VERSION(2,22,0)
  cursor=gdk_window_get_cursor(window);
  if (cursor!=NULL&&gdk_cursor_get_cursor_type(cursor)==ctype) return;
  cursor=NULL;
#endif
  disp=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].disp;
  if (cstyle!=LIVES_CURSOR_NORMAL) {
    cursor=gdk_cursor_new_for_display(disp,ctype);
    gdk_window_set_cursor (window, cursor);
  }
  else gdk_window_set_cursor(window,NULL);
  if (cursor!=NULL) lives_cursor_unref(cursor);
#endif
}




void hide_cursor(LiVESXWindow *window) {
  //make the cursor invisible in playback windows
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,16,0)
  if (GDK_IS_WINDOW(window)) {
    GdkCursor *cursor=gdk_cursor_new(GDK_BLANK_CURSOR);
    gdk_window_set_cursor(window,cursor);
    lives_cursor_unref(cursor);
  }
#else
  static GdkCursor *hidden_cursor=NULL;

  char cursor_bits[] = {0x00};
  char cursormask_bits[] = {0x00};
  GdkPixmap *source, *mask;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0, 0, 0 };

  if (hidden_cursor==NULL) {
    source = gdk_bitmap_create_from_data (NULL, cursor_bits,
					  1, 1);
    mask = gdk_bitmap_create_from_data (NULL, cursormask_bits,
					1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap (source, mask, &fg, &bg, 0, 0);
    g_object_unref (source);
    g_object_unref (mask);
  }
  if (GDK_IS_WINDOW(window)) gdk_window_set_cursor (window, hidden_cursor);
#endif 
#endif
}


void unhide_cursor(LiVESXWindow *window) {
  if (LIVES_IS_XWINDOW(window)) lives_xwindow_set_cursor(window,NULL);
}


void get_border_size (LiVESWidget *win, int *bx, int *by) {
#ifdef GUI_GTK
  GdkRectangle rect;
  gint wx,wy;
  gdk_window_get_frame_extents (lives_widget_get_xwindow (win),&rect);
  gdk_window_get_origin (lives_widget_get_xwindow (win), &wx, &wy);
  *bx=wx-rect.x;
  *by=wy-rect.y;
#endif
}




/*
 * Set active string to the combo box
 */
void lives_combo_set_active_string(LiVESCombo *combo, const char *active_str) {
  lives_entry_set_text(LIVES_ENTRY(lives_bin_get_child(LIVES_BIN(combo))),active_str);
}

LiVESWidget *lives_combo_get_entry(LiVESCombo *widget) {
  return lives_bin_get_child(LIVES_BIN(widget));
}


boolean lives_widget_set_can_focus_and_default(LiVESWidget *widget) {
  if (!lives_widget_set_can_focus(widget,TRUE)) return FALSE;
  return lives_widget_set_can_default(widget,TRUE);
}



void lives_general_button_clicked (LiVESButton *button, livespointer data_to_free) {
  // destroy the button top-level and free data
  lives_widget_destroy(lives_widget_get_toplevel(LIVES_WIDGET(button)));
  lives_widget_context_update();

  if (data_to_free!=NULL) g_free(data_to_free);
}


LiVESWidget *add_hsep_to_box (LiVESBox *box) {
  LiVESWidget *hseparator = lives_hseparator_new ();
  lives_box_pack_start (box, hseparator, FALSE, FALSE, widget_opts.packing_width>>1);
  if (!widget_opts.no_gui) 
    lives_widget_show(hseparator);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(hseparator, LIVES_WIDGET_STATE_NORMAL);
  }
  return hseparator;
}


LiVESWidget *add_vsep_to_box (LiVESBox *box) {
  LiVESWidget *vseparator = lives_vseparator_new ();
  lives_box_pack_start (box, vseparator, FALSE, FALSE, widget_opts.packing_height>>1);
  if (!widget_opts.no_gui) 
    lives_widget_show(vseparator);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(vseparator, LIVES_WIDGET_STATE_NORMAL);
  }
  return vseparator;
}

#ifdef GUI_GTK
static gchar spaces[W_MAX_FILLER_LEN+1];
static boolean spaces_inited=FALSE;
#endif

LiVESWidget *add_fill_to_box (LiVESBox *box) {
  LiVESWidget *widget=NULL;
#ifdef GUI_GTK
  LiVESWidget *blank_label;
  static gint old_spaces=-1;
  static gchar *xspaces=NULL;

  if (!spaces_inited) {
    register int i;
    for (i=0;i<W_MAX_FILLER_LEN;i++) {
      g_snprintf(spaces+i,1," ");
    }
  }

  if (widget_opts.filler_len>W_MAX_FILLER_LEN||widget_opts.filler_len<0) return NULL;

  if (widget_opts.filler_len!=old_spaces) {
    if (xspaces!=NULL) g_free(xspaces);
    xspaces=g_strndup(spaces,widget_opts.filler_len);
    old_spaces=widget_opts.filler_len;
  }

  blank_label = lives_standard_label_new (xspaces);

  lives_box_pack_start (box, blank_label, TRUE, TRUE, 0);
  lives_widget_set_hexpand(blank_label,TRUE);
  if (!widget_opts.no_gui) 
    lives_widget_show(blank_label);
  widget=blank_label;

#endif
  return widget;
}





