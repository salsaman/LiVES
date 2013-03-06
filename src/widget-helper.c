// widget-helper.c
// LiVES
// (c) G. Finch 2012 - 2013 <salsaman@gmail.com>
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


LIVES_INLINE lives_painter_surface_t *lives_painter_surface_create_from_widget(LiVESWidget *widget, lives_painter_format_t format, 
									       int width, int height) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
#if GTK_CHECK_VERSION(3,0,0)
  LiVESXWindow *window=lives_widget_get_xwindow(widget);
  if (window!=NULL) {
  surf=gdk_window_create_similar_surface(window,format,width,height);
}
#endif
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


boolean return_true (LiVESWidget *widget, LiVESXEvent *event, LiVESObjectPtr user_data) {
  // event callback that just returns TRUE
  return TRUE;
}



LIVES_INLINE void lives_object_unref(LiVESObjectPtr object) {
#ifdef GUI_GTK
  g_object_unref(object);
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
  gtk_style_context_get_color (gtk_widget_get_style_context (widget), GTK_STATE_NORMAL, color);
#else
  lives_widget_color_copy(color,&gtk_widget_get_style(widget)->fg[GTK_STATE_NORMAL]);
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
  return gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
  return GTK_DIALOG(dialog)->vbox;
#endif

#endif
}

LIVES_INLINE LiVESWidget *lives_dialog_get_action_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_action_area(GTK_DIALOG(dialog));
#else
  return GTK_DIALOG(dialog)->vbox;
#endif

#endif
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
						      gpointer destroy_fn_data) {

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
  return gdk_pixbuf_new_from_file_at_scale(filename, width, height, preserve_aspect_ratio, error);
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


LIVES_INLINE LiVESWidget *lives_hscale_new_with_range(double min, double max, double step) {
  LiVESWidget *hscale=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hscale=gtk_scale_new_with_range(LIVES_ORIENTATION_HORIZONTAL,min,max,step);
#else
  hscale=gtk_hscale_new_with_range(min,max,step);
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


LIVES_INLINE void lives_combo_append_text(LiVESCombo *combo, const char *text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),text);
#else
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),text);
#endif
#endif
}

#ifdef GUI_GTK
static void lives_combo_remove_all_text(LiVESCombo *combo) {
#if GTK_CHECK_VERSION(3,0,0)
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
#else
  int count = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(combo),
					     NULL);
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



LIVES_INLINE boolean lives_toggle_button_get_active(LiVESToggleButton *button) {
#ifdef GUI_GTK
  return gtk_toggle_button_get_active(button);
#endif
}


LIVES_INLINE void lives_toggle_button_set_active(LiVESToggleButton *button, boolean active) {
#ifdef GUI_GTK
  gtk_toggle_button_set_active(button,active);
#endif
}


LIVES_INLINE void lives_tooltips_set(LiVESWidget *widget, const char *tip_text) {
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


LIVES_INLINE LiVESSList *lives_radio_button_get_group(LiVESRadioButton *rbutton) {
#ifdef GUI_GTK
  return gtk_radio_button_get_group(rbutton);
#endif
}


LIVES_INLINE LiVESWidget *lives_widget_get_parent(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_parent(widget);
#endif
}


LIVES_INLINE LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  return gtk_widget_get_window(widget);
#else
  return GDK_WINDOW(widget->window);
#endif
#endif
}

LIVES_INLINE void lives_widget_set_can_focus(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_focus(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
#endif
#endif
}


LIVES_INLINE void lives_widget_set_can_default(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_default(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_DEFAULT);
#endif
#endif
}


LIVES_INLINE boolean lives_widget_is_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_is_sensitive(widget);
#else
  return GTK_WIDGET_IS_SENSITIVE (widget);
#endif
#endif
}


LIVES_INLINE boolean lives_widget_is_visible(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_visible(widget);
#else
  return GTK_WIDGET_VISIBLE (widget);
#endif
#endif
}


LIVES_INLINE boolean lives_widget_is_realized(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_realized(widget);
#else
  return GTK_WIDGET_REALIZED (widget);
#endif
#endif
}


LIVES_INLINE void lives_container_remove(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_remove(container,widget);
#endif
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


LIVES_INLINE const char *lives_label_get_text(LiVESLabel *label) {
#ifdef GUI_GTK
  return gtk_label_get_text(label);
#endif
  return NULL;
}


LIVES_INLINE void lives_entry_set_editable(LiVESEntry *entry, boolean editable) {
#ifdef GUI_GTK
  gtk_editable_set_editable(GTK_EDITABLE(entry),editable);
#endif
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


LIVES_INLINE boolean lives_image_menu_item_set_always_show_image(LiVESImageMenuItem *item, boolean show) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,16,0)
  gtk_image_menu_item_set_always_show_image(item,show);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_file_chooser_set_do_overwrite_confirmation(LiVESFileChooser *chooser, boolean set) {
  // return TRUE if implemented
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,8,0)
  gtk_file_chooser_set_do_overwrite_confirmation(chooser,set);
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




void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  gchar *text=gtk_widget_get_tooltip_text(source);
  gtk_widget_set_tooltip_text(dest,text);
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
#ifdef GUI_GTK

  gtk_combo_box_set_active(combo,-1);

  lives_combo_remove_all_text(combo);

  // add the new list
  while (list!=NULL) {
    lives_combo_append_text(LIVES_COMBO(combo),(const char *)list->data);
    list=list->next;
  }
#endif
}



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
#ifdef GUI_GTK

  label=gtk_label_new(text);

  if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    lives_widget_set_fg_color(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), widget_opts.justify);

  gtk_label_set_line_wrap (GTK_LABEL (label), widget_opts.line_wrap);
#endif

  return label;
}


LiVESWidget *lives_standard_label_new_with_mnemonic(const char *text, LiVESWidget *mnemonic_widget) {
  LiVESWidget *label=NULL;
#ifdef GUI_GTK

  label=gtk_label_new_with_mnemonic(text);

  if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    lives_widget_set_fg_color(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), widget_opts.justify);
  gtk_label_set_line_wrap (GTK_LABEL (label), widget_opts.line_wrap);

  if (mnemonic_widget!=NULL) gtk_label_set_mnemonic_widget (GTK_LABEL(label),mnemonic_widget);
#endif

  return label;
}


LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, 
					     const char *tooltip) {
  LiVESWidget *checkbutton=NULL;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label;
  LiVESWidget *hbox;

  checkbutton = gtk_check_button_new ();
  if (tooltip!=NULL) lives_tooltips_set(checkbutton, tooltip);

  if (labeltext!=NULL) {
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,checkbutton);

    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,checkbutton);
    }
    else label=lives_standard_label_new (labeltext);

    gtk_container_add(GTK_CONTAINER(eventbox),label);

    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      checkbutton);
  
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      lives_widget_set_fg_color(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }
  
  if (box!=NULL) {
    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_width);
    }
    
    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);
    
    
    if (widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    
    gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, widget_opts.packing_width);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

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
  LiVESWidget *label;
  LiVESWidget *hbox;

  radiobutton = gtk_radio_button_new (rbgroup);

  if (tooltip!=NULL) lives_tooltips_set(radiobutton, tooltip);

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,radiobutton);
    }
    else label=lives_standard_label_new (labeltext);
    
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,radiobutton);
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton);
    
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      lives_widget_set_fg_color(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }


  if (box!=NULL) {
    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    else {
      hbox = lives_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_width);
    }
    
    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);
    
    if (widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

    gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, widget_opts.packing_width);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

#endif

  return radiobutton;
}


int calc_spin_button_width(double min, double max, int dp) {
  char *txt=g_strdup_printf ("%d",(int)max);
  int maxlen=strlen (txt);
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
  LiVESWidget *label;
  LiVESWidget *hbox;
  LiVESAdjustment *adj;

  int maxlen;

  adj = lives_adjustment_new (val, min, max, step, page, 0.);
  spinbutton = gtk_spin_button_new (adj, 1, dp);
  if (tooltip!=NULL) lives_tooltips_set(spinbutton, tooltip);

  maxlen=calc_spin_button_width(min,max,dp);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton),maxlen);

  lives_widget_set_can_focus_and_default(spinbutton);
  gtk_entry_set_activates_default (GTK_ENTRY (spinbutton), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),GTK_UPDATE_ALWAYS);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton),TRUE);

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,spinbutton);
    }
    else label=lives_standard_label_new (labeltext);
    
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,spinbutton);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      lives_widget_set_fg_color(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }

  if (box!=NULL) {
    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    else {
      hbox = lives_hbox_new (TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_width);
    }
    
    if (!widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    
    gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, widget_opts.packing_width);
    
    if (widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

#endif

  return spinbutton;
}






LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip) {
  LiVESWidget *combo=NULL;

  // pack a themed combo box into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label;
  LiVESWidget *hbox;
  LiVESEntry *entry;

  combo=lives_combo_new();
  if (tooltip!=NULL) lives_tooltips_set(combo, tooltip);

  entry=(LiVESEntry *)lives_combo_get_entry(LIVES_COMBO(combo));

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic (labeltext,LIVES_WIDGET(entry));
    }
    else label = lives_standard_label_new (labeltext);
    
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,combo);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      lives_widget_set_fg_color(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }

  if (box!=NULL) {
    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    else {
      hbox = lives_hbox_new (TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_width);
    }
    
    if (!widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, widget_opts.packing_width);
    if (widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

  gtk_editable_set_editable (GTK_EDITABLE(entry),FALSE);
  gtk_entry_set_activates_default(entry,TRUE);

  lives_combo_populate(LIVES_COMBO(combo),list);

  if (list!=NULL) gtk_combo_box_set_active(LIVES_COMBO(combo),0);

#endif

  return combo;
}


LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, const char *txt, int dispwidth, int maxchars, LiVESBox *box, 
					     const char *tooltip) {

  LiVESWidget *entry=NULL;

#ifdef GUI_GTK
  LiVESWidget *label=NULL;

  LiVESWidget *hbox=NULL;

  entry=gtk_entry_new();

  if (tooltip!=NULL) lives_tooltips_set(entry, tooltip);

  if (txt!=NULL)
    gtk_entry_set_text (GTK_ENTRY (entry),txt);

  if (dispwidth!=-1) gtk_entry_set_width_chars (GTK_ENTRY (entry),dispwidth);
  if (maxchars!=-1) gtk_entry_set_max_length(GTK_ENTRY (entry),maxchars);

  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic (labeltext,entry);
    }
    else label = lives_standard_label_new (labeltext);

    if (tooltip!=NULL) lives_tooltips_copy(label,entry);
  }


  if (box!=NULL) {
    if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
    
    else {
      hbox = lives_hbox_new (TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, widget_opts.packing_width);
    }
    
    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (!widget_opts.swap_label&&label!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
    
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, widget_opts.packing_width);
    
    if (widget_opts.swap_label&&label!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width);
  }

#endif

  return entry;
}



LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons) {
  LiVESWidget *dialog=NULL;

#ifdef GUI_GTK

  dialog = gtk_dialog_new ();

  if (title!=NULL)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  gtk_window_set_deletable(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  lives_widget_set_hexpand(dialog,TRUE);
  lives_widget_set_vexpand(dialog,TRUE);

  if (prefs->gui_monitor!=0) {
    gtk_window_set_screen(GTK_WINDOW(dialog),mainw->mgeom[prefs->gui_monitor-1].screen);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), widget_opts.border_width*2);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);

  if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    lives_widget_set_bg_color(dialog, GTK_STATE_NORMAL, &palette->normal_back);

#if !GTK_CHECK_VERSION(3,0,0)
    lives_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
#endif
  }

  if (add_std_buttons) {
    GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());
    GtkWidget *cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    GtkWidget *okbutton = gtk_button_new_from_stock ("gtk-ok");

    gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

    gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
				LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

    lives_widget_set_can_focus_and_default(cancelbutton);

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);

    lives_widget_set_can_focus_and_default(okbutton);
    gtk_widget_grab_default (okbutton);
  }

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  // must do this before setting modal !
  if (!widget_opts.no_gui) 
    gtk_widget_show(dialog);

  if (!widget_opts.non_modal)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);


#endif

  return dialog;

}



LiVESWidget *lives_standard_hruler_new(void) {
  LiVESWidget *hruler=NULL;

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  hruler=gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,NULL);
  gtk_scale_set_draw_value(GTK_SCALE(hruler),FALSE);
  gtk_scale_set_has_origin(GTK_SCALE(hruler),FALSE);
  gtk_scale_set_digits(GTK_SCALE(hruler),8);
#else
  hruler=gtk_hruler_new();
#endif

#endif

  return hruler;
}



// utils
LIVES_INLINE void lives_cursor_unref(LiVESXCursor *cursor) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  g_object_unref(G_OBJECT(cursor));
#else
  gdk_cursor_unref(cursor);
#endif
#endif
}


void lives_widget_get_bg_color(LiVESWidget *widget, LiVESWidgetColor *color) {
#ifdef GUI_GTK
  lives_widget_get_bg_state_color(widget,GTK_STATE_NORMAL,color);
#endif
}

void lives_widget_get_fg_color(LiVESWidget *widget, LiVESWidgetColor *color) {
#ifdef GUI_GTK
  lives_widget_get_fg_state_color(widget,GTK_STATE_NORMAL,color);
#endif
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


char *text_view_get_text(LiVESTextView *textview) {
  GtkTextIter siter,eiter;
  GtkTextBuffer *textbuf=gtk_text_view_get_buffer (textview);
  gtk_text_buffer_get_start_iter(textbuf,&siter);
  gtk_text_buffer_get_end_iter(textbuf,&eiter);

  return gtk_text_buffer_get_text(textbuf,&siter,&eiter,TRUE);
}


void text_view_set_text(LiVESTextView *textview, const gchar *text) {
  GtkTextBuffer *textbuf=gtk_text_view_get_buffer (textview);
  gtk_text_buffer_set_text(textbuf,text,-1);
}


int get_box_child_index (LiVESBox *box, LiVESWidget *tchild) {
  GList *list=gtk_container_get_children(GTK_CONTAINER(box));
  GtkWidget *child;
  int i=0;

  while (list!=NULL) {
    child=(GtkWidget *)list->data;
    if (child==tchild) return i;
    list=list->next;
    i++;
  }
  return -1;
}


void adjustment_configure(LiVESAdjustment *adjustment,
		     double value,
		     double lower,
		     double upper,
		     double step_increment,
		     double page_increment,
		     double page_size) {
  g_object_freeze_notify (G_OBJECT(adjustment));
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_configure(adjustment,value,lower,upper,step_increment,page_increment,page_size);
#else
  adjustment->upper=upper;
  adjustment->lower=lower;
  adjustment->value=value;
  adjustment->step_increment=step_increment;
  adjustment->page_increment=page_increment;
  adjustment->page_size=page_size;
#endif
#endif

  g_object_thaw_notify (G_OBJECT(adjustment));
}



void lives_set_cursor_style(lives_cursor_t cstyle, LiVESWidget *widget) {
  LiVESXWindow *window;
  if (mainw->cursor!=NULL) lives_cursor_unref(mainw->cursor);
  mainw->cursor=NULL;

  if (widget==NULL) {
    if (mainw->multitrack==NULL&&mainw->is_ready) window=lives_widget_get_xwindow(mainw->LiVES);
    else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) window=lives_widget_get_xwindow(mainw->multitrack->window);
    else return;
  }
  else window=lives_widget_get_xwindow(widget);

  switch(cstyle) {
  case LIVES_CURSOR_NORMAL:
    if (GDK_IS_WINDOW(window))
      gdk_window_set_cursor (window, NULL);
    return;
  case LIVES_CURSOR_BUSY:
    mainw->cursor=gdk_cursor_new(GDK_WATCH);
    if (GDK_IS_WINDOW(window))
      gdk_window_set_cursor (window, mainw->cursor);
    return;
  default:
    return;
  }
}




void hide_cursor(LiVESXWindow *window) {
  //make the cursor invisible in playback windows

#if GTK_CHECK_VERSION(3,0,0)
lives_painter_surface_t *s;
GdkPixbuf *pixbuf;

 if (hidden_cursor==NULL) {
   s = lives_painter_image_surface_create (LIVES_PAINTER_FORMAT_A1, 1, 1);
   pixbuf = gdk_pixbuf_get_from_surface (s,
					 0, 0,
					 1, 1);
   
   lives_painter_surface_destroy (s);
   
   hidden_cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default(), pixbuf, 0, 0);
   
   g_object_unref (pixbuf);
 }
#else
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
#endif 
  if (GDK_IS_WINDOW(window)) gdk_window_set_cursor (window, hidden_cursor);
}


void unhide_cursor(LiVESXWindow *window) {
  if (GDK_IS_WINDOW(window)) gdk_window_set_cursor(window,NULL);
}


void get_border_size (LiVESWidget *win, int *bx, int *by) {
  GdkRectangle rect;
  gint wx,wy;
  gdk_window_get_frame_extents (lives_widget_get_xwindow (win),&rect);
  gdk_window_get_origin (lives_widget_get_xwindow (win), &wx, &wy);
  *bx=wx-rect.x;
  *by=wy-rect.y;
}




/*
 * Set active string to the combo box
 */
void lives_combo_set_active_string(LiVESCombo *combo, const char *active_str) {

#ifdef GUI_GTK
  gtk_entry_set_text(GTK_ENTRY(lives_bin_get_child(LIVES_BIN(combo))),active_str);
#endif

}

LiVESWidget *lives_combo_get_entry(LiVESCombo *widget) {
  return lives_bin_get_child(LIVES_BIN(widget));
}


void lives_widget_set_can_focus_and_default(LiVESWidget *widget) {
  lives_widget_set_can_focus(widget,TRUE);
  lives_widget_set_can_default(widget,TRUE);
}



void lives_general_button_clicked (LiVESButton *button, LiVESObjectPtr data_to_free) {
  // destroy the button top-level and free data

#ifdef GUI_GTK
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));

  if (data_to_free!=NULL) g_free(data_to_free);
#endif
}


boolean lives_general_delete_event(LiVESWidget *widget, LiVESXEvent *event, LiVESObjectPtr data_to_free) {
#ifdef GUI_GTK
  gtk_widget_destroy(widget);

  if (data_to_free!=NULL) g_free(data_to_free);
#endif

  return TRUE;
}

void add_hsep_to_box (LiVESBox *box, boolean expand) {
#ifdef GUI_GTK
  GtkWidget *hseparator = lives_hseparator_new ();
  gtk_box_pack_start (box, hseparator, expand, TRUE, 0);
  if (!widget_opts.no_gui) 
    gtk_widget_show(hseparator);
#endif
}

void add_fill_to_box (LiVESBox *box) {
#ifdef GUI_GTK
  GtkWidget *blank_label = gtk_label_new ("");
  gtk_box_pack_start (box, blank_label, TRUE, TRUE, 0);
  lives_widget_set_hexpand(blank_label,FALSE);
  lives_widget_set_vexpand(blank_label,FALSE);
  if (!widget_opts.no_gui) 
    gtk_widget_show(blank_label);
#endif
}




