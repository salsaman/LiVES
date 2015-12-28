// widget-helper.c
// LiVES
// (c) G. Finch 2012 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#include "main.h"

// The idea here is to replace toolkit specific functions with generic ones

// TODO - replace as much code in the other files with these functions as possible

// TODO - add for other toolkits, e.g. qt

// basic functions

////////////////////////////////////////////////////
//lives_painter functions

LIVES_INLINE lives_painter_t *lives_painter_create(lives_painter_surface_t *target) {
  lives_painter_t *cr=NULL;
#ifdef PAINTER_CAIRO
  cr=cairo_create(target);
#endif
#ifdef PAINTER_QPAINTER
  cr = new lives_painter_t(target);
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
#ifdef PAINTER_QPAINTER
  QWidget *widg = static_cast<QWidget *>(widget);
  if (widg!=NULL) cr = new lives_painter_t(widg);
#endif
  return cr;
}


LIVES_INLINE boolean lives_painter_set_source_pixbuf(lives_painter_t *cr, const LiVESPixbuf *pixbuf, double pixbuf_x, double pixbuf_y) {
  // blit pixbuf to cairo at x,y
#ifdef PAINTER_CAIRO
  gdk_cairo_set_source_pixbuf(cr,pixbuf,pixbuf_x,pixbuf_y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QPointF qp(pixbuf_x,pixbuf_y);
  const QImage *qi = (const QImage *)pixbuf;
  cr->drawImage(qp, *qi);
  return TRUE;
#endif
  return FALSE;

}


LIVES_INLINE boolean lives_painter_set_source_surface(lives_painter_t *cr, lives_painter_surface_t *surface, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_set_source_surface(cr,surface,x,y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QPointF qp(x,y);
  cr->drawImage(qp,*surface);
  return TRUE;
#endif
  return FALSE;


}

LIVES_INLINE boolean lives_painter_paint(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_paint(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_fill(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_fill(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->fillPath(*(cr->p),cr->pen.color());
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}

LIVES_INLINE boolean lives_painter_stroke(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_stroke(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->strokePath(*(cr->p),cr->pen);
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}

LIVES_INLINE boolean lives_painter_clip(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_clip(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->setClipPath(*(cr->p),Qt::IntersectClip);
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}

LIVES_INLINE boolean lives_painter_destroy(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_destroy(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->end();
  delete cr;
  return TRUE;
#endif
  return FALSE;
}






LIVES_INLINE boolean lives_painter_render_background(LiVESWidget *widget, lives_painter_t *cr, double x, double y, double width,
    double height) {
#ifdef PAINTER_CAIRO
#if GTK_CHECK_VERSION(3,0,0)
  GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
  gtk_render_background(ctx,cr,x,y,width,height);
#else
  LiVESWidgetColor color;
  lives_widget_color_copy(&color,&gtk_widget_get_style(widget)->bg[lives_widget_get_state(widget)]);

#if LIVES_WIDGET_COLOR_HAS_ALPHA
  lives_painter_set_source_rgba(cr,
                                LIVES_WIDGET_COLOR_SCALE(color.red),
                                LIVES_WIDGET_COLOR_SCALE(color.green),
                                LIVES_WIDGET_COLOR_SCALE(color.blue),
                                LIVES_WIDGET_COLOR_SCALE(color.alpha));
#else
  lives_painter_set_source_rgb(cr,
                               LIVES_WIDGET_COLOR_SCALE(color.red),
                               LIVES_WIDGET_COLOR_SCALE(color.green),
                               LIVES_WIDGET_COLOR_SCALE(color.blue));
#endif
  lives_painter_rectangle(cr,x,y,width,height);
  lives_painter_fill(cr);
#endif
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_surface_destroy(lives_painter_surface_t *surf) {
#ifdef PAINTER_CAIRO
  cairo_surface_destroy(surf);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  surf->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}

LIVES_INLINE boolean lives_painter_new_path(lives_painter_t *cr) {
#ifdef PAINTER_CAIRO
  cairo_new_path(cr);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  delete cr->p;
  cr->p = new QPainterPath;
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_translate(lives_painter_t *cr, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_translate(cr,x,y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QTransform qt;
  qt.translate(x,y);
  cr->setTransform(qt,true);
  return TRUE;
#endif
  return FALSE;

}


LIVES_INLINE boolean lives_painter_set_line_width(lives_painter_t *cr, double width) {
#ifdef PAINTER_CAIRO
  cairo_set_line_width(cr,width);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->pen.setWidthF(width);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_move_to(lives_painter_t *cr, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_move_to(cr,x,y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->moveTo(x,y);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_line_to(lives_painter_t *cr, double x, double y) {
#ifdef PAINTER_CAIRO
  cairo_line_to(cr,x,y);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->lineTo(x,y);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_rectangle(lives_painter_t *cr, double x, double y, double width, double height) {
#ifdef PAINTER_CAIRO
  cairo_rectangle(cr,x,y,width,height);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->addRect(x,y,width,height);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_arc(lives_painter_t *cr, double xc, double yc, double radius, double angle1, double angle2) {
#ifdef PAINTER_CAIRO
  cairo_arc(cr,xc,yc,radius,angle1,angle2);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  double l=xc-radius;
  double t=yc-radius;
  double w=radius*2,h=w;
  angle1=angle1/M_PI*180.;
  angle2=angle2/M_PI*180.;
  cr->p->arcTo(l,t,w,h,angle1,angle2-angle1);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_set_operator(lives_painter_t *cr, lives_painter_operator_t op) {
  // if op was not LIVES_PAINTER_OPERATOR_DEFAULT, and FALSE is returned, then the operation failed,
  // and op was set to the default
#ifdef PAINTER_CAIRO
  cairo_set_operator(cr,op);
  if (op==LIVES_PAINTER_OPERATOR_UNKNOWN) return FALSE;
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->setCompositionMode(op);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_set_source_rgb(lives_painter_t *cr, double red, double green, double blue) {
  // r,g,b values 0.0 -> 1.0
#ifdef PAINTER_CAIRO
  cairo_set_source_rgb(cr,red,green,blue);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QColor qc(red*255.,green*255.,blue*255.);
  cr->pen.setColor(qc);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_set_source_rgba(lives_painter_t *cr, double red, double green, double blue, double alpha) {
  // r,g,b,a values 0.0 -> 1.0
#ifdef PAINTER_CAIRO
  cairo_set_source_rgba(cr,red,green,blue,alpha);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  QColor qc(red*255.,green*255.,blue*255.,alpha*255.);
  cr->pen.setColor(qc);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_set_fill_rule(lives_painter_t *cr, lives_painter_fill_rule_t fill_rule) {
#ifdef PAINTER_CAIRO
  cairo_set_fill_rule(cr,fill_rule);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  cr->p->setFillRule(fill_rule);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_painter_surface_flush(lives_painter_surface_t *surf) {
#ifdef PAINTER_CAIRO
  cairo_surface_flush(surf);
  return TRUE;
#endif
#ifdef PAINTER_QPAINTER
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE lives_painter_surface_t *lives_painter_image_surface_create_for_data(uint8_t *data, lives_painter_format_t format,
    int width, int height, int stride) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  surf=cairo_image_surface_create_for_data(data,format,width,height,stride);
#endif
#ifdef PAINTER_QPAINTER
  surf=new lives_painter_surface_t(data,format,width,height,stride);
#endif
  return surf;
}


LIVES_INLINE lives_painter_surface_t *lives_painter_surface_create_from_widget(LiVESWidget *widget, lives_painter_content_t content,
    int width, int height) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  LiVESXWindow *window=lives_widget_get_xwindow(widget);
  if (window!=NULL) {
#if G_ENCODE_VERSION(GDK_MAJOR_VERSION,GDK_MINOR_VERSION) >= G_ENCODE_VERSION(2,22)
    surf=gdk_window_create_similar_surface(window,content,width,height);
#else
    surf=cairo_image_surface_create(LIVES_PAINTER_FORMAT_ARGB32,width,height);
#endif
  }
#endif

#ifdef PAINTER_QPAINTER
  surf=new lives_painter_surface_t(width,height,LIVES_PAINTER_FORMAT_ARGB32);
#endif

  return surf;
}


LIVES_INLINE lives_painter_surface_t *lives_painter_image_surface_create(lives_painter_format_t format, int width, int height) {
  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  surf=cairo_image_surface_create(format,width,height);
#endif
#ifdef PAINTER_QPAINTER
  surf=new lives_painter_surface_t(width,height,format);
#endif
  return surf;
}



////////////////////////// painter info funcs

lives_painter_surface_t *lives_painter_get_target(lives_painter_t *cr) {

  lives_painter_surface_t *surf=NULL;
#ifdef PAINTER_CAIRO
  surf=cairo_get_target(cr);
#endif
#ifdef PAINTER_QPAINTER
  surf=cr->target;
#endif
  return surf;

}


int lives_painter_format_stride_for_width(lives_painter_format_t form, int width) {
  int stride=-1;
#ifdef PAINTER_CAIRO
  stride=cairo_format_stride_for_width(form,width);
#endif
#ifdef PAINTER_QPAINTER
  stride=width * 4; //TODO !!
#endif
  return stride;
}


uint8_t *lives_painter_image_surface_get_data(lives_painter_surface_t *surf) {
  uint8_t *data=NULL;
#ifdef PAINTER_CAIRO
  data=cairo_image_surface_get_data(surf);
#endif
#ifdef PAINTER_QPAINTER
  data=(uint8_t *)surf->bits();
#endif
  return data;
}


int lives_painter_image_surface_get_width(lives_painter_surface_t *surf) {
  int width=0;
#ifdef PAINTER_CAIRO
  width=cairo_image_surface_get_width(surf);
#endif
#ifdef PAINTER_QPAINTER
  width=((QImage *)surf)->width();
#endif
  return width;
}


int lives_painter_image_surface_get_height(lives_painter_surface_t *surf) {
  int height=0;
#ifdef PAINTER_CAIRO
  height=cairo_image_surface_get_height(surf);
#endif
#ifdef PAINTER_QPAINTER
  height=((QImage *)surf)->height();
#endif
  return height;
}


int lives_painter_image_surface_get_stride(lives_painter_surface_t *surf) {
  int stride=0;
#ifdef PAINTER_CAIRO
  stride=cairo_image_surface_get_stride(surf);
#endif
#ifdef PAINTER_QPAINTER
  stride=((QImage *)surf)->bytesPerLine();
#endif
  return stride;
}


lives_painter_format_t lives_painter_image_surface_get_format(lives_painter_surface_t *surf) {
  lives_painter_format_t format=(lives_painter_format_t)0;
#ifdef PAINTER_CAIRO
  format=cairo_image_surface_get_format(surf);
#endif
#ifdef PAINTER_QPAINTER
  format=((QImage *)surf)->format();
#endif
  return format;
}


////////////////////////////////////////////////////////

LIVES_INLINE boolean lives_mem_set_vtable(LiVESMemVTable *alt_vtable) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  return FALSE;
#else
  g_mem_set_vtable(alt_vtable);
  return TRUE;
#endif
#endif

#ifdef GUI_QT
  static_alt_vtable = alt_vtable;

  lives_free = alt_vtable->free;

  lives_malloc = malloc_wrapper;

  lives_realloc = realloc_wrapper;

  if (alt_vtable->try_malloc == NULL) lives_try_malloc = try_malloc_wrapper;
  else lives_try_malloc = alt_vtable->try_malloc;

  if (alt_vtable->try_realloc == NULL) lives_try_realloc = try_realloc_wrapper;
  else lives_try_realloc = alt_vtable->try_realloc;

  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE livespointer lives_object_ref(livespointer object) {
#ifdef GUI_GTK
  g_object_ref(object);
#endif
#ifdef GUI_QT
  static_cast<LiVESObject *>(object)->inc_refcount();
#endif
  return object;
}


LIVES_INLINE boolean lives_object_unref(livespointer object) {
#ifdef GUI_GTK
  g_object_unref(object);
  return TRUE;
#endif
#ifdef GUI_QT
  static_cast<LiVESObject *>(object)->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}


#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
LIVES_INLINE livespointer lives_object_ref_sink(livespointer object) {
  g_object_ref_sink(object);
  return object;
}
#else
LIVES_INLINE void lives_object_ref_sink(livespointer object) {
  GtkObject *gtkobject;
  //assert(GTK_IS_OBJECT(object));
  gtkobject=(GtkObject *)object;
  gtk_object_sink(gtkobject);
}
#endif
#endif

#ifdef GUI_QT
LIVES_INLINE livespointer lives_object_ref_sink(livespointer object) {
  static_cast<LiVESObject *>(object)->ref_sink();
  return object;
}
#endif



LIVES_INLINE boolean lives_signal_handler_block(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_block(instance,handler_id);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESObject *obj = static_cast<LiVESObject *>(instance);
  obj->block_signal(handler_id);
  return TRUE;
#endif
  return FALSE;
}

#ifndef GUI_GTK
LIVES_INLINE boolean lives_signal_handlers_block_by_func(livespointer instance, livespointer func, livespointer data) {
#ifdef GUI_QT
  LiVESObject *obj = static_cast<LiVESObject *>(instance);
  obj->block_signals((ulong)func,data);
  return TRUE;
#endif
  return FALSE;
}
#endif

LIVES_INLINE boolean lives_signal_handler_unblock(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_unblock(instance,handler_id);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESObject *obj = static_cast<LiVESObject *>(instance);
  obj->unblock_signal(handler_id);
  return TRUE;
#endif
  return FALSE;
}


#ifndef GUI_GTK
LIVES_INLINE boolean lives_signal_handlers_unblock_by_func(livespointer instance, livespointer func, livespointer data) {
#ifdef GUI_QT
  LiVESObject *obj = static_cast<LiVESObject *>(instance);
  obj->unblock_signals((ulong)func,data);
  return TRUE;
#endif
  return FALSE;
}
#endif



LIVES_INLINE boolean lives_signal_handler_disconnect(livespointer instance, unsigned long handler_id) {
#ifdef GUI_GTK
  g_signal_handler_disconnect(instance,handler_id);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESObject *obj = static_cast<LiVESObject *>(instance);
  obj->disconnect_signal(handler_id);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_signal_stop_emission_by_name(livespointer instance, const char *detailed_signal) {
#ifdef GUI_GTK
  g_signal_stop_emission_by_name(instance,detailed_signal);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESWidget *widget = static_cast<LiVESWidget *>(instance);
  if (!strcmp(detailed_signal, LIVES_WIDGET_EXPOSE_EVENT)) widget->add_onetime_event_block(LIVES_EXPOSURE_MASK);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE boolean lives_widget_set_sensitive(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
  gtk_widget_set_sensitive(widget,state);

#ifdef GTK_SUBMENU_SENS_BUG
  if (GTK_IS_MENU_ITEM(widget)) {
    LiVESWidget *sub;
    if ((sub=gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget)))!=NULL) {
      g_object_ref(sub);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget),NULL);
      gtk_widget_set_sensitive(sub,state);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget),sub);
      g_object_unref(sub);
    }
  }
#endif

  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->setEnabled(state);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_get_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_sensitive(widget);
#endif
#ifdef GUI_QT
  return (static_cast<QWidget *>(widget))->isEnabled();
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_show(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(widget) && (static_cast<LiVESMainWindow *>(widget))->get_position() == LIVES_WIN_POS_CENTER_ALWAYS) {
    QRect primaryScreenGeometry(QApplication::desktop()->screenGeometry());
    widget->move(-50000,-50000);
    (static_cast<QWidget *>(widget))->setVisible(true);
    widget->move((primaryScreenGeometry.width() - widget->width()) / 2.0,
                 (primaryScreenGeometry.height() - widget->height()) / 2.0);
  } else {
    (static_cast<QWidget *>(widget))->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_hide(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_hide(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->setVisible(false);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_show_all(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_show_all(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->show();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_destroy(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_destroy(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_queue_draw(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_queue_draw(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->update();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_queue_draw_area(LiVESWidget *widget, int x, int y, int width, int height) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_queue_draw_area(widget,x,y,width,height);
#else
  gtk_widget_queue_draw(widget);
#endif
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->update(x,y,width,height);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_queue_resize(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_queue_resize(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->updateGeometry();
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_widget_set_size_request(LiVESWidget *widget, int width, int height) {
#ifdef GUI_GTK
  gtk_widget_set_size_request(widget,width,height);
  return TRUE;
#endif
#ifdef GUI_QT
  QWidget *widg=(static_cast<QWidget *>(widget));
  widg->setMaximumSize(width,height);
  widg->setMinimumSize(width,height);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_process_updates(LiVESWidget *widget, boolean upd_children) {
#ifdef GUI_GTK
  GdkWindow *window=lives_widget_get_xwindow(widget);
  gdk_window_process_updates(window,upd_children);
  return TRUE;
#endif
#ifdef GUI_QT
  QWidget *widg=(static_cast<QWidget *>(widget));
  widg->repaint();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_xwindow_process_all_updates() {
#ifdef GUI_GTK
  gdk_window_process_all_updates();
  return TRUE;
#endif
#ifdef GUI_QT
  QCoreApplication::processEvents();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_reparent(LiVESWidget *widget, LiVESWidget *new_parent) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,14,0)
  g_object_ref(widget);
  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(widget)),widget);
  gtk_container_add(GTK_CONTAINER(new_parent),widget);
  g_object_unref(widget);
#else
  gtk_widget_reparent(widget,new_parent);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->setParent(static_cast<QWidget *>(new_parent));
  widget->inc_refcount();
  widget->get_parent()->remove_child(widget);
  new_parent->add_child(widget);
  widget->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_app_paintable(LiVESWidget *widget, boolean paintable) {
  return TRUE;
#ifdef GUI_GTK
  gtk_widget_set_app_paintable(widget,paintable);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QWidget *>(widget))->setUpdatesEnabled(!paintable);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESResponseType lives_dialog_run(LiVESDialog *dialog) {
#ifdef GUI_GTK
  return gtk_dialog_run(dialog);
#endif
#ifdef GUI_QT
  int dc;
  dc=dialog->exec();
  return dc;
#endif
  return LIVES_RESPONSE_INVALID;
}


LIVES_INLINE boolean lives_dialog_response(LiVESDialog *dialog, int response) {
#ifdef GUI_GTK
  gtk_dialog_response(dialog,response);
  return TRUE;
#endif
#ifdef GUI_QT
  dialog->setResult(response);
  return TRUE;
#endif
  return FALSE;
}


#if GTK_CHECK_VERSION(3,16,0)
static char *make_random_string() {
  char *str=malloc(32);
  register int i;

  str[0]=str[1]=str[2]='X';

  for (i=3; i<31; i++) str[i]=((lives_random()&15)+65);
  str[31]=0;
  return str;
}
#endif

#include "giw/giwled.h"

LIVES_INLINE boolean lives_widget_set_bg_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
#if GTK_CHECK_VERSION(3,16,0)

  GtkCssProvider *provider = gtk_css_provider_new();
  GtkStyleContext *ctx = gtk_widget_get_style_context(widget);

  char *widget_name,*wname;
  char *colref;
  char *css_string;
  char *state_str;

  gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER
                                 (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  widget_name=g_strdup(gtk_widget_get_name(widget));

  if (strncmp(widget_name,"XXX",3)) {
    if (widget_name!=NULL) g_free(widget_name);
    widget_name=make_random_string();
    gtk_widget_set_name(widget,widget_name);
  }

  gtk_widget_set_name(widget,widget_name);

  colref=gdk_rgba_to_string(color);

#ifdef GTK_TEXT_VIEW_CSS_BUG
  if (GTK_IS_TEXT_VIEW(widget)) wname=g_strdup("GtkTextView");
  else {
#endif
    switch (state) {
    case GTK_STATE_FLAG_ACTIVE:
      state_str=":active";
      break;
    case GTK_STATE_FLAG_PRELIGHT:
      state_str=":prelight";
      break;
    case GTK_STATE_FLAG_SELECTED:
      state_str=":selected";
      break;
    case GTK_STATE_FLAG_INSENSITIVE:
      state_str=":insensitive";
      break;
    default:
      state_str="";
    }
    
    if (GTK_IS_NOTEBOOK(widget)) wname=g_strdup_printf("#%s tab",widget_name);
    else wname=g_strdup_printf("#%s%s",widget_name,state_str);

#ifdef GTK_TEXT_VIEW_CSS_BUG
  }
#endif


  css_string=g_strdup_printf(" %s {\n background-color: %s;\n }\n }\n",wname,colref);

  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                  css_string,
                                  -1, NULL);



  
  g_free(colref);
  g_free(widget_name);
  g_free(wname);

  g_free(css_string);
  g_object_unref(provider);

#else
  gtk_widget_override_background_color(widget,state,color);
#endif
#else
  gtk_widget_modify_bg(widget,state,color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_bg_color(state,color);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_fg_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
#if GTK_CHECK_VERSION(3,16,0)

  GtkCssProvider *provider = gtk_css_provider_new();
  GtkStyleContext *ctx = gtk_widget_get_style_context(widget);

  char *widget_name,*wname;
  char *colref;
  char *css_string;

  gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER
                                 (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  widget_name=g_strdup(gtk_widget_get_name(widget));

  if (strncmp(widget_name,"XXX",3)) {
    if (widget_name!=NULL) g_free(widget_name);
    widget_name=make_random_string();
    gtk_widget_set_name(widget,widget_name);
  }

  colref=gdk_rgba_to_string(color);

#ifdef GTK_TEXT_VIEW_CSS_BUG
  if (GTK_IS_TEXT_VIEW(widget)) wname=g_strdup("GtkTextView");
  else {
#endif
    if (GTK_IS_NOTEBOOK(widget)) wname=g_strdup_printf("#%s tab",widget_name);
    wname=g_strdup_printf("#%s",widget_name);


#ifdef GTK_TEXT_VIEW_CSS_BUG
  }
#endif
  
  css_string=g_strdup_printf(" %s {\n color: %s;\n }\n }\n",wname,colref);

  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                  css_string,
                                  -1, NULL);

  g_free(colref);
  g_free(widget_name);
  g_free(wname);

  g_free(css_string);
  g_object_unref(provider);
#else
  gtk_widget_override_color(widget,state,color);
#endif
#else
  gtk_widget_modify_fg(widget,state,color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_fg_color(state,color);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_text_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  lives_widget_set_fg_color(widget,state,color);
#else
  gtk_widget_modify_text(widget,state,color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_text_color(state,color);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_base_color(LiVESWidget *widget, LiVESWidgetState state, const LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  lives_widget_set_bg_color(widget,state,color);
#else
  gtk_widget_modify_base(widget,state,color);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_base_color(state,color);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_widget_get_fg_state_color(LiVESWidget *widget, LiVESWidgetState state, LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_style_context_get_color(gtk_widget_get_style_context(widget), LIVES_WIDGET_STATE_NORMAL, color);
#else
  lives_widget_color_copy(color,&gtk_widget_get_style(widget)->fg[LIVES_WIDGET_STATE_NORMAL]);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  lives_widget_color_copy(color,widget->get_fg_color(state));
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_get_bg_state_color(LiVESWidget *widget, LiVESWidgetState state, LiVESWidgetColor *color) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color(gtk_widget_get_style_context(widget), LIVES_WIDGET_STATE_NORMAL, color);
  G_GNUC_END_IGNORE_DEPRECATIONS
#else
  lives_widget_color_copy(color,&gtk_widget_get_style(widget)->bg[LIVES_WIDGET_STATE_NORMAL]);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  lives_widget_color_copy(color,widget->get_bg_color(state));
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidgetColor *lives_widget_color_copy(LiVESWidgetColor *c1, const LiVESWidgetColor *c2) {
  // if c1 is NULL, create a new copy of c2, otherwise copy c2 -> c1
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
  } else {
#if GTK_CHECK_VERSION(3,0,0)
    c0=gdk_rgba_copy(c2);
#else
    c0=gdk_color_copy(c2);
#endif
  }
#endif

#ifdef GUI_QT
  if (c1==NULL) {
    c1 = new LiVESWidgetColor;
  }
  c1->red=c2->red;
  c1->green=c2->green;
  c1->blue=c2->blue;
  c1->alpha=c2->alpha;

  c0 = c1;
#endif

  return c0;
}


LIVES_INLINE LiVESWidget *lives_event_box_new(void) {
  LiVESWidget *eventbox=NULL;
#ifdef GUI_GTK
  eventbox=gtk_event_box_new();
#endif
#ifdef GUI_QT
  eventbox = new LiVESEventBox;
#endif
  return eventbox;
}


LIVES_INLINE boolean lives_event_box_set_above_child(LiVESEventBox *ebox, boolean set) {
#ifdef GUI_GTK
  gtk_event_box_set_above_child(ebox,set);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_image_new(void) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
  image=gtk_image_new();
#endif
#ifdef GUI_QT
  image=new LiVESImage;
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
  } else {
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

#ifdef GUI_QT
  if (!QIcon::hasThemeIcon(stock_id)) lives_printerr("Missing icon %s\n",stock_id);
  else {
    QIcon qi = QIcon::fromTheme(stock_id);
    QPixmap qp = qi.pixmap(size);

    QImage qm = qp.toImage();
    uint8_t *data = qm.bits();
    int width = qm.width();
    int height = qm.height();
    int bpl = qm.bytesPerLine();
    QImage::Format fmt = qm.format();

    image = new LiVESImage(data,width,height,bpl,fmt,imclean,data);
  }

#endif
  return image;
}


LIVES_INLINE LiVESWidget *lives_image_new_from_file(const char *filename) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
  image=gtk_image_new_from_file(filename);
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(filename); // TODO ??
  QImage qm(qs);
  uint8_t *data = qm.bits();
  int width = qm.width();
  int height = qm.height();
  int bpl = qm.bytesPerLine();
  QImage::Format fmt = qm.format();

  image = new LiVESImage(data,width,height,bpl,fmt,imclean,data);
#endif
  return image;
}


LIVES_INLINE LiVESWidget *lives_image_new_from_pixbuf(LiVESPixbuf *pixbuf) {
  LiVESWidget *image=NULL;
#ifdef GUI_GTK
  image=gtk_image_new_from_pixbuf(pixbuf);
#endif
#ifdef GUI_QT
  image = new LiVESImage(static_cast<QImage *>(pixbuf));
#endif
  return image;
}



LIVES_INLINE boolean lives_image_set_from_pixbuf(LiVESImage *image, LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  gtk_image_set_from_pixbuf(image,pixbuf);
  return TRUE;
#endif
#ifdef GUI_QT
  *(static_cast<QImage *>(image)) = pixbuf->copy(0, 0, (static_cast<QImage *>(pixbuf))->width(), (static_cast<QImage *>(pixbuf))->height());
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESPixbuf *lives_image_get_pixbuf(LiVESImage *image) {
  LiVESPixbuf *pixbuf=NULL;
#ifdef GUI_GTK
  pixbuf=gtk_image_get_pixbuf(image);
#endif
#ifdef GUI_QT
  pixbuf = new LiVESPixbuf(image);
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
#ifdef GUI_QT
  QColor qc = QColor(spec);
  if (qc.isValid()) {
    color->red = qc.redF();
    color->green = qc.greenF();
    color->blue = qc.blueF();
    color->alpha = qc.alphaF();
    retval=TRUE;
  }
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
#ifdef GUI_QT
  return dialog->get_content_area();
#endif
  return NULL;
}



LIVES_INLINE LiVESWidget *lives_dialog_get_action_area(LiVESDialog *dialog) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
#ifdef G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#endif
  return gtk_dialog_get_action_area(LIVES_DIALOG(dialog));
#ifdef G_GNUC_END_IGNORE_DEPRECATIONS
  G_GNUC_END_IGNORE_DEPRECATIONS
#endif
#else
  return LIVES_DIALOG(dialog)->vbox;
#endif
#endif
#ifdef GUI_QT
  return dialog->get_action_area();
#endif
  return NULL;
}



LIVES_INLINE boolean lives_dialog_add_action_widget(LiVESDialog *dialog, LiVESWidget *widget, int response) {
#ifdef GUI_GTK
  gtk_dialog_add_action_widget(dialog,widget,response);
  return TRUE;
#endif
#ifdef GUI_QT
  QDialogButtonBox *qbb = dynamic_cast<QDialogButtonBox *>(dialog->get_action_area());

  qbb->addButton(dynamic_cast<QPushButton *>(widget), static_cast<QDialogButtonBox::ButtonRole>(response));
#endif
  return FALSE;

}


LIVES_INLINE LiVESWidget *lives_window_new(LiVESWindowType wintype) {
  LiVESWidget *window=NULL;
#ifdef GUI_GTK
  window=gtk_window_new(wintype);
#endif

#ifdef GUI_QT
  if (wintype == LIVES_WINDOW_TOPLEVEL) {
    window = new LiVESMainWindow;
  } else {
    window = new LiVESDialog;
  }
#endif
  return window;
}


LIVES_INLINE boolean lives_window_set_title(LiVESWindow *window, const char *title) {
#ifdef GUI_GTK
  gtk_window_set_title(window,title);
  return TRUE;
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(title);
  window->setWindowTitle(qs);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_transient_for(LiVESWindow *window, LiVESWindow *parent) {
#ifdef GUI_GTK
  gtk_window_set_transient_for(window,parent);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    parent->winId();
    QWindow *qpwindow = parent->windowHandle();
    qwindow->setTransientParent(qpwindow);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_modal(LiVESWindow *window, boolean modal) {
#ifdef GUI_GTK
  gtk_window_set_modal(window,modal);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    QDialog *qwindow = dynamic_cast<QDialog *>(window);
    if (qwindow != NULL)
      qwindow->setModal(modal);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_deletable(LiVESWindow *window, boolean deletable) {
#ifdef GUI_GTK
  gtk_window_set_deletable(window,deletable);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    //QDialog *qd = (QDialog *)window;
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags;
    if (!deletable) {
      newflags = flags;
      if (flags & Qt::WindowCloseButtonHint) newflags ^= Qt::WindowCloseButtonHint;
      newflags &= (flags | Qt::CustomizeWindowHint | Qt::WindowMinMaxButtonsHint);
    } else {
      newflags = flags | Qt::WindowCloseButtonHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_resizable(LiVESWindow *window, boolean resizable) {
#ifdef GUI_GTK
  gtk_window_set_resizable(window,resizable);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags = flags;
    if (!resizable) {
      if (flags & Qt::WindowMinMaxButtonsHint) newflags ^= Qt::WindowMinMaxButtonsHint;
      newflags |= Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint;
    } else {
      newflags = flags | Qt::WindowMinMaxButtonsHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_keep_below(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_keep_below(window,set);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags = flags;
    if (set) {
      newflags = flags | Qt::WindowStaysOnBottomHint;
    } else {
      if (flags & Qt::WindowStaysOnBottomHint) newflags ^= Qt::WindowStaysOnBottomHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_decorated(LiVESWindow *window, boolean set) {
#ifdef GUI_GTK
  gtk_window_set_decorated(window,set);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_DIALOG(window)) {
    bool vis = window->isVisible();
    const Qt::WindowFlags flags = window->windowFlags();
    Qt::WindowFlags newflags = flags;
    if (!set) {
      newflags = flags | Qt::FramelessWindowHint;
    } else {
      if (flags & Qt::FramelessWindowHint) newflags ^= Qt::FramelessWindowHint;
    }
    window->setWindowFlags(newflags);
    if (vis) window->setVisible(true);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_auto_startup_notification(boolean set) {
#ifdef GUI_GTK
  gtk_window_set_auto_startup_notification(set);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
#endif

  return FALSE;
}



LIVES_INLINE boolean lives_window_set_screen(LiVESWindow *window, LiVESXScreen *screen) {
#ifdef GUI_GTK
  gtk_window_set_screen(window,screen);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setScreen(screen);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_default_size(LiVESWindow *window, int width, int height) {
#ifdef GUI_GTK
  gtk_window_set_default_size(window,width,height);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->resize(width,height);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE const char *lives_window_get_title(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_get_title(window);
#endif
#ifdef GUI_QT
  return (const char *)window->windowTitle().toUtf8().constData();
#endif
  return NULL;
}


LIVES_INLINE boolean lives_window_move(LiVESWindow *window, int x, int y) {
#ifdef GUI_GTK
  gtk_window_move(window,x,y);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qw = window->windowHandle();
    qw->setX(x);
    qw->setY(y);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_get_position(LiVESWidget *widget, int *x, int *y) {
#ifdef GUI_GTK
  GdkWindow *window=lives_widget_get_xwindow(widget);
  gdk_window_get_position(window,x,y);
  return TRUE;
#endif
#ifdef GUI_QT
  QPoint p(0,0);
  p = widget->mapToGlobal(p);
  *x = p.x();
  *y = p.y();
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_window_get_position(LiVESWindow *window, int *x, int *y) {
#ifdef GUI_GTK
  gtk_window_get_position(window,x,y);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qw = window->windowHandle();
    *x = qw->x();
    *y = qw->y();
    return TRUE;
  }
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_set_position(LiVESWindow *window, LiVESWindowPosition pos) {
#ifdef GUI_GTK
  gtk_window_set_position(window,pos);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window))
    (static_cast<LiVESMainWindow *>(window))->set_position(pos);
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
#ifdef GUI_QT
  // TODO
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_resize(LiVESWindow *window, int width, int height) {
#ifdef GUI_GTK
  gtk_window_resize(window,width,height);
  return TRUE;
#endif
  // TODO
  return FALSE;
}


LIVES_INLINE boolean lives_window_present(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_present(window);
  return TRUE;
#endif
#ifdef GUI_QT
  window->raise();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_fullscreen(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_fullscreen(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowFullScreen);
    qwindow->setFlags(Qt::Widget | Qt::Window | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_unfullscreen(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_unfullscreen(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowNoState);
    qwindow->setFlags(Qt::Widget | Qt::Window);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_maximize(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_maximize(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowMaximized);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_unmaximize(LiVESWindow *window) {
#ifdef GUI_GTK
  gtk_window_unmaximize(window);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    window->winId();
    QWindow *qwindow = window->windowHandle();
    qwindow->setWindowState(Qt::WindowNoState);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESAccelGroup *lives_accel_group_new(void) {
  LiVESAccelGroup *group=NULL;
#ifdef GUI_GTK
  group=gtk_accel_group_new();
#endif
#ifdef GUI_QT
  group = new LiVESAccelGroup;
#endif
  return group;
}



LIVES_INLINE boolean lives_accel_group_connect(LiVESAccelGroup *group, uint32_t key, LiVESXModifierType mod,
    LiVESAccelFlags flags, LiVESWidgetClosure *closure) {

#ifdef GUI_GTK
  gtk_accel_group_connect(group,key,mod,flags,closure);
  return TRUE;
#endif
#ifdef GUI_QT
  group->connect(key,mod,flags,closure);
  return FALSE;
#endif
}



LIVES_INLINE boolean lives_accel_group_disconnect(LiVESAccelGroup *group, LiVESWidgetClosure *closure) {

#ifdef GUI_GTK
  gtk_accel_group_disconnect(group,closure);
  return TRUE;
#endif
#ifdef GUI_QT
  group->disconnect(closure);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE boolean lives_widget_add_accelerator(LiVESWidget *widget, const char *accel_signal, LiVESAccelGroup *accel_group,
    uint32_t accel_key, LiVESXModifierType accel_mods, LiVESAccelFlags accel_flags) {
#ifdef GUI_GTK
  gtk_widget_add_accelerator(widget,accel_signal,accel_group,accel_key,accel_mods,accel_flags);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->add_accel(accel_signal,accel_group,accel_key,accel_mods,accel_flags);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_window_add_accel_group(LiVESWindow *window, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_window_add_accel_group(window,group);
  return TRUE;
#endif
#ifdef GUI_QT
  window->add_accel_group(group);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_window_remove_accel_group(LiVESWindow *window, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_window_remove_accel_group(window,group);
  return TRUE;
#endif
#ifdef GUI_QT
  window->remove_accel_group(group);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_menu_set_accel_group(LiVESMenu *menu, LiVESAccelGroup *group) {
#ifdef GUI_GTK
  gtk_menu_set_accel_group(menu,group);
  return TRUE;
#endif
#ifdef GUI_QT
  menu->add_accel_group(group);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_accel_groups_activate(LiVESObject *object, uint32_t key, LiVESXModifierType mod) {
#ifdef GUI_GTK
  gtk_accel_groups_activate(object,key,mod);
  return TRUE;
#endif
#ifdef GUI_QT
  object->activate_accel(key, mod);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE boolean lives_window_has_toplevel_focus(LiVESWindow *window) {
#ifdef GUI_GTK
  return gtk_window_has_toplevel_focus(window);
#endif
#ifdef GUI_QT
  if (LIVES_IS_WINDOW(window)) {
    return QApplication::activeWindow() == static_cast<QWidget *>(window);
  }
  return TRUE;
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
  QImage::Format fmt;
  if (!has_alpha) fmt=QImage::Format_RGB888;
  else {
    fmt=QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  // on destruct, we need to call lives_free_buffer_fn(uchar *pixels, livespointer destroy_fn_data)
  return new LiVESPixbuf(width, height, fmt);
#endif
}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_data(const unsigned char *buf, boolean has_alpha, int width, int height,
    int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn,
    livespointer destroy_fn_data) {

#ifdef GUI_GTK
  return gdk_pixbuf_new_from_data((const guchar *)buf, GDK_COLORSPACE_RGB, has_alpha, 8, width, height, rowstride,
                                  lives_free_buffer_fn,
                                  destroy_fn_data);
#endif


#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  QImage::Format fmt;
  if (!has_alpha) fmt=QImage::Format_RGB888;
  else {
    fmt=QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  return new LiVESPixbuf((uchar *)buf, width, height, rowstride, fmt, imclean, (void *)buf);
#endif

}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file(filename, error);
#endif

#ifdef GUI_QT
  LiVESPixbuf *image = new LiVESPixbuf;
  QString qs = QString::fromUtf8(filename); // TODO ??
  if (!(static_cast<QImage *>(image))->load(qs)) {
    // do something with error
    LIVES_WARN("QImage not loaded");
    delete image;
    return NULL;
  }
  return image;
#endif
  return NULL;
}




LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height,
    boolean preserve_aspect_ratio,
    LiVESError **error) {

#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file_at_scale(filename, width, height, preserve_aspect_ratio, error);
#endif

#ifdef GUI_QT
  QImage image;
  QImage image2;
  Qt::AspectRatioMode asp;
#ifdef IS_MINGW
  QString qs = QString::fromUtf8(filename);
#else
  QString qs = QString::fromLocal8Bit(filename);
#endif
  QImageReader qir;
  qir.setFileName(qs);
  if (!preserve_aspect_ratio) qir.setScaledSize(QSize(width,height));
  if (!qir.read(&image)) {
    if (error != NULL) {
      *error = (LiVESError *)malloc(sizeof(LiVESError));
      (*error)->code = qir.error();
      (*error)->message = strdup(qir.errorString().toUtf8().constData());
    }
    return NULL;
  }
  if (preserve_aspect_ratio) asp=Qt::KeepAspectRatio;
  else asp=Qt::IgnoreAspectRatio;
  image2 = image.scaled(width, height, asp,  Qt::SmoothTransformation);
  if (image2.isNull()) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return new LiVESPixbuf(&image2);
#endif

  return NULL;
}



LIVES_INLINE int lives_pixbuf_get_rowstride(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_rowstride(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->bytesPerLine();
#endif
}


LIVES_INLINE int lives_pixbuf_get_width(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_width(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->width();
#endif
}


LIVES_INLINE int lives_pixbuf_get_height(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_height(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->height();
#endif
}


LIVES_INLINE int lives_pixbuf_get_n_channels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_n_channels(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->depth()>>3;
#endif
}



LIVES_INLINE unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return (uchar *)(dynamic_cast<const QImage *>(pixbuf))->bits();
#endif
}


LIVES_INLINE const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *pixbuf) {

#ifdef GUI_GTK
  return (const guchar *)gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return (const uchar *)(dynamic_cast<const QImage *>(pixbuf))->bits();
#endif
}



LIVES_INLINE boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_has_alpha(pixbuf);
#endif

#ifdef GUI_QT
  return (dynamic_cast<const QImage *>(pixbuf))->hasAlphaChannel();
#endif
}


LIVES_INLINE LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height,
    LiVESInterpType interp_type) {

#ifdef GUI_GTK
  return gdk_pixbuf_scale_simple(src, dest_width, dest_height, interp_type);
#endif


#ifdef GUI_QT
  QImage image = (dynamic_cast<const QImage *>(src))->scaled(dest_width, dest_height, Qt::IgnoreAspectRatio, interp_type);
  if (image.isNull()) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return new LiVESPixbuf(&image);

#endif

}

LIVES_INLINE boolean lives_pixbuf_saturate_and_pixelate(const LiVESPixbuf *src, LiVESPixbuf *dest, float saturation, boolean pixilate) {

#ifdef GUI_GTK
  gdk_pixbuf_saturate_and_pixelate(src, dest, saturation, pixilate);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
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
#ifdef GUI_QT
  adj = new LiVESAdjustment(value,lower,upper,step_increment,page_increment,page_size);
#endif
  return adj;
}


LIVES_INLINE boolean lives_box_set_homogeneous(LiVESBox *box, boolean homogenous) {
#ifdef GUI_GTK
  gtk_box_set_homogeneous(box,homogenous);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_box_reorder_child(LiVESBox *box, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_box_reorder_child(box,child,pos);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    qbox->removeWidget(child);
    qbox->insertWidget(pos,child);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    qbox->removeWidget(child);
    qbox->insertWidget(pos,child);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_box_set_spacing(LiVESBox *box, int spacing) {
#ifdef GUI_GTK
  gtk_box_set_spacing(box,spacing);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    qbox->setSpacing(spacing);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    qbox->setSpacing(spacing);
  }
  return TRUE;
#endif
  return FALSE;
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
#ifdef GUI_QT
  LiVESHBox *hxbox = new LiVESHBox;
  hxbox->setSpacing(spacing);
  hbox = static_cast<LiVESWidget *>(hxbox);
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
#ifdef GUI_QT
  LiVESVBox *vxbox = new LiVESVBox;
  vxbox->setSpacing(spacing);
  vbox = static_cast<LiVESWidget *>(vxbox);
#endif
  return vbox;
}



LIVES_INLINE boolean lives_box_pack_start(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_start(box,child,expand,fill,padding);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    QBoxLayout::Direction dir = qbox->direction();
    if (dir == QBoxLayout::LeftToRight) {
      qbox->setDirection(QBoxLayout::RightToLeft);
    } else if (dir == QBoxLayout::RightToLeft) {
      qbox->setDirection(QBoxLayout::LeftToRight);
    }
    if (fill) child->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Preferred);
    qbox->insertWidget(0,child,expand?100:0,fill?(Qt::Alignment)0:Qt::AlignHCenter);
    qbox->setDirection(dir);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    QBoxLayout::Direction dir = qbox->direction();
    if (dir == QBoxLayout::TopToBottom) qbox->setDirection(QBoxLayout::BottomToTop);
    else qbox->setDirection(QBoxLayout::TopToBottom);
    if (fill) child->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::MinimumExpanding);
    qbox->insertWidget(0,child,expand?100:0,fill?(Qt::Alignment)0:Qt::AlignVCenter);
    qbox->setDirection(dir);
  }
  box->add_child(child);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_box_pack_end(LiVESBox *box, LiVESWidget *child, boolean expand, boolean fill, uint32_t padding) {
#ifdef GUI_GTK
  gtk_box_pack_end(box,child,expand,fill,padding);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_HBOX(box)) {
    QHBoxLayout *qbox = dynamic_cast<QHBoxLayout *>(box);
    if (fill) child->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Preferred);
    qbox->insertWidget(0,child,expand?100:0,fill?(Qt::Alignment)0:Qt::AlignHCenter);
  } else {
    QVBoxLayout *qbox = dynamic_cast<QVBoxLayout *>(box);
    if (fill) child->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::MinimumExpanding);
    qbox->insertWidget(0,child,expand?100:0,fill?(Qt::Alignment)0:Qt::AlignVCenter);
  }
  box->add_child(child);
  return TRUE;
#endif
  return FALSE;
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
#ifdef GUI_QT
  hsep = new LiVESHSeparator;
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
#ifdef GUI_QT
  vsep = new LiVESVSeparator;
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
#ifdef GUI_QT
  bbox = new LiVESButtonBox(LIVES_ORIENTATION_HORIZONTAL);
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
#ifdef GUI_QT
  bbox = new LiVESButtonBox(LIVES_ORIENTATION_VERTICAL);
#endif
#endif
  return bbox;
}



LIVES_INLINE boolean lives_button_box_set_layout(LiVESButtonBox *bbox, LiVESButtonBoxStyle bstyle) {
#ifdef GUI_GTK
  gtk_button_box_set_layout(bbox,bstyle);
  return TRUE;
#ifdef GUI_QT
  if (bstyle == LIVES_BUTTONBOX_CENTER) {
    bbox->setCenterButtons(true);
  } else {
    bbox->setCenterButtons(false);
  }
#endif
#endif
  return FALSE;
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
#ifdef GUI_QT
  hscale = new LiVESScale(LIVES_ORIENTATION_HORIZONTAL, adj);
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
#ifdef GUI_QT
  vscale = new LiVESScale(LIVES_ORIENTATION_VERTICAL, adj);
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
#ifdef GUI_QT
  LiVESPaned *qs = new LiVESPaned;
  qs->setOrientation(LIVES_ORIENTATION_HORIZONTAL);
  hpaned = static_cast<LiVESWidget *>(qs);
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
#ifdef GUI_QT
  LiVESPaned *qs = new LiVESPaned;
  qs->setOrientation(LIVES_ORIENTATION_VERTICAL);
  vpaned = static_cast<LiVESWidget *>(qs);
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
#ifdef GUI_QT
  hscrollbar = new LiVESScrollbar(LIVES_ORIENTATION_HORIZONTAL, adj);
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
#ifdef GUI_QT
  vscrollbar = new LiVESScrollbar(LIVES_ORIENTATION_VERTICAL, adj);
#endif
  return vscrollbar;
}


LIVES_INLINE LiVESWidget *lives_label_new(const char *text) {
  LiVESWidget *label=NULL;
#ifdef GUI_GTK
  label=gtk_label_new(text);
  gtk_label_set_justify(LIVES_LABEL(label), widget_opts.justify);
  gtk_label_set_line_wrap(LIVES_LABEL(label), widget_opts.line_wrap);
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(text);
  LiVESLabel *ql = new LiVESLabel(qs);
  ql->setAlignment(widget_opts.justify);
  ql->setWordWrap(widget_opts.line_wrap);
  ql->setTextFormat(Qt::PlainText);
  label = static_cast<LiVESWidget *>(ql);
#endif
  return label;
}


LIVES_INLINE LiVESWidget *lives_arrow_new(LiVESArrowType arrow_type, LiVESShadowType shadow_type) {
  LiVESWidget *arrow=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,12,0)
  const char *format = "<b>\%s</b>";
  char *markup;
  char *str;

  switch (arrow_type) {
  case LIVES_ARROW_DOWN:
    str="v";
    break;
  case LIVES_ARROW_LEFT:
    str="<";
    break;
  case LIVES_ARROW_RIGHT:
    str=">";
    break;
  default:
    return arrow;
  }

  arrow=gtk_label_new("");
  markup = g_markup_printf_escaped(format, str);
  gtk_label_set_markup(GTK_LABEL(arrow), markup);
  g_free(markup);

#else
  arrow=gtk_arrow_new(arrow_type,shadow_type);
#endif
#endif
#ifdef GUI_QT
  QStyleOption *qstyleopt = 0;
  QIcon qi = QApplication::style()->standardIcon(arrow_type, qstyleopt);
  QPixmap qp = qi.pixmap(LIVES_ICON_SIZE_DIALOG);
  QImage qm = qp.toImage();
  arrow = new LiVESArrow(&qm);
#endif
  return arrow;
}


LIVES_INLINE LiVESWidget *lives_alignment_new(float xalign, float yalign, float xscale, float yscale) {
  LiVESWidget *alignment=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  alignment=gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(LIVES_FRAME(alignment), GTK_SHADOW_NONE);
  if (xalign==0.5) gtk_widget_set_halign(alignment,GTK_ALIGN_CENTER);
  if (xalign==0.&&xscale==1.) gtk_widget_set_halign(alignment,GTK_ALIGN_FILL);
  if (xalign==0.&&xscale==0.) gtk_widget_set_halign(alignment,GTK_ALIGN_START);
  if (yalign==0.5) gtk_widget_set_valign(alignment,GTK_ALIGN_CENTER);
  if (yalign==0.&&yscale==1.) gtk_widget_set_valign(alignment,GTK_ALIGN_FILL);
  if (yalign==0.&&yscale==0.) gtk_widget_set_valign(alignment,GTK_ALIGN_START);
#else
  alignment=gtk_alignment_new(xalign,yalign,xscale,yscale);
#endif
#endif
#ifdef GUI_QT
  alignment = new LiVESAlignment(xalign,yalign,xscale,yscale);
#endif
  return alignment;
}


LIVES_INLINE boolean lives_alignment_set(LiVESAlignment *alignment, float xalign, float yalign, float xscale, float yscale) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (xalign==0.5) gtk_widget_set_halign((GtkWidget *)alignment,GTK_ALIGN_CENTER);
  if (xalign==0.&&xscale==1.) gtk_widget_set_halign((GtkWidget *)alignment,GTK_ALIGN_FILL);
  if (xalign==0.&&xscale==0.) gtk_widget_set_halign((GtkWidget *)alignment,GTK_ALIGN_START);
  if (yalign==0.5) gtk_widget_set_valign((GtkWidget *)alignment,GTK_ALIGN_CENTER);
  if (yalign==0.&&yscale==1.) gtk_widget_set_valign((GtkWidget *)alignment,GTK_ALIGN_FILL);
  if (yalign==0.&&yscale==0.) gtk_widget_set_valign((GtkWidget *)alignment,GTK_ALIGN_START);
#else
  gtk_alignment_set(alignment,xalign,yalign,xscale,yscale);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  alignment->set_alignment(xalign,yalign,xscale,yscale);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_expander_new_with_mnemonic(const char *label) {
  LiVESWidget *expander=NULL;
#ifdef GUI_GTK
  expander=gtk_expander_new_with_mnemonic(label);
#if GTK_CHECK_VERSION(3,2,0)
  gtk_expander_set_resize_toplevel(GTK_EXPANDER(expander),TRUE);
#endif
#endif
  return expander;
}



LIVES_INLINE LiVESWidget *lives_expander_new(const char *label) {
  LiVESWidget *expander=NULL;
#ifdef GUI_GTK
  expander=gtk_expander_new(label);
#if GTK_CHECK_VERSION(3,2,0)
  gtk_expander_set_resize_toplevel(GTK_EXPANDER(expander),TRUE);
#endif
#endif
  return expander;
}



LIVES_INLINE LiVESWidget *lives_expander_get_label_widget(LiVESExpander *expander) {
  LiVESWidget *widget=NULL;
#ifdef GUI_GTK
  widget=gtk_expander_get_label_widget(expander);
#endif
  return widget;
}




LIVES_INLINE boolean lives_label_set_halignment(LiVESLabel *label, float yalign) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,16,0)
  gtk_label_set_yalign(label,yalign);
#else
  gtk_misc_set_alignment(GTK_MISC(label),0.,yalign);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QRect qr = (static_cast<QFrame *>(label))->contentsRect();
  int pixels = (float)qr.width() * yalign;
  label->setIndent(pixels);
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_combo_new(void) {
  LiVESWidget *combo=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  combo = gtk_combo_box_text_new_with_entry();
#else
  combo = gtk_combo_box_entry_new_text();
#endif
#endif
#ifdef GUI_QT
  combo = new LiVESCombo;
#endif
  return combo;
}


LIVES_INLINE LiVESWidget *lives_combo_new_with_model(LiVESTreeModel *model) {
  LiVESWidget *combo=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  combo = gtk_combo_box_new_with_model_and_entry(model);
#else
  combo = gtk_combo_box_entry_new();
  gtk_combo_box_set_model(GTK_COMBO_BOX(combo),model);
#endif
#endif
#ifdef GUI_QT
  combo = new LiVESCombo;
  QComboBox *qcombo = dynamic_cast<QComboBox *>(combo);
  qcombo->setModel(model->to_qsimodel());
  if (model->get_qtree_widget() != NULL) qcombo->setView(model->get_qtree_widget());
#endif
  return combo;

}


LIVES_INLINE LiVESTreeModel *lives_combo_get_model(LiVESCombo *combo) {
  LiVESTreeModel *model=NULL;
#ifdef GUI_GTK
  model=gtk_combo_box_get_model(combo);
#endif
#ifdef GUI_QT
  QAbstractItemModel *qqmodel = (static_cast<QComboBox *>(combo))->model();

  QVariant qv = (qqmodel)->property("LiVESObject");

  if (qv.isValid()) {
    model = static_cast<LiVESTreeModel *>(qv.value<LiVESObject *>());
  }

#endif
  return model;
}



LIVES_INLINE boolean lives_combo_append_text(LiVESCombo *combo, const char *text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),text);
#else
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),text);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(text);
  (static_cast<QComboBox *>(combo))->addItem(qs);
  return TRUE;
#endif
  return FALSE;
}



static boolean lives_combo_remove_all_text(LiVESCombo *combo) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
#else
  register int count = lives_tree_model_iter_n_children(lives_combo_get_model(combo),NULL);
  while (count-- > 0) gtk_combo_box_remove_text(combo,0);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  static_cast<QComboBox *>(combo)->clear();
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE boolean lives_combo_set_entry_text_column(LiVESCombo *combo, int column) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo),column);
#else
  gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(combo),column);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QComboBox *>(combo))->setModelColumn(column);
  return TRUE;
#endif
  return FALSE;
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
#ifdef GUI_QT
  return strdup((static_cast<QComboBox *>(combo))->currentText().toUtf8().constData());
#endif
  return NULL;
}


LIVES_INLINE boolean lives_combo_set_active_index(LiVESCombo *combo, int index) {
#ifdef GUI_GTK
  gtk_combo_box_set_active(combo,index);
  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QComboBox *>(combo))->setCurrentIndex(index);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_combo_set_active_iter(LiVESCombo *combo, LiVESTreeIter *iter) {
#ifdef GUI_GTK
  gtk_combo_box_set_active_iter(combo,iter);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESTreeModel *model = lives_combo_get_model(combo);
  if (model != NULL) {
    QTreeWidget *widget = model->get_qtree_widget();
    if (widget != NULL) widget->setCurrentItem(iter);
  }
  return TRUE;
#endif
  return FALSE;

}


LIVES_INLINE boolean lives_combo_get_active_iter(LiVESCombo *combo, LiVESTreeIter *iter) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active_iter(combo,iter);
#endif
#ifdef GUI_QT
  LiVESTreeModel *model = lives_combo_get_model(combo);
  if (model != NULL) {
    QTreeWidget *widget = model->get_qtree_widget();
    if (widget != NULL) {
      QTreeWidgetItem *qtwi = widget->currentItem();
      *iter = *qtwi;
      return TRUE;
    }
  }
#endif
  return FALSE;
}


LIVES_INLINE int lives_combo_get_active(LiVESCombo *combo) {
#ifdef GUI_GTK
  return gtk_combo_box_get_active(combo);
#endif
#ifdef GUI_QT
  return (static_cast<QComboBox *>(combo))->currentIndex();
#endif
  return -1;
}



LIVES_INLINE LiVESWidget *lives_text_view_new(void) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_text_view_new();
#endif
#ifdef GUI_QT
  tview = new LiVESTextView;
#endif
  return tview;
}



LIVES_INLINE LiVESWidget *lives_text_view_new_with_buffer(LiVESTextBuffer *tbuff) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_text_view_new_with_buffer(tbuff);
#endif
#ifdef GUI_QT
  tview = new LiVESTextView(tbuff);
#endif
  return tview;
}


LIVES_INLINE LiVESTextBuffer *lives_text_view_get_buffer(LiVESTextView *tview) {
  LiVESTextBuffer *tbuff=NULL;
#ifdef GUI_GTK
  tbuff=gtk_text_view_get_buffer(tview);
#endif
#ifdef GUI_QT
  tbuff = tview->get_buffer();
#endif
  return tbuff;
}


LIVES_INLINE boolean lives_text_view_set_editable(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_editable(tview, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setReadOnly(!setting);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_view_set_accepts_tab(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_accepts_tab(tview, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setTabChangesFocus(!setting);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_view_set_cursor_visible(LiVESTextView *tview, boolean setting) {
#ifdef GUI_GTK
  gtk_text_view_set_cursor_visible(tview, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  if (setting) tview->setCursorWidth(1);
  else tview->setCursorWidth(0);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_view_set_wrap_mode(LiVESTextView *tview, LiVESWrapMode wrapmode) {
#ifdef GUI_GTK
  gtk_text_view_set_wrap_mode(tview, wrapmode);
  return TRUE;
#endif
#ifdef GUI_QT
  if (wrapmode == LIVES_WRAP_NONE) tview->setLineWrapMode(QTextEdit::NoWrap);
  if (wrapmode == LIVES_WRAP_WORD) tview->setLineWrapMode(QTextEdit::WidgetWidth);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_view_set_justification(LiVESTextView *tview, LiVESJustification justify) {
#ifdef GUI_GTK
  gtk_text_view_set_justification(tview, justify);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setAlignment(justify);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_view_scroll_mark_onscreen(LiVESTextView *tview, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_view_scroll_mark_onscreen(tview, mark);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
  tview->scrollToAnchor(mark->get_name());
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESTextBuffer *lives_text_buffer_new(void) {
  LiVESTextBuffer *tbuff=NULL;
#ifdef GUI_GTK
  tbuff=gtk_text_buffer_new(NULL);
#endif
#ifdef GUI_QT
  tbuff = new LiVESTextBuffer;
#endif
  return tbuff;
}



LIVES_INLINE boolean lives_text_buffer_insert(LiVESTextBuffer *tbuff, LiVESTextIter *iter, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert(tbuff, iter, text, len);
  return TRUE;
#endif
#ifdef GUI_QT
  QTextCursor qtc = QTextCursor(tbuff);
  qtc.setPosition(*iter);
  QString qs = QString::fromUtf8(text);
  qtc.insertText(qs);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_buffer_insert_at_cursor(LiVESTextBuffer *tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_insert_at_cursor(tbuff, text, len);
  return TRUE;
#endif
#ifdef GUI_QT
  QTextCursor qtc = tbuff->get_cursor();
  QString qs = QString::fromUtf8(text, len);
  qtc.insertText(qs);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_set_text(LiVESTextBuffer *tbuff, const char *text, int len) {
#ifdef GUI_GTK
  gtk_text_buffer_set_text(tbuff, text, len);
  return TRUE;
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(text, len);
  tbuff->setPlainText(qs);
#endif
  return FALSE;
}


LIVES_INLINE char *lives_text_buffer_get_text(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end, boolean inc_hidden_chars) {
#ifdef GUI_GTK
  return gtk_text_buffer_get_text(tbuff,start,end,inc_hidden_chars);
#endif
#ifdef GUI_QT
  QTextCursor qtc = QTextCursor(tbuff);
  qtc.setPosition(*start);
  qtc.setPosition(*end, QTextCursor::KeepAnchor);
  return strdup(qtc.selection().toPlainText().toUtf8().constData());
#endif
  return NULL;
}


LIVES_INLINE boolean lives_text_buffer_get_start_iter(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_start_iter(tbuff, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  *iter = 0;
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_text_buffer_get_end_iter(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_get_end_iter(tbuff, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  *iter = tbuff->characterCount();
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_place_cursor(LiVESTextBuffer *tbuff, LiVESTextIter *iter) {
#ifdef GUI_GTK
  gtk_text_buffer_place_cursor(tbuff, iter);
  return TRUE;
#endif
#ifdef GUI_QT
  tbuff->get_cursor().setPosition(*iter);
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
#ifdef GUI_QT
  tmark = new LiVESTextMark(tbuff,mark_name,*where,left_gravity);
#endif
  return tmark;
}



LIVES_INLINE boolean lives_text_buffer_delete_mark(LiVESTextBuffer *tbuff, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_buffer_delete_mark(tbuff, mark);
  return TRUE;
#endif
#ifdef GUI_QT
  mark->dec_refcount();
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_delete(LiVESTextBuffer *tbuff, LiVESTextIter *start, LiVESTextIter *end) {
#ifdef GUI_GTK
  gtk_text_buffer_delete(tbuff, start, end);
  return TRUE;
#endif
#ifdef GUI_QT
  QTextCursor qtc = QTextCursor(tbuff);
  qtc.setPosition(*start);
  qtc.setPosition(*end, QTextCursor::KeepAnchor);
  qtc.removeSelectedText();
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_text_buffer_get_iter_at_mark(LiVESTextBuffer *tbuff, LiVESTextIter *iter, LiVESTextMark *mark) {
#ifdef GUI_GTK
  gtk_text_buffer_get_iter_at_mark(tbuff, iter, mark);
  return TRUE;
#endif
#ifdef GUI_QT
  *iter = mark->position();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_dialog_new(void) {
  LiVESWidget *dialog=NULL;
#ifdef GUI_GTK
  dialog=gtk_dialog_new();
#endif
#ifdef GUI_QT
  dialog = new LiVESDialog();
#endif
  return dialog;
}


LIVES_INLINE LiVESWidget *lives_button_new(void) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_button_new();
  gtk_button_set_use_underline(GTK_BUTTON(button),TRUE);
#endif
#ifdef GUI_QT
  button = new LiVESButton;
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_button_new_with_mnemonic(const char *label) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_button_new_with_mnemonic(label);
#endif
#ifdef GUI_QT
  QString qlabel = QString::fromUtf8(label);
  qlabel = qlabel.replace('&',"&&");
  qlabel = qlabel.replace('_','&');
  button = new LiVESButton(qlabel);
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_button_new_with_label(const char *label) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_button_new_with_label(label);
#endif
#ifdef GUI_QT
  QString qlabel = QString::fromUtf8(label);
  button = new LiVESButton(qlabel);
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_button_new_from_stock(const char *stock_id, const char *label) {
  LiVESWidget *button=NULL;

#if GTK_CHECK_VERSION(3,10,0) || defined GUI_QT
  if (!strcmp(stock_id,LIVES_STOCK_LABEL_CANCEL)) stock_id=LIVES_STOCK_CANCEL;
  if (!strcmp(stock_id,LIVES_STOCK_LABEL_OK)) stock_id=LIVES_STOCK_OK;
  // gtk 3.10 + -> we need to set the text ourselves
  if (!strcmp(stock_id,LIVES_STOCK_NO)) stock_id="gtk-no";
  if (!strcmp(stock_id,LIVES_STOCK_APPLY)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_APPLY);
  } else if (!strcmp(stock_id,LIVES_STOCK_OK)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_OK);
  } else if (!strcmp(stock_id,LIVES_STOCK_CANCEL)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_CANCEL);
  } else if (!strcmp(stock_id,LIVES_STOCK_YES)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_YES);
  } else if (!strcmp(stock_id,"gtk-no")) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_NO);
  } else if (!strcmp(stock_id,LIVES_STOCK_CLOSE)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_CLOSE);
  } else if (!strcmp(stock_id,LIVES_STOCK_REVERT_TO_SAVED)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_REVERT);
  } else if (!strcmp(stock_id,LIVES_STOCK_REFRESH)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_REFRESH);
  } else if (!strcmp(stock_id,LIVES_STOCK_DELETE)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_DELETE);
  } else if (!strcmp(stock_id,LIVES_STOCK_SAVE)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_SAVE);
  } else if (!strcmp(stock_id,LIVES_STOCK_SAVE_AS)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_SAVE_AS);
  } else if (!strcmp(stock_id,LIVES_STOCK_OPEN)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_OPEN);
  } else if (!strcmp(stock_id,LIVES_STOCK_QUIT)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_QUIT);
  } else if (!strcmp(stock_id,LIVES_STOCK_GO_FORWARD)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_GO_FORWARD);
  } else if (!strcmp(stock_id,LIVES_STOCK_MEDIA_FORWARD)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_MEDIA_FORWARD);
  } else if (!strcmp(stock_id,LIVES_STOCK_MEDIA_REWIND)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_MEDIA_REWIND);
  } else if (!strcmp(stock_id,LIVES_STOCK_MEDIA_STOP)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_MEDIA_STOP);
  } else if (!strcmp(stock_id,LIVES_STOCK_MEDIA_PLAY)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_MEDIA_PLAY);
  } else if (!strcmp(stock_id,LIVES_STOCK_MEDIA_PAUSE)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_MEDIA_PAUSE);
  } else if (!strcmp(stock_id,LIVES_STOCK_MEDIA_RECORD)) {
    button=lives_button_new_with_mnemonic(LIVES_STOCK_LABEL_MEDIA_RECORD);
  } else {
    // text not known
    button=lives_button_new();
  }

#ifdef GUI_GTK
  if (prefs->show_button_images
      ||!strcmp(stock_id,LIVES_STOCK_ADD)
      ||!strcmp(stock_id,LIVES_STOCK_REMOVE)
     ) {
    LiVESWidget *image=gtk_image_new_from_icon_name(stock_id,GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(button),image);
#endif

#ifdef GUI_QT
    if (!QIcon::hasThemeIcon(stock_id)) lives_printerr("Missing icon %s\n",stock_id);
    else {
      QAbstractButton *qbutton = dynamic_cast<QAbstractButton *>(button);
      if (qbutton != NULL) {
        qbutton->setIconSize(LIVES_ICON_SIZE_BUTTON);
        QIcon qi = QIcon::fromTheme(stock_id);
        QPixmap qp = qi.pixmap(qbutton->iconSize());
        qbutton->setIcon(qp);
      }
#endif

    }

#else
  // < 3.10
  button=gtk_button_new_from_stock(stock_id);
#endif

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,6,0)
    gtk_button_set_always_show_image(GTK_BUTTON(button),prefs->show_button_images);
#endif
    if (label!=NULL)
      gtk_button_set_label(GTK_BUTTON(button),label);
#endif


    return button;
  }

#ifdef BALANCE
} // to fix indentation
#endif


LIVES_INLINE boolean lives_button_set_label(LiVESButton *button, const char *label) {
#ifdef GUI_GTK
  gtk_button_set_label(button,label);
  return TRUE;
#endif
#ifdef GUI_QT
  QString qlabel = QString::fromUtf8(label);
  if (button->get_use_underline()) {
    qlabel = qlabel.replace('&',"&&");
    qlabel = qlabel.replace('_','&');
  }
  (dynamic_cast<QAbstractButton *>(button))->setText(qlabel);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_button_set_use_underline(LiVESButton *button, boolean use) {
#ifdef GUI_GTK
  gtk_button_set_use_underline(button,use);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_use_underline(use);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_button_set_relief(LiVESButton *button, LiVESReliefStyle rstyle) {
#ifdef GUI_GTK
  gtk_button_set_relief(button,rstyle);
  return TRUE;
#endif
#ifdef GUI_QT
  if (rstyle==LIVES_RELIEF_NONE) button->setFlat(true);
  else button->setFlat(false);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_button_set_image(LiVESButton *button, LiVESWidget *image) {
#ifdef GUI_GTK
  gtk_button_set_image(button,image);
  return TRUE;
#endif
#ifdef GUI_QT
  QAbstractButton *qbutton = dynamic_cast<QAbstractButton *>(button);
  if (image != NULL && image->get_type() == LIVES_WIDGET_TYPE_IMAGE) {
    QImage *qim = dynamic_cast<QImage *>(image);
    if (qim != NULL) {
      QPixmap qpx;
      qpx.convertFromImage(*qim);
      QIcon *qi = new QIcon(qpx);
      qbutton->setIcon(*qi);
    }
  } else {
    //qbutton->setIcon(NULL);
  }
  qbutton->setIconSize(LIVES_ICON_SIZE_BUTTON);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_button_set_focus_on_click(LiVESButton *button, boolean focus) {
#ifdef GUI_GTK
  gtk_button_set_focus_on_click(button,focus);
  return TRUE;
#endif
#ifdef GUI_QT
  QWidget *qw = (static_cast<QWidget *>(static_cast<QPushButton *>(button)));
  if (qw->focusPolicy() == Qt::NoFocus) return TRUE;
  if (focus) {
    qw->setFocusPolicy(Qt::StrongFocus);
  } else qw->setFocusPolicy(Qt::TabFocus);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_paned_set_position(LiVESPaned *paned, int pos) {
  // call this only after adding widgets

#ifdef GUI_GTK
  gtk_paned_set_position(paned,pos);
  return TRUE;
#endif
#ifdef GUI_QT
  int size = 0;
  QList<int> qli = paned->sizes();
  for (int i = 0; i < qli.size(); i++) {
    size += qli.at(i);
  }
  qli.clear();
  qli.append(pos);
  qli.append(size - pos);
  paned->setSizes(qli);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_paned_pack(int where, LiVESPaned *paned, LiVESWidget *child, boolean resize, boolean shrink) {
#ifdef GUI_GTK
  if (where==1) gtk_paned_pack1(paned,child,resize,shrink);
  else gtk_paned_pack2(paned,child,resize,shrink);
  return TRUE;
#endif
#ifdef GUI_QT
  paned->insertWidget(where - 1, child);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_drawing_area_new(void) {
  LiVESWidget *darea=NULL;
#ifdef GUI_GTK
  darea=gtk_drawing_area_new();
#endif
#ifdef GUI_QT
  darea = new LiVESDrawingArea;
#endif
  return darea;
}



LIVES_INLINE int lives_event_get_time(LiVESXEvent *event) {
#ifdef GUI_GTK
  return gdk_event_get_time(event);
#endif
#ifdef GUI_QT
  // TODO
  LiVESXEventButton *xevent = (LiVESXEventButton *)event;
  return xevent->time;
#endif
  return 0;
}


LIVES_INLINE boolean lives_toggle_button_get_active(LiVESToggleButton *button) {
#ifdef GUI_GTK
  return gtk_toggle_button_get_active(button);
#endif
#ifdef GUI_QT
  return (static_cast<QCheckBox *>(button))->isChecked();
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_toggle_button_set_active(LiVESToggleButton *button, boolean active) {
#ifdef GUI_GTK
  gtk_toggle_button_set_active(button,active);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setChecked(active);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_toggle_button_set_mode(LiVESToggleButton *button, boolean drawind) {
#ifdef GUI_GTK
  gtk_toggle_button_set_mode(button,drawind);
  return TRUE;
#endif
#ifdef GUI_QT
  //button->setChecked(active);
  return TRUE;
#endif
  return FALSE;
}




LIVES_INLINE LiVESWidget *lives_radio_button_new(LiVESSList *group) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_radio_button_new(group);
#endif
#ifdef GUI_QT
  QButtonGroup *qbg;
  button = new LiVESRadioButton;
  if (group == NULL) {
    qbg = new QButtonGroup;
    group = lives_slist_append(group, (void *)qbg);
    qbg->setExclusive(true);
  } else {
    qbg = const_cast<QButtonGroup *>(static_cast<const QButtonGroup *>(lives_slist_nth_data(group, 0)));
  }

  (static_cast<LiVESRadioButton *>(button))->set_group(group);
  qbg->addButton(dynamic_cast<QPushButton *>(button));

#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_check_button_new(void) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_check_button_new();
#endif
#ifdef GUI_QT
  button = new LiVESCheckButton;
#endif
  return button;
}


LIVES_INLINE LiVESWidget *lives_check_button_new_with_label(const char *label) {
  LiVESWidget *button=NULL;
#ifdef GUI_GTK
  button=gtk_check_button_new_with_label(label);
#endif
#ifdef GUI_QT
  button = new LiVESCheckButton;
  QString qlabel = QString::fromUtf8(label);
  if ((static_cast<LiVESButtonBase *>(button))->get_use_underline()) {
    qlabel = qlabel.replace('&',"&&");
    qlabel = qlabel.replace('_','&');
  }
  (dynamic_cast<QAbstractButton *>(button))->setText(qlabel);
#endif
  return button;
}


LIVES_INLINE boolean lives_widget_set_tooltip_text(LiVESWidget *widget, const char *tip_text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(widget,tip_text);
#else
  GtkTooltips *tips;
  tips = gtk_tooltips_new();
  gtk_tooltips_set_tip(tips,widget,tip_text,NULL);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QString qs = QString::fromUtf8(tip_text);
  widget->setToolTip(qs);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_grab_focus(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_grab_focus(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->setFocus(Qt::OtherFocusReason);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_grab_default(LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_widget_grab_default(widget);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_PUSH_BUTTON(widget)) {
    QPushButton *button = dynamic_cast<QPushButton *>(widget);
    if (button != NULL)
      button->setDefault(true);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESSList *lives_radio_button_get_group(LiVESRadioButton *rbutton) {
#ifdef GUI_GTK
  return gtk_radio_button_get_group(rbutton);
#endif
#ifdef GUI_QT
  return rbutton->get_list();
#endif
  return NULL;
}



LIVES_INLINE LiVESWidget *lives_widget_get_parent(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_parent(widget);
#endif
#ifdef GUI_QT
  return widget->get_parent();
#endif
  return NULL;
}



LIVES_INLINE LiVESWidget *lives_widget_get_toplevel(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_toplevel(widget);
#endif
#ifdef GUI_QT
  QWidget *qwidget = widget->window();
  QVariant qv = qwidget->property("LiVESObject");
  if (qv.isValid()) {
    return static_cast<LiVESWidget *>(qv.value<LiVESObject *>());
  }
  return widget;
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
#ifdef GUI_QT
  QWidget *qwidget = static_cast<QWidget *>(widget);
  qwidget->winId();
  return qwidget->windowHandle();
#endif
  return NULL;
}


LIVES_INLINE boolean lives_xwindow_set_keep_above(LiVESXWindow *xwin, boolean setting) {
#ifdef GUI_GTK
  gdk_window_set_keep_above(xwin, setting);
  return TRUE;
#endif
#ifdef GUI_QT
  bool vis = xwin->isVisible();
  const Qt::WindowFlags flags = xwin->flags();
  Qt::WindowFlags newflags = flags;
  if (setting) {
    newflags = flags | Qt::WindowStaysOnTopHint;
  } else {
    if (flags & Qt::WindowStaysOnBottomHint) newflags ^= Qt::WindowStaysOnTopHint;
  }
  xwin->setFlags(newflags);
  if (vis) xwin->setVisible(true);

  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_widget_set_can_focus(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_focus(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_FOCUS);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  if (state) {
    widget->setFocusPolicy(Qt::StrongFocus);
  } else widget->setFocusPolicy(Qt::NoFocus);
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
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_DEFAULT);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_DEFAULT);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_PUSH_BUTTON(widget)) {
    QPushButton *button = dynamic_cast<QPushButton *>(widget);
    if (button != NULL)
      button->setAutoDefault(state);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_add_events(LiVESWidget *widget, int events) {
#ifdef GUI_GTK
  gtk_widget_add_events(widget,events);
  return TRUE;
#endif
#ifdef GUI_QT
  events |= widget->get_events();
  widget->set_events(events);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_set_events(LiVESWidget *widget, int events) {
#ifdef GUI_GTK
  gtk_widget_set_events(widget,events);
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_events(events);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_remove_accelerator(LiVESWidget *widget, LiVESAccelGroup *acgroup,
    uint32_t accel_key, LiVESXModifierType accel_mods) {
#ifdef GUI_GTK
  return gtk_widget_remove_accelerator(widget,acgroup,accel_key,accel_mods);
#endif
#ifdef GUI_QT
  return (static_cast<LiVESObject *>(widget))->remove_accels(acgroup, accel_key, accel_mods);
#endif
  return FALSE;
}


boolean lives_widget_get_preferred_size(LiVESWidget *widget, LiVESRequisition *min_size, LiVESRequisition *nat_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_get_preferred_size(widget,min_size,nat_size);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  *min_size = widget->minimumSize();
  *nat_size = widget->size();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_is_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_is_sensitive(widget);
#else
  return GTK_WIDGET_IS_SENSITIVE(widget);
#endif
#endif
#ifdef GUI_QT
  return widget->isEnabled();
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_is_visible(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_visible(widget);
#else
  return GTK_WIDGET_VISIBLE(widget);
#endif
#endif
#ifdef GUI_QT
  return widget->isVisible();
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_widget_is_realized(LiVESWidget *widget) {
  // used for giw widgets
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_realized(widget);
#else
  return GTK_WIDGET_REALIZED(widget);
#endif
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_container_add(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_add(container,widget);
  return TRUE;
#endif
#ifdef GUI_QT
  if (container == NULL || widget == NULL) return TRUE;
  // types that cannot be cast to qlayout: paned, menushell, notebook, table, textview, toolbar, treeview

  if (LIVES_IS_PANED(container)) {
    QSplitter *ql = dynamic_cast<QSplitter *>(container);
    ql->addWidget(widget);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_SCROLLED_WINDOW(container)) {
    QScrollArea *ql = dynamic_cast<QScrollArea *>(container);
    ql->setWidget(widget);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU(container)) {
    QMenu *ql = dynamic_cast<QMenu *>(container);
    ql->addAction(dynamic_cast<QAction *>(widget));
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU_BAR(container)) {
    QMenuBar *ql = dynamic_cast<QMenuBar *>(container);
    ql->addAction(dynamic_cast<QAction *>(widget));
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_NOTEBOOK(container)) {
    LiVESNotebook *ql = static_cast<LiVESNotebook *>(container);
    ql->addTab(widget,NULL);
    ql->append_page();
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_TABLE(container)) {
    QGridLayout *ql = dynamic_cast<QGridLayout *>(container);
    ql->addWidget(widget, ql->rowCount(), 0);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_TOOLBAR(container)) {
    QToolBar *ql = dynamic_cast<QToolBar *>(container);
    ql->addWidget(widget);
    container->add_child(widget);
    container = NULL;
  } else if (LIVES_IS_FRAME(container)) {
    LiVESFrame *ql = static_cast<LiVESFrame *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_TOOL_ITEM(container)) {
    LiVESToolItem *ql = static_cast<LiVESToolItem *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_WINDOW(container)) {
    LiVESMainWindow *ql = static_cast<LiVESMainWindow *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_DIALOG(container)) {
    LiVESDialog *ql = static_cast<LiVESDialog *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_BUTTON(container)) {
    LiVESButton *ql = static_cast<LiVESButton *>(container);
    container = ql->get_layout();
  }

  if (container != NULL) {
    QLayout *ql = dynamic_cast<QLayout *>(container);
    if (ql == NULL) {
      LIVES_WARN("Attempt to add widget to non-layout class");
      return FALSE;
    }
    ql->addWidget(widget);
    container->add_child(widget);
  }

  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_container_remove(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_remove(container,widget);
  return TRUE;
#endif
#ifdef GUI_QT
  // types that cannot be cast to qlayout: paned, menushell, notebook, table, textview, toolbar, treeview
  if (LIVES_IS_PANED(container)) {
    return FALSE; // cannot remove
  } else if (LIVES_IS_SCROLLED_WINDOW(container)) {
    QScrollArea *ql = dynamic_cast<QScrollArea *>(container);
    ql->takeWidget();
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU(container)) {
    QMenu *ql = dynamic_cast<QMenu *>(container);
    ql->removeAction(dynamic_cast<QAction *>(widget));
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_MENU_BAR(container)) {
    QMenuBar *ql = dynamic_cast<QMenuBar *>(container);
    ql->removeAction(dynamic_cast<QAction *>(widget));
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_NOTEBOOK(container)) {
    QTabWidget *ql = dynamic_cast<QTabWidget *>(container);
    int tabno = container->get_child_index(widget);
    ql->removeTab(tabno);
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_TOOLBAR(container)) {
    QWidget *ql = static_cast<QWidget *>(container);
    ql->removeAction(dynamic_cast<QAction *>(widget));
    container->remove_child(widget);
    container = NULL;
  } else if (LIVES_IS_FRAME(container)) {
    LiVESFrame *ql = static_cast<LiVESFrame *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_TOOL_ITEM(container)) {
    LiVESToolItem *ql = static_cast<LiVESToolItem *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_WINDOW(container)) {
    LiVESMainWindow *ql = static_cast<LiVESMainWindow *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_DIALOG(container)) {
    LiVESDialog *ql = static_cast<LiVESDialog *>(container);
    container = ql->get_layout();
  } else if (LIVES_IS_BUTTON(container)) {
    LiVESButton *ql = static_cast<LiVESButton *>(container);
    container = ql->get_layout();
  }

  if (container != NULL) {
    QLayout *ql = dynamic_cast<QLayout *>(container);
    if (ql == NULL) {
      LIVES_WARN("Attempt to remove widget to non-layout class");
      return FALSE;
    }
    ql->removeWidget(widget);
    container->remove_child(widget);
  }
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
#ifdef GUI_QT
  QLayout *ql = dynamic_cast<QLayout *>(container);
  if (ql == NULL) {
    LIVES_WARN("Attempt to add widget to non-layout class");
    return FALSE;
  }
  ql->setContentsMargins(width, width, width, width);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_container_foreach(LiVESContainer *cont, LiVESWidgetCallback callback, livespointer cb_data) {
  // excludes internal children
#ifdef GUI_GTK
  gtk_container_foreach(cont,callback,cb_data);
  return TRUE;
#endif
#ifdef GUI_QT
  for (int i = 0; i < cont->count_children(); ++i) {
    (*callback)(cont->get_child(i), cb_data);
  }
  return TRUE;
#endif
  return FALSE;
}


static LIVES_INLINE boolean lives_container_forall(LiVESContainer *cont, LiVESWidgetCallback callback, livespointer cb_data) {
  // includes internal children
#ifdef GUI_GTK
  gtk_container_forall(cont,callback,cb_data);
  return TRUE;
#endif
#ifdef GUI_QT
  /*  QLayout *ql = static_cast<QLayout *>(container);
      if (ql == NULL) {
      LIVES_WARN("Attempt to add widget to non-layout class");
      return FALSE;
      }
      for (int i = 0; i < ql->count(); ++i) {
      QWidget *widget = ql->itemAt(i)->widget();
      if (widget) {
      (*callback)(widget, cb_data);
      }
      }
      return TRUE;*/
#endif
  return FALSE;
}


LIVES_INLINE LiVESList *lives_container_get_children(LiVESContainer *cont) {
  LiVESList *children=NULL;
#ifdef GUI_GTK
  children=gtk_container_get_children(cont);
#endif
#ifdef GUI_QT
  LiVESList *list = new LiVESList(*cont->get_children());
  return list;
#endif
  return children;
}



LIVES_INLINE boolean lives_container_set_focus_child(LiVESContainer *cont, LiVESWidget *child) {
#ifdef GUI_GTK
  gtk_container_set_focus_child(cont,child);
  return TRUE;
#endif
#ifdef GUI_QT
  if (child == NULL) {
    for (int i = 0; i < cont->count_children(); ++i) {
      if (cont->get_child(i)->hasFocus()) {
        cont->get_child(i)->clearFocus();
        break;
      }
    }
  } else {
    if (cont->hasFocus()) {
      child->setFocus();
    }
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_progress_bar_new(void) {
  LiVESWidget *pbar=NULL;
#ifdef GUI_GTK
  pbar=gtk_progress_bar_new();
#endif
#ifdef GUI_QT
  pbar = new LiVESProgressBar;
#endif
  return pbar;
}


LIVES_INLINE boolean lives_progress_bar_set_fraction(LiVESProgressBar *pbar, double fraction) {
#ifdef GUI_GTK
  gtk_progress_bar_set_fraction(pbar,fraction);
  return TRUE;
#endif
#ifdef GUI_QT
  pbar->setMinimum(0);
  pbar->setMaximum(1000000);
  pbar->setValue(fraction * 1000000.);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_progress_bar_set_pulse_step(LiVESProgressBar *pbar, double fraction) {
#ifdef GUI_GTK
  gtk_progress_bar_set_pulse_step(pbar,fraction);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_progress_bar_pulse(LiVESProgressBar *pbar) {
#ifdef GUI_GTK
  gtk_progress_bar_pulse(pbar);
  return TRUE;
#endif
#ifdef GUI_QT
  pbar->setMinimum(0);
  pbar->setMaximum(0);
  pbar->setValue(qrand() % 99 + 1);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_spin_button_new(LiVESAdjustment *adj, double climb_rate, uint32_t digits) {
  LiVESWidget *sbutton=NULL;
#ifdef GUI_GTK
  sbutton=gtk_spin_button_new(adj,climb_rate,digits);
#endif
#ifdef GUI_QT
  sbutton = new LiVESSpinButton(adj, climb_rate, digits);
#endif
  return sbutton;
}


LIVES_INLINE double lives_spin_button_get_value(LiVESSpinButton *button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value(button);
#endif
#ifdef GUI_QT
  return button->value();
#endif
  return 0.;
}


LIVES_INLINE int lives_spin_button_get_value_as_int(LiVESSpinButton *button) {
#ifdef GUI_GTK
  return gtk_spin_button_get_value_as_int(button);
#endif
#ifdef GUI_QT
  return (int)button->value();
#endif
  return 0.;
}


LIVES_INLINE LiVESAdjustment *lives_spin_button_get_adjustment(LiVESSpinButton *button) {
  LiVESAdjustment *adj=NULL;
#ifdef GUI_GTK
  adj=gtk_spin_button_get_adjustment(button);
#endif
#ifdef GUI_QT
  adj = button->get_adj();
#endif
  return adj;
}


LIVES_INLINE boolean lives_spin_button_set_value(LiVESSpinButton *button, double value) {
#ifdef GUI_GTK
  gtk_spin_button_set_value(button,value);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setValue(value);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_set_range(LiVESSpinButton *button, double min, double max) {
#ifdef GUI_GTK
  gtk_spin_button_set_range(button,min,max);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESAdjustment *adj = button->get_adj();
  button->setRange(min, max);
  double value = button->value();
  if (value < min) value = min;
  if (value > max) value = max;
  button->setValue(value);
  adj->freeze();
  adj->set_lower(min);
  adj->set_upper(max);
  adj->thaw();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_set_wrap(LiVESSpinButton *button, boolean wrap) {
#ifdef GUI_GTK
  gtk_spin_button_set_wrap(button,wrap);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setWrapping(wrap);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_set_digits(LiVESSpinButton *button, uint32_t digits) {
#ifdef GUI_GTK
  gtk_spin_button_set_digits(button,digits);
  return TRUE;
#endif
#ifdef GUI_QT
  button->setDecimals(digits);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_spin_button_update(LiVESSpinButton *button) {
#ifdef GUI_GTK
  gtk_spin_button_update(button);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE; // not needed ?
#endif
  return FALSE;
}



LIVES_INLINE LiVESToolItem *lives_tool_button_new(LiVESWidget *icon_widget, const char *label) {
  LiVESToolItem *button=NULL;
#ifdef GUI_GTK
  button=gtk_tool_button_new(icon_widget,label);
#endif
#ifdef GUI_QT
  button = new LiVESToolButton(icon_widget, label);
#endif
  return button;
}


LIVES_INLINE LiVESToolItem *lives_tool_item_new(void) {
  LiVESToolItem *item=NULL;
#ifdef GUI_GTK
  item=gtk_tool_item_new();
#endif
#ifdef GUI_QT
  item = new LiVESToolItem;
#endif
  return item;
}


LIVES_INLINE boolean lives_tool_button_set_icon_widget(LiVESToolButton *button, LiVESWidget *icon) {
#ifdef GUI_GTK
  gtk_tool_button_set_icon_widget(button,icon);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_icon_widget(icon);
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tool_button_set_label_widget(LiVESToolButton *button, LiVESWidget *label) {
#ifdef GUI_GTK
  gtk_tool_button_set_label_widget(button,label);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_label_widget(label);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tool_button_set_use_underline(LiVESToolButton *button, boolean use_underline) {
#ifdef GUI_GTK
  gtk_tool_button_set_use_underline(button,use_underline);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_use_underline(use_underline);
  return TRUE;
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
#ifdef GUI_QT
  ruler->setMinimum(lower);
  ruler->setMaximum(upper);
  ruler->setValue(position);
#endif

#endif
}


LIVES_INLINE LiVESWidget *lives_message_dialog_new(LiVESWindow *parent, LiVESDialogFlags flags, LiVESMessageType type,
    LiVESButtonsType buttons, const char *msg_fmt, ...) {
  LiVESWidget *mdial=NULL;
#ifdef GUI_GTK
  mdial=gtk_message_dialog_new(parent,flags,type,buttons,msg_fmt,NULL);
#endif
#ifdef GUI_QT
  LiVESMessageDialog *xmdial = new LiVESMessageDialog;
  xmdial->setIcon(type);
  mdial = static_cast<LiVESWidget *>(xmdial);
#endif
  return mdial;
}


LIVES_INLINE double lives_ruler_get_value(LiVESRuler *ruler) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  return gtk_range_get_value(GTK_RANGE(ruler));
#else
  return ruler->position;
#endif
#endif
#ifdef GUI_QT
  return ruler->value();
#endif
  return 0.;
}


LIVES_INLINE double lives_ruler_set_value(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_range_set_value(GTK_RANGE(ruler),value);
#else
  ruler->position=value;
#endif
#endif
#ifdef GUI_QT
  ruler->setValue(value);
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
#ifdef GUI_QT
  ruler->setMaximum(value);
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
#ifdef GUI_QT
  ruler->setMinimum(value);
#endif
  return value;
}


LIVES_INLINE LiVESCellRenderer *lives_cell_renderer_text_new(void) {
  LiVESCellRenderer *renderer=NULL;
#ifdef GUI_GTK
  renderer=gtk_cell_renderer_text_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_TEXT);
#endif
  return renderer;
}


LIVES_INLINE LiVESCellRenderer *lives_cell_renderer_spin_new(void) {
  LiVESCellRenderer *renderer=NULL;
#ifdef GUI_GTK
  renderer=gtk_cell_renderer_spin_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_SPIN);
#endif
  return renderer;
}


LIVES_INLINE LiVESCellRenderer *lives_cell_renderer_toggle_new(void) {
  LiVESCellRenderer *renderer=NULL;
#ifdef GUI_GTK
  renderer=gtk_cell_renderer_toggle_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_TOGGLE);
#endif
  return renderer;
}



LIVES_INLINE LiVESCellRenderer *lives_cell_renderer_pixbuf_new(void) {
  LiVESCellRenderer *renderer=NULL;
#ifdef GUI_GTK
  renderer=gtk_cell_renderer_pixbuf_new();
#endif
#ifdef GUI_QT
  renderer = new LiVESCellRenderer(LIVES_CELL_RENDERER_PIXBUF);
#endif
  return renderer;
}




LIVES_INLINE LiVESWidget *lives_toolbar_new(void) {
  LiVESWidget *toolbar=NULL;
#ifdef GUI_GTK
  toolbar=gtk_toolbar_new();
#endif
#ifdef GUI_QT
  toolbar = new LiVESToolbar;
#endif
  return toolbar;
}



LIVES_INLINE boolean lives_toolbar_insert(LiVESToolbar *toolbar, LiVESToolItem *item, int pos) {
#ifdef GUI_GTK
  gtk_toolbar_insert(toolbar,item,pos);
  return TRUE;
#endif
#ifdef GUI_QT
  QAction *act;
  int naction = toolbar->num_actions();
  if (pos >= naction) {
    act = toolbar->addWidget(static_cast<QWidget *>(item));
  } else {
    QAction *bef = toolbar->get_action(pos);
    act = toolbar->insertWidget(bef, static_cast<QWidget *>(item));
  }
  toolbar->add_action(act, pos);
  toolbar->add_child(item);
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
#ifdef GUI_QT
  return toolbar->iconSize();
#endif
  return LIVES_ICON_SIZE_INVALID;
}



LIVES_INLINE boolean lives_toolbar_set_icon_size(LiVESToolbar *toolbar, LiVESIconSize icon_size) {
#ifdef GUI_GTK
  gtk_toolbar_set_icon_size(toolbar,icon_size);
  return TRUE;
#endif
#ifdef GUI_QT
  toolbar->setIconSize(icon_size);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_toolbar_set_style(LiVESToolbar *toolbar, LiVESToolbarStyle style) {
#ifdef GUI_GTK
  gtk_toolbar_set_style(toolbar,style);
  return TRUE;
#endif
#ifdef GUI_QT
  toolbar->setToolButtonStyle(style);
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
#ifdef GUI_QT
  QPoint p(0,0);
  p = widget->mapToGlobal(p);
  x = p.x();
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
#ifdef GUI_QT
  QPoint p(0,0);
  p = widget->mapToGlobal(p);
  y = p.y();
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
#ifdef GUI_QT
  width = widget->size().width();
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
#ifdef GUI_QT
  height = widget->size().height();
#endif
  return height;
}


LIVES_INLINE boolean lives_widget_set_state(LiVESWidget *widget, LiVESWidgetState state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  gtk_widget_set_state_flags(widget,state,TRUE);
#else
  gtk_widget_set_state(widget,state);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  widget->set_state(state);
  return TRUE;
#endif
  return FALSE;
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

#ifdef GUI_QT
  return widget->get_state();
#else
  return (LiVESWidgetState)0;
#endif


}



LIVES_INLINE LiVESWidget *lives_bin_get_child(LiVESBin *bin) {
  LiVESWidget *child=NULL;
#ifdef GUI_GTK
  child=gtk_bin_get_child(bin);
#endif
#ifdef GUI_QT
  return bin->get_child(0);
#endif
#ifdef GUI_QT
  if (LIVES_IS_SCROLLED_WINDOW(bin)) return bin;
  return bin->get_child(0);
#endif
  return child;
}



LIVES_INLINE double lives_adjustment_get_upper(LiVESAdjustment *adj) {
  double upper=0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  upper=gtk_adjustment_get_upper(adj);
#else
  upper=adj->upper;
#endif
#endif
#ifdef GUI_QT
  upper = adj->get_upper();
#endif
  return upper;
}


LIVES_INLINE double lives_adjustment_get_lower(LiVESAdjustment *adj) {
  double lower=0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  lower=gtk_adjustment_get_lower(adj);
#else
  lower=adj->lower;
#endif
#endif
#ifdef GUI_QT
  lower = adj->get_lower();
#endif
  return lower;
}


LIVES_INLINE double lives_adjustment_get_page_size(LiVESAdjustment *adj) {
  double page_size=0.;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  page_size=gtk_adjustment_get_page_size(adj);
#else
  page_size=adj->page_size;
#endif
#endif
#ifdef GUI_QT
  page_size = adj->get_page_size();
#endif
  return page_size;
}


LIVES_INLINE double lives_adjustment_get_value(LiVESAdjustment *adj) {
  double value=0.;
#ifdef GUI_GTK
  value=gtk_adjustment_get_value(adj);
#endif
#ifdef GUI_QT
  value = adj->get_value();
#endif
  return value;
}


LIVES_INLINE boolean lives_adjustment_set_upper(LiVESAdjustment *adj, double upper) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_set_upper(adj,upper);
#else
  adj->upper=upper;
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_upper(upper);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_adjustment_set_lower(LiVESAdjustment *adj, double lower) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_set_lower(adj,lower);
#else
  adj->lower=lower;
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_lower(lower);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_adjustment_set_page_size(LiVESAdjustment *adj, double page_size) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_set_page_size(adj,page_size);
#else
  adj->page_size=page_size;
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_page_size(page_size);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_adjustment_set_value(LiVESAdjustment *adj, double value) {
#ifdef GUI_GTK
  gtk_adjustment_set_value(adj,value);
  return TRUE;
#endif
#ifdef GUI_QT
  adj->set_value(value);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_adjustment_clamp_page(LiVESAdjustment *adj, double lower, double upper) {
#ifdef GUI_GTK
  gtk_adjustment_clamp_page(adj,lower,upper);
  return TRUE;
#endif
#ifdef GUI_QT
  if (adj->get_upper() > adj->get_value() + adj->get_page_size())
    adj->set_value(adj->get_upper() - adj->get_page_size());
  if (adj->get_lower() < adj->get_value()) adj->set_value(adj->get_lower());

  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESAdjustment *lives_range_get_adjustment(LiVESRange *range) {
  LiVESAdjustment *adj=NULL;
#ifdef GUI_GTK
  adj=gtk_range_get_adjustment(range);
#endif
#ifdef GUI_QT
  adj = range->get_adj();
#endif
  return adj;
}


LIVES_INLINE boolean lives_range_set_value(LiVESRange *range, double value) {
#ifdef GUI_GTK
  gtk_range_set_value(range,value);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_range_set_range(LiVESRange *range, double min, double max) {
#ifdef GUI_GTK
  gtk_range_set_range(range,min,max);
  return TRUE;
#endif
#ifdef GUI_QT
  double newval;
  LiVESAdjustment *adj = range->get_adj();
  if (adj->get_value() < min) newval = min;
  if (adj->get_value() > max) newval = max;
  adj->set_lower(min);
  adj->set_upper(max);
  adj->set_value(newval);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_range_set_increments(LiVESRange *range, double step, double page) {
#ifdef GUI_GTK
  gtk_range_set_increments(range,step,page);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESAdjustment *adj = range->get_adj();
  adj->set_step_increment(step);
  adj->set_page_increment(page);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_range_set_inverted(LiVESRange *range, boolean invert) {
#ifdef GUI_GTK
  gtk_range_set_inverted(range,invert);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_SCALE(range)) {
    (static_cast<QAbstractSlider *>(static_cast<LiVESScale *>(range)))->setInvertedAppearance(invert);
  } else {
    (static_cast<QAbstractSlider *>(static_cast<LiVESScrollbar *>(range)))->setInvertedAppearance(invert);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE double lives_range_get_value(LiVESRange *range) {
  double value=0.;
#ifdef GUI_GTK
  value=gtk_range_get_value(range);
#endif
#ifdef GUI_QT
  value = range->get_adj()->get_value();
#endif
  return value;
}


LIVES_INLINE boolean lives_tree_model_get(LiVESTreeModel *tmod, LiVESTreeIter *titer, ...) {
  boolean res=FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_tree_model_get_valist(tmod,titer,argList);
  res=TRUE;
#endif
#ifdef GUI_QT
  // get 1 or more cells in row refd by titer
  // we have col number, locn to store value in
  char *attribute=va_arg(argList, char *);
  QVariant qv;

  while (attribute != NULL) {
    int colnum = va_arg(argList, int);
    int coltype = tmod->get_coltype(colnum);

    // types may be STRING, INT, (BOOLEAN, UINT, PIXBUF)
    if (coltype == LIVES_COL_TYPE_INT) {
      int *iattr = va_arg(argList, int *);
      qv = titer->data(colnum, Qt::DisplayRole);
      *iattr = qv.value<int>();
    }
    if (coltype == LIVES_COL_TYPE_STRING) {
      char **cattr = va_arg(argList, char **);
      qv = titer->data(colnum, Qt::DisplayRole);
      *cattr = strdup(qv.value<QString>().toUtf8().constData());
    }
    attribute=va_arg(argList, char *);
  }

#endif
  va_end(argList);
  return res;
}


LIVES_INLINE boolean lives_tree_model_get_iter(LiVESTreeModel *tmod, LiVESTreeIter *titer, LiVESTreePath *tpath) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter(tmod,titer,tpath);
#endif
#ifdef GUI_QT
  int *indices = tpath->get_indices();
  int cnt = tpath->get_depth();

  QTreeWidgetItem *qtwi = tmod->get_qtree_widget()->invisibleRootItem();

  for (int i=0; i < cnt; i++) {
    qtwi = qtwi->child(indices[i]);
  }

  *titer = *qtwi;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tree_model_get_iter_first(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_get_iter_first(tmod,titer);
#endif
#ifdef GUI_QT
  QTreeWidgetItem *qtwi = tmod->get_qtree_widget()->invisibleRootItem();
  qtwi = qtwi->child(0);
  *titer = *qtwi;
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESTreePath *lives_tree_model_get_path(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
  LiVESTreePath *tpath=NULL;
#ifdef GUI_GTK
  tpath=gtk_tree_model_get_path(tmod,titer);
#endif
#ifdef GUI_QT
  QList<int> qli;
  QTreeWidgetItem *qtw = titer;
  int idx;
  QTreeWidget *twidget = tmod->get_qtree_widget();

  while (1) {
    if (qtw->parent() == 0) {
      idx = twidget->indexOfTopLevelItem(qtw);
      qli.append(idx);
      break;
    }
    idx = qtw->parent()->indexOfChild(qtw);
    qli.append(idx);
  }

  tpath = new LiVESTreePath(qli);

#endif
  return tpath;
}


LIVES_INLINE boolean lives_tree_model_iter_children(LiVESTreeModel *tmod, LiVESTreeIter *titer, LiVESTreeIter *parent) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_children(tmod,titer,parent);
#endif
#ifdef GUI_QT
  if (parent == NULL || parent == tmod->get_qtree_widget()->invisibleRootItem()) {
    QTreeWidget *tw = tmod->get_qtree_widget();
    if (tw->topLevelItemCount() == 0) return FALSE;
    *titer = *tw->topLevelItem(0);
    return TRUE;
  }
  if (parent->childCount() == 0) return FALSE;
  *titer = *parent->child(0);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE int lives_tree_model_iter_n_children(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_n_children(tmod,titer);
#endif
#ifdef GUI_QT
  if (titer == NULL || titer == tmod->get_qtree_widget()->invisibleRootItem()) {
    // return num toplevel
    return tmod->get_qtree_widget()->topLevelItemCount();
  }
  return titer->childCount();
#endif
  return 0;
}


LIVES_INLINE boolean lives_tree_model_iter_next(LiVESTreeModel *tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_model_iter_next(tmod,titer);
#endif
#ifdef GUI_QT
  QTreeWidgetItem *parent = titer->parent();
  if (parent == NULL || parent == tmod->get_qtree_widget()->invisibleRootItem()) {
    QTreeWidget *tw = tmod->get_qtree_widget();
    int idx = tw->indexOfTopLevelItem(titer) + 1;
    if (idx >= tw->topLevelItemCount()) {
      return FALSE;
    }
    *titer = *(tw->topLevelItem(idx));
    return TRUE;
  }
  int idx = parent->indexOfChild(titer) + 1;
  if (idx >= parent->childCount()) {
    return FALSE;
  }
  *titer = *(parent->child(idx));
  return TRUE;

#endif
  return FALSE;
}


LIVES_INLINE boolean lives_tree_path_free(LiVESTreePath *tpath) {
#ifdef GUI_GTK
  gtk_tree_path_free(tpath);
  return TRUE;
#endif
#ifdef GUI_QT
  delete tpath;
#endif
  return FALSE;
}


LIVES_INLINE LiVESTreePath *lives_tree_path_new_from_string(const char *path) {
  LiVESTreePath *tpath=NULL;
#ifdef GUI_GTK
  tpath=gtk_tree_path_new_from_string(path);
#endif
#ifdef GUI_QT
  tpath = new LiVESTreePath(path);
#endif
  return tpath;
}



LIVES_INLINE int lives_tree_path_get_depth(LiVESTreePath *tpath) {
  int depth=-1;
#ifdef GUI_GTK
  depth=gtk_tree_path_get_depth(tpath);
#endif
#ifdef GUI_QT
  return tpath->get_depth();
#endif
  return depth;
}



LIVES_INLINE int *lives_tree_path_get_indices(LiVESTreePath *tpath) {
  int *indices=NULL;
#ifdef GUI_GTK
  indices=gtk_tree_path_get_indices(tpath);
#endif
#ifdef GUI_QT
  indices = tpath->get_indices();
#endif
  return indices;
}



LIVES_INLINE LiVESTreeStore *lives_tree_store_new(int ncols, ...) {
  LiVESTreeStore *tstore=NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols>0) {
    GType types[ncols];
    register int i;
    for (i=0; i<ncols; i++) {
      types[i]=va_arg(argList, long unsigned int);
    }
    tstore=gtk_tree_store_newv(ncols,types);
  }
#endif

#ifdef GUI_QT
  if (ncols > 0) {
    QModelIndex qmi = QModelIndex();
    int types[ncols];

    for (int i=0; i < ncols; i++) {
      types[i]=va_arg(argList, long unsigned int);
    }

    tstore = new LiVESTreeStore(ncols, types);
    tstore->insertColumns(0, ncols, qmi);
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
#ifdef GUI_QT
  QVariant qv = (static_cast<QAbstractItemModel *>(tstore))->property("LiVESObject");
  if (qv.isValid()) {

    LiVESTreeModel *ltm = static_cast<LiVESTreeModel *>(qv.value<LiVESObject *>());
    QTreeWidget *qtw = ltm->get_qtree_widget();

    if (parent != NULL) {
      QTreeWidgetItem *qtwi = new QTreeWidgetItem(parent);
      *titer = *qtwi;
      return TRUE;
    } else {
      QTreeWidgetItem *qtwi = new QTreeWidgetItem(qtw);
      *titer = *qtwi;
      return TRUE;
    }
  }
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
#ifdef GUI_QT
  // set a row in the treestore
  QVariant qv;

  while (1) {
    int colnum = va_arg(argList, long unsigned int);
    if (colnum == -1) break;

    int coltype = tstore->get_coltype(colnum);

    // types may be STRING, INT, (BOOLEAN, UINT, PIXBUF)
    if (coltype == LIVES_COL_TYPE_INT) {
      int iattr = va_arg(argList, int);
      qv = QVariant::fromValue(iattr);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
    if (coltype == LIVES_COL_TYPE_STRING) {
      char *cattr = va_arg(argList, char *);
      QString qs = QString::fromUtf8(cattr);
      qv = QVariant::fromValue(qs);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
  }

#endif
  va_end(argList);
  return res;
}



LIVES_INLINE LiVESWidget *lives_tree_view_new_with_model(LiVESTreeModel *tmod) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_tree_view_new_with_model(tmod);
#endif
#ifdef GUI_QT
  LiVESTreeView *trview = new LiVESTreeView;
  trview->set_model(tmod);
  tview = static_cast<LiVESWidget *>(trview);
#endif
  return tview;
}



LIVES_INLINE LiVESWidget *lives_tree_view_new(void) {
  LiVESWidget *tview=NULL;
#ifdef GUI_GTK
  tview=gtk_tree_view_new();
#endif
#ifdef GUI_QT
  tview = new LiVESTreeView;
#endif
  return tview;
}





LIVES_INLINE boolean lives_tree_view_set_model(LiVESTreeView *tview, LiVESTreeModel *tmod) {
#ifdef GUI_GTK
  gtk_tree_view_set_model(tview,tmod);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->set_model(tmod);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESTreeModel *lives_tree_view_get_model(LiVESTreeView *tview) {
  LiVESTreeModel *tmod=NULL;
#ifdef GUI_GTK
  tmod=gtk_tree_view_get_model(tview);
#endif
#ifdef GUI_QT
  tmod = tview->get_model();
#endif
  return tmod;
}



LIVES_INLINE LiVESTreeSelection *lives_tree_view_get_selection(LiVESTreeView *tview) {
  LiVESTreeSelection *tsel=NULL;
#ifdef GUI_GTK
  tsel=gtk_tree_view_get_selection(tview);
#endif
#ifdef GUI_QT
  tsel = tview;
#endif
  return tsel;
}



LIVES_INLINE int lives_tree_view_append_column(LiVESTreeView *tview, LiVESTreeViewColumn *tvcol) {
#ifdef GUI_GTK
  gtk_tree_view_append_column(tview,tvcol);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->append_column(tvcol);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_view_set_headers_visible(LiVESTreeView *tview, boolean vis) {
#ifdef GUI_GTK
  gtk_tree_view_set_headers_visible(tview,vis);
  return TRUE;
#endif
#ifdef GUI_QT
  tview->setHeaderHidden(!vis);
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
#ifdef GUI_QT
  adj = tview->get_hadj();
#endif
  return adj;
}




LIVES_INLINE LiVESTreeViewColumn *lives_tree_view_column_new_with_attributes(const char *title, LiVESCellRenderer *crend, ...) {
  LiVESTreeViewColumn *tvcol=NULL;
  va_list args;
  va_start(args, crend);
  int column;
  char *attribute;
  boolean expand=FALSE;
#ifdef GUI_GTK

  tvcol=gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(tvcol,title);
  gtk_tree_view_column_pack_start(tvcol,crend,expand);

  attribute=va_arg(args, char *);

  while (attribute != NULL) {
    column = va_arg(args, int);
    gtk_tree_view_column_add_attribute(tvcol, crend, attribute, column);
    attribute = va_arg(args, char *);
  }

#endif
#ifdef GUI_QT
  tvcol = crend;
  tvcol->set_title(title);
  tvcol->set_expand(expand);
  while (attribute != NULL) {
    column = va_arg(args, int);
    tvcol->add_attribute(attribute, column);
    attribute = va_arg(args, char *);
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
#ifdef GUI_QT
  tvcol->set_sizing(type);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_view_column_set_fixed_width(LiVESTreeViewColumn *tvcol, int fwidth) {
#ifdef GUI_GTK
  gtk_tree_view_column_set_fixed_width(tvcol,fwidth);
  return TRUE;
#endif
#ifdef GUI_QT
  tvcol->set_fixed_width(fwidth);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_selection_get_selected(LiVESTreeSelection *tsel, LiVESTreeModel **tmod, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  return gtk_tree_selection_get_selected(tsel,tmod,titer);
#endif
#ifdef GUI_QT
  int mode = tsel->selectionMode();
  if (mode == LIVES_SELECTION_MULTIPLE) return TRUE;
  // tsel should have single seln.
  QList<QTreeWidgetItem *> qtwi = tsel->selectedItems();
  *titer = *(qtwi.at(0));
  *tmod = tsel->get_model();
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_selection_set_mode(LiVESTreeSelection *tsel, LiVESSelectionMode tselmod) {
#ifdef GUI_GTK
  gtk_tree_selection_set_mode(tsel,tselmod);
  return TRUE;
#endif
#ifdef GUI_QT
  tsel->setSelectionMode(tselmod);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_tree_selection_select_iter(LiVESTreeSelection *tsel, LiVESTreeIter *titer) {
#ifdef GUI_GTK
  gtk_tree_selection_select_iter(tsel,titer);
  return TRUE;
#endif
#ifdef GUI_QT
  titer->setSelected(true);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESListStore *lives_list_store_new(int ncols, ...) {
  LiVESListStore *lstore=NULL;
  va_list argList;
  va_start(argList, ncols);
#ifdef GUI_GTK
  if (ncols>0) {
    GType types[ncols];
    register int i;
    for (i=0; i<ncols; i++) {
      types[i]=va_arg(argList, long unsigned int);
    }
    lstore=gtk_list_store_newv(ncols,types);
  }
#endif

#ifdef GUI_QT
  if (ncols > 0) {
    QModelIndex qmi = QModelIndex();
    int types[ncols];

    for (int i=0; i < ncols; i++) {
      types[i]=va_arg(argList, long unsigned int);
    }

    lstore = new LiVESListStore(ncols, types);
    lstore->insertColumns(0, ncols, qmi);
  }
#endif
  va_end(argList);
  return lstore;
}



LIVES_INLINE boolean lives_list_store_set(LiVESListStore *lstore, LiVESTreeIter *titer, ...) {
  boolean res=FALSE;
  va_list argList;
  va_start(argList, titer);
#ifdef GUI_GTK
  gtk_list_store_set_valist(lstore,titer,argList);
  res=TRUE;
#endif
#ifdef GUI_QT
  // set a row in the liststore
  QVariant qv;

  while (1) {
    int colnum = va_arg(argList, int);
    if (colnum == -1) break;

    int coltype = lstore->get_coltype(colnum);

    // types may be STRING, INT, (BOOLEAN, UINT, PIXBUF)
    if (coltype == LIVES_COL_TYPE_INT) {
      int iattr = va_arg(argList, long unsigned int);
      qv = QVariant::fromValue(iattr);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
    if (coltype == LIVES_COL_TYPE_STRING) {
      char *cattr = va_arg(argList, char *);
      QString qs = QString::fromUtf8(cattr);
      qv = QVariant::fromValue(qs);
      titer->setData(colnum, Qt::DisplayRole, qv);
    }
  }

#endif
  va_end(argList);
  return res;
}


LIVES_INLINE boolean lives_list_store_insert(LiVESListStore *lstore, LiVESTreeIter *titer, int position) {
#ifdef GUI_GTK
  gtk_list_store_insert(lstore,titer,position);
  return TRUE;
#endif
#ifdef GUI_QT
  lstore->insertRow(position);
  QTreeWidgetItem *qtwi = qtwi->child(position);
  *titer = *qtwi;
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE const char *lives_label_get_text(LiVESLabel *label) {
#ifdef GUI_GTK
  return gtk_label_get_text(label);
#endif
#ifdef GUI_QT
  return label->text().toUtf8().constData();
#endif
  return NULL;
}


LIVES_INLINE boolean lives_label_set_text(LiVESLabel *label, const char *text) {
#ifdef GUI_GTK
  gtk_label_set_text(label,text);
  return TRUE;
#endif
#ifdef GUI_QT
  label->setTextFormat(Qt::PlainText);
  label->setText(QString::fromUtf8(text));
  label->set_text(QString::fromUtf8(text));
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_label_set_text_with_mnemonic(LiVESLabel *label, const char *text) {
#ifdef GUI_GTK
  gtk_label_set_text_with_mnemonic(label,text);
  return TRUE;
#endif
#ifdef GUI_QT
  label->setTextFormat(Qt::PlainText);
  label->setText(qmake_mnemonic(QString::fromUtf8(text)));
  label->set_text(qmake_mnemonic(QString::fromUtf8(text)));
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_label_set_markup(LiVESLabel *label, const char *markup) {
#ifdef GUI_GTK
  gtk_label_set_markup(label,markup);
  return TRUE;
#endif
#ifdef GUI_QT
  label->setTextFormat(Qt::RichText);
  label->setText(QString::fromUtf8(markup));
  label->set_text(QString::fromUtf8(markup));
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_label_set_markup_with_mnemonic(LiVESLabel *label, const char *markup) {
#ifdef GUI_GTK
  gtk_label_set_markup_with_mnemonic(label,markup);
  return TRUE;
#endif
#ifdef GUI_QT
  label->setTextFormat(Qt::RichText);
  label->setText(qmake_mnemonic(QString::fromUtf8(markup)));
  label->set_text(qmake_mnemonic(QString::fromUtf8(markup)));
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_label_set_mnemonic_widget(LiVESLabel *label, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_label_set_mnemonic_widget(label,widget);
  return TRUE;
#endif
#ifdef GUI_QT
  label->set_mnemonic_widget(widget);
  (static_cast<QLabel *>(label))->setBuddy(static_cast<QWidget *>(widget));
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_label_get_mnemonic_widget(LiVESLabel *label) {
  LiVESWidget *widget=NULL;
#ifdef GUI_GTK
  widget=gtk_label_get_mnemonic_widget(label);
#endif
#ifdef GUI_QT
  widget = label->get_mnemonic_widget();
#endif
  return widget;
}



LIVES_INLINE boolean lives_label_set_selectable(LiVESLabel *label, boolean setting) {
#ifdef GUI_GTK
  gtk_label_set_selectable(label,setting);
  return TRUE;
#endif
#ifdef GUI_QT
  QLabel *qlabel = static_cast<QLabel *>(label);
  Qt::TextInteractionFlags flags = qlabel->textInteractionFlags();
  if (setting) {
    flags |= Qt::TextSelectableByMouse;
    flags |= Qt::TextSelectableByKeyboard;
  } else {
    if (flags & Qt::TextSelectableByMouse) flags ^= Qt::TextSelectableByMouse;
    if (flags & Qt::TextSelectableByKeyboard) flags ^= Qt::TextSelectableByKeyboard;
  }
  qlabel->setTextInteractionFlags(flags);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_editable_set_editable(LiVESEditable *editable, boolean is_editable) {
#ifdef GUI_GTK
  gtk_editable_set_editable(editable,is_editable);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_LABEL(editable)) {
    QLabel *qlabel = dynamic_cast<QLabel *>(editable);
    Qt::TextInteractionFlags flags = qlabel->textInteractionFlags();
    if (is_editable) {
      flags |= Qt::TextEditable;
    } else {
      if (flags & Qt::TextEditable) flags ^= Qt::TextEditable;
    }
    qlabel->setTextInteractionFlags(flags);
  } else {
    QAbstractSpinBox *qsb = dynamic_cast<QAbstractSpinBox *>(editable);
    qsb->setReadOnly(!is_editable);
  }
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_editable_select_region(LiVESEditable *editable, int start_pos, int end_pos) {
#ifdef GUI_GTK
  gtk_editable_select_region(editable,start_pos,end_pos);
  return TRUE;
#endif
#ifdef GUI_QT
  if (LIVES_IS_LABEL(editable)) {
    QLabel *qlabel = dynamic_cast<QLabel *>(editable);
    qlabel->setSelection(start_pos, end_pos);
  }
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_entry_new(void) {
  LiVESWidget *entry=NULL;
#ifdef GUI_GTK
  entry=gtk_entry_new();
#endif
#ifdef GUI_QT
  entry = new LiVESEntry();
#endif
  return entry;
}




LIVES_INLINE boolean lives_entry_set_max_length(LiVESEntry *entry, int len) {
#ifdef GUI_GTK
  gtk_entry_set_max_length(entry,len);
  return TRUE;
#endif
#ifdef GUI_QT
  entry->setMaxLength(len);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_activates_default(LiVESEntry *entry, boolean act) {
#ifdef GUI_GTK
  gtk_entry_set_activates_default(entry,act);
  return TRUE;
#endif
#ifdef GUI_QT
  // do nothing, enter is picked up by dialog
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_visibility(LiVESEntry *entry, boolean vis) {
#ifdef GUI_GTK
  gtk_entry_set_visibility(entry,vis);
  return TRUE;
#endif
#ifdef GUI_QT
  if (!vis) {
    entry->setEchoMode(QLineEdit::Password);
  } else {
    entry->setEchoMode(QLineEdit::Normal);
  }
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_has_frame(LiVESEntry *entry, boolean has) {
#ifdef GUI_GTK
  gtk_entry_set_has_frame(entry,has);
  return TRUE;
#endif
#ifdef GUI_QT
  entry->setFrame(has);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE const char *lives_entry_get_text(LiVESEntry *entry) {
#ifdef GUI_GTK
  return gtk_entry_get_text(entry);
#endif
#ifdef GUI_QT
  return entry->text().toUtf8().constData();
#endif
  return NULL;
}


LIVES_INLINE boolean lives_entry_set_text(LiVESEntry *entry, const char *text) {
#ifdef GUI_GTK
  gtk_entry_set_text(entry,text);
  return TRUE;
#endif
#ifdef GUI_QT
  entry->setText(QString::fromUtf8(text));
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_entry_set_width_chars(LiVESEntry *entry, int nchars) {
#ifdef GUI_GTK
  gtk_entry_set_width_chars(entry,nchars);
  return TRUE;
#endif
#ifdef GUI_QT
  QFontMetrics metrics(QApplication::font());
  (static_cast<QLineEdit *>(entry))->setFixedSize(metrics.width('A')*nchars,
      (static_cast<QWidget *>(static_cast<QLineEdit *>(entry)))->height());
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_scrolled_window_new(LiVESAdjustment *hadj, LiVESAdjustment *vadj) {
  LiVESWidget *swindow=NULL;
#ifdef GUI_GTK
  swindow=gtk_scrolled_window_new(hadj,vadj);
#endif
#ifdef GUI_QT
  swindow = new LiVESScrolledWindow(hadj, vadj);
#endif
  return swindow;
}


LIVES_INLINE LiVESAdjustment *lives_scrolled_window_get_hadjustment(LiVESScrolledWindow *swindow) {
  LiVESAdjustment *adj=NULL;
#ifdef GUI_GTK
  adj=gtk_scrolled_window_get_hadjustment(swindow);
#endif
#ifdef GUI_QT
  adj = swindow->get_hadj();
#endif
  return adj;
}


LIVES_INLINE LiVESAdjustment *lives_scrolled_window_get_vadjustment(LiVESScrolledWindow *swindow) {
  LiVESAdjustment *adj=NULL;
#ifdef GUI_GTK
  adj=gtk_scrolled_window_get_vadjustment(swindow);
#endif
#ifdef GUI_QT
  adj = swindow->get_vadj();
#endif
  return adj;
}


LIVES_INLINE boolean lives_scrolled_window_set_policy(LiVESScrolledWindow *scrolledwindow, LiVESPolicyType hpolicy,
    LiVESPolicyType vpolicy) {
#ifdef GUI_GTK
  gtk_scrolled_window_set_policy(scrolledwindow, hpolicy, vpolicy);
  return TRUE;
#endif
#ifdef GUI_QT
  scrolledwindow->setHorizontalScrollBarPolicy(hpolicy);
  scrolledwindow->setVerticalScrollBarPolicy(hpolicy);
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
#ifdef GUI_QT
  scrolledwindow->setWidget(static_cast<QWidget *>(child));
  scrolledwindow->add_child(child);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_xwindow_raise(LiVESXWindow *xwin) {
#ifdef GUI_GTK
  gdk_window_raise(xwin);
  return TRUE;
#endif
#ifdef GUI_QT
  xwin->raise();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_xwindow_set_cursor(LiVESXWindow *xwin, LiVESXCursor *cursor) {
#ifdef GUI_GTK
  gdk_window_set_cursor(xwin,cursor);
  return TRUE;
#endif
#ifdef GUI_QT
  if (cursor != NULL)
    xwin->setCursor(*cursor);
  else
    xwin->unsetCursor();
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
#ifdef GUI_QT
  return TRUE;
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
#ifdef GUI_QT
  widget->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Preferred);
  return TRUE;
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
#ifdef GUI_QT
  widget->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::MinimumExpanding);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_menu_new(void) {
  LiVESWidget *menu=NULL;
#ifdef GUI_GTK
  menu=gtk_menu_new();
#endif
#ifdef GUI_QT
  menu = new LiVESMenu;
#endif
  return menu;
}


LIVES_INLINE LiVESWidget *lives_menu_bar_new(void) {
  LiVESWidget *menubar=NULL;
#ifdef GUI_GTK
  menubar=gtk_menu_bar_new();
#endif
#ifdef GUI_QT
  menubar = new LiVESMenuBar;
#endif
  return menubar;
}


LIVES_INLINE LiVESWidget *lives_menu_item_new(void) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_menu_item_new();
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(mainw->LiVES);
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
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(qmake_mnemonic(QString::fromUtf8(label)),mainw->LiVES);
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
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(QString::fromUtf8(label),mainw->LiVES);
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
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(QString::fromUtf8(label),mainw->LiVES);
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
#ifdef GUI_QT
  menuitem = new LiVESMenuItem(qmake_mnemonic(QString::fromUtf8(label)),mainw->LiVES);
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
#ifdef GUI_QT
  QActionGroup *qag;
  LiVESRadioMenuItem *xmenuitem = new LiVESRadioMenuItem(QString::fromUtf8(label),mainw->LiVES);
  if (group == NULL) {
    qag = new QActionGroup(NULL);
    group = lives_slist_append(group, (void *)qag);
    qag->setExclusive(true);
  } else {
    qag = const_cast<QActionGroup *>(static_cast<const QActionGroup *>(lives_slist_nth_data(group, 0)));
  }

  xmenuitem->set_group(group);
  qag->addAction(static_cast<QAction *>(xmenuitem));
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
  menuitem = static_cast<LiVESWidget *>(xmenuitem);
#endif
  return menuitem;
}



LIVES_INLINE LiVESSList *lives_radio_menu_item_get_group(LiVESRadioMenuItem *rmenuitem) {
#ifdef GUI_GTK
  return gtk_radio_menu_item_get_group(rmenuitem);
#endif
#ifdef GUI_QT
  return rmenuitem->get_list();
#endif
  return NULL;
}



LIVES_INLINE LiVESWidget *lives_check_menu_item_new_with_label(const char *label) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
  menuitem=gtk_check_menu_item_new_with_label(label);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
#ifdef GUI_QT
  menuitem = new LiVESCheckMenuItem(QString::fromUtf8(label),mainw->LiVES);
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
#ifdef GUI_QT
  menuitem = new LiVESCheckMenuItem(qmake_mnemonic(QString::fromUtf8(label)),mainw->LiVES);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE boolean lives_check_menu_item_set_draw_as_radio(LiVESCheckMenuItem *item, boolean setting) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_draw_as_radio(item,setting);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESWidget *lives_image_menu_item_new_from_stock(const char *stock_id, LiVESAccelGroup *accel_group) {
  LiVESWidget *menuitem=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,10,0)
  char *xstock_id=lives_strdup(stock_id); // need to back this up as we will use translation functions
  menuitem=gtk_menu_item_new_with_mnemonic(xstock_id);

  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_SAVE)) {
    gtk_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem),"<LiVES>/save");
  }

  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_QUIT)) {
    gtk_menu_item_set_accel_path(LIVES_MENU_ITEM(menuitem),"<LiVES>/quit");
  }
  lives_free(xstock_id);
#else
  menuitem=gtk_image_menu_item_new_from_stock(stock_id,accel_group);
#endif
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
#ifdef GUI_QT
  char *xstock_id=lives_strdup(stock_id); // need to back this up as we will use translation functions
  LiVESMenuItem *xmenuitem = new LiVESMenuItem(qmake_mnemonic(QString::fromUtf8(xstock_id)),mainw->LiVES);

  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_SAVE)) {
    xmenuitem->setShortcut(make_qkey_sequence(LIVES_KEY_s, LIVES_CONTROL_MASK));
  }

  if (!strcmp(xstock_id,LIVES_STOCK_LABEL_QUIT)) {
    xmenuitem->setMenuRole(QAction::QuitRole);
    xmenuitem->setShortcut(make_qkey_sequence(LIVES_KEY_q, LIVES_CONTROL_MASK));
  }
  lives_free(xstock_id);

  menuitem = static_cast<LiVESWidget *>(xmenuitem);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme2(menuitem, LIVES_WIDGET_STATE_INSENSITIVE);
  }
#endif
  return menuitem;
}


LIVES_INLINE LiVESToolItem *lives_menu_tool_button_new(LiVESWidget *icon, const char *label) {
  LiVESToolItem *toolitem=NULL;
#ifdef GUI_GTK
  toolitem=gtk_menu_tool_button_new(icon,label);
#endif
#ifdef GUI_QT
  toolitem = new LiVESMenuToolButton(QString::fromUtf8(label), mainw->LiVES, icon);
#endif
  return toolitem;
}



LIVES_INLINE boolean lives_menu_tool_button_set_menu(LiVESMenuToolButton *toolbutton, LiVESWidget *menu) {
#ifdef GUI_GTK
  gtk_menu_tool_button_set_menu(toolbutton,menu);
  return TRUE;
#endif
#ifdef GUI_QT
  (dynamic_cast<QMenu *>(menu))->addAction(static_cast<QAction *>(toolbutton));
  menu->add_child(toolbutton);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_menu_item_set_submenu(LiVESMenuItem *menuitem, LiVESWidget *menu) {
#ifdef GUI_GTK
  gtk_menu_item_set_submenu(menuitem,menu);

#ifdef GTK_SUBMENU_SENS_BUG
  if (!lives_widget_is_sensitive(LIVES_WIDGET(menuitem))) {
    g_print("Warning, adding submenu when insens!");
    //assert(FALSE);
  }
#endif

  return TRUE;
#endif
#ifdef GUI_QT
  (static_cast<QAction *>(menuitem))->setMenu(dynamic_cast<QMenu *>(menu));
  menuitem->add_child(menu);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_menu_item_activate(LiVESMenuItem *menuitem) {
#ifdef GUI_GTK
  gtk_menu_item_activate(menuitem);
  return TRUE;
#endif
#ifdef GUI_QT
  menuitem->activate(QAction::Trigger);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_check_menu_item_set_active(LiVESCheckMenuItem *item, boolean state) {
#ifdef GUI_GTK
  gtk_check_menu_item_set_active(item,state);
  return TRUE;
#endif
#ifdef GUI_QT
  item->setChecked(state);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_check_menu_item_get_active(LiVESCheckMenuItem *item) {
#ifdef GUI_GTK
  return gtk_check_menu_item_get_active(item);
#endif
#ifdef GUI_QT
  return item->isChecked();
#endif
  return FALSE;
}


#if !GTK_CHECK_VERSION(3,10,0)

LIVES_INLINE boolean lives_image_menu_item_set_image(LiVESImageMenuItem *item, LiVESWidget *image) {
#ifdef GUI_GTK
  gtk_image_menu_item_set_image(item,image);
  return TRUE;
#endif
#ifdef GUI_QT
  QImage *qim = dynamic_cast<QImage *>(image);
  if (qim != NULL) {
    QPixmap qpx;
    qpx.convertFromImage(*qim);
    QIcon *qi = new QIcon(qpx);
    item->setIcon(*qi);
  }
  return TRUE;
#endif
  return FALSE;
}

#endif

LIVES_INLINE boolean lives_menu_set_title(LiVESMenu *menu, const char *title) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,10,0)
  gtk_menu_set_title(menu,title);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  menu->setTitle(QString::fromUtf8(title));
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_menu_popup(LiVESMenu *menu, LiVESXEventButton *event) {
#ifdef GUI_GTK
  gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
  return TRUE;
#endif
#ifdef GUI_QT
  menu->popup((static_cast<QWidget *>(static_cast<LiVESWidget *>(menu)))->pos());
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_menu_reorder_child(LiVESMenu *menu, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_menu_reorder_child(menu,child,pos);
  return TRUE;
#endif
#ifdef GUI_QT
  menu->reorder_child(child, pos);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_menu_detach(LiVESMenu *menu) {
  // NB also calls detacher callback
#ifdef GUI_GTK
  gtk_menu_detach(menu);
  return TRUE;
#endif
#ifdef GUI_QT
  LiVESTornOffMenu *ltom = new LiVESTornOffMenu(menu);
  ltom->no_refcount();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_menu_shell_append(LiVESMenuShell *menushell, LiVESWidget *child) {
#ifdef GUI_GTK
  gtk_menu_shell_append(menushell,child);
  return TRUE;
#endif
#ifdef GUI_QT
  QAction *action = dynamic_cast<QAction *>(child);
  if (LIVES_IS_MENU(menushell)) {
    QMenu *qmenu = dynamic_cast<QMenu *>(menushell);
    qmenu->addAction(action);
  } else {
    QMenuBar *qmenu = dynamic_cast<QMenuBar *>(menushell);
    qmenu->addAction(action);
  }
  menushell->add_child(child);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_menu_shell_insert(LiVESMenuShell *menushell, LiVESWidget *child, int pos) {
#ifdef GUI_GTK
  gtk_menu_shell_insert(menushell,child,pos);
  return TRUE;
#endif
#ifdef GUI_QT
  lives_menu_shell_append(menushell, child);
  if (LIVES_IS_MENU(menushell))(static_cast<LiVESMenu *>(menushell))->reorder_child(child, pos);
  else (dynamic_cast<LiVESMenuBar *>(menushell))->reorder_child(child, pos);
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_menu_shell_prepend(LiVESMenuShell *menushell, LiVESWidget *child) {
#ifdef GUI_GTK
  gtk_menu_shell_prepend(menushell,child);
  return TRUE;
#endif
#ifdef GUI_QT
  return lives_menu_shell_insert(menushell, child, 0);
#endif
  return FALSE;
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
#ifdef GUI_QT
  item->setIconVisibleInMenu(show);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_scale_set_draw_value(LiVESScale *scale, boolean draw_value) {
#ifdef GUI_GTK
  gtk_scale_set_draw_value(scale,draw_value);
  return TRUE;
#endif
#ifdef GUI_QT
  return !draw_value;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_scale_set_value_pos(LiVESScale *scale, LiVESPositionType ptype) {
#ifdef GUI_GTK
  gtk_scale_set_value_pos(scale,ptype);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
  return FALSE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_scale_set_digits(LiVESScale *scale, int digits) {
#ifdef GUI_GTK
  gtk_scale_set_digits(scale,digits);
  return TRUE;
#endif
#ifdef GUI_QT
  // TODO
  return FALSE;
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
  gtk_scale_button_set_orientation(scale,orientation);
  return TRUE;
#endif
#endif
#endif
#ifdef GUI_QT
  scale->setOrientation(orientation);
  return TRUE;
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
#ifdef GUI_QT
  value = scale->value();
#endif
  return value;
}

LIVES_INLINE boolean lives_scale_button_set_value(LiVESScaleButton *scale, double value) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_scale_button_set_value(scale,value);
#else
  gtk_adjustment_set_value(gtk_range_get_adjustment(scale),value);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  value = scale->value();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE char *lives_file_chooser_get_filename(LiVESFileChooser *chooser) {
  char *fname=NULL;
#ifdef GUI_GTK
  fname=gtk_file_chooser_get_filename(chooser);
#endif
#ifdef GUI_QT
  QStringList qsl = chooser->selectedFiles();
  fname = strdup(qsl.at(0).toUtf8().constData());
#endif
  return fname;
}


LIVES_INLINE LiVESSList *lives_file_chooser_get_filenames(LiVESFileChooser *chooser) {
  LiVESSList *fnlist=NULL;
#ifdef GUI_GTK
  fnlist=gtk_file_chooser_get_filenames(chooser);
#endif
#ifdef GUI_QT
  QStringList qsl = chooser->selectedFiles();
  for (int i=0; i < qsl.size(); i++) {
    fnlist = lives_slist_append(fnlist,((livesconstpointer)(strdup(qsl.at(0).toUtf8().constData()))));
  }
#endif
  return fnlist;
}

#ifdef GUI_GTK
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
#endif
#endif

LIVES_INLINE LiVESWidget *lives_frame_new(const char *label) {
  LiVESWidget *frame=NULL;
#ifdef GUI_GTK
  frame=gtk_frame_new(label);
#endif
#ifdef GUI_QT
  frame = new LiVESFrame(label);
#endif
  return frame;
}



LIVES_INLINE boolean lives_frame_set_label(LiVESFrame *frame, const char *label) {
#ifdef GUI_GTK
  gtk_frame_set_label(frame,label);
  return TRUE;
#endif
#ifdef GUI_QT
  frame->set_label(QString::fromUtf8(label));
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_frame_set_label_widget(LiVESFrame *frame, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_frame_set_label_widget(frame,widget);
  return TRUE;
#endif
#ifdef GUI_QT
  frame->set_label_widget(widget);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_frame_get_label_widget(LiVESFrame *frame) {
  LiVESWidget *widget=NULL;
#ifdef GUI_GTK
  widget=gtk_frame_get_label_widget(frame);
#endif
#ifdef GUI_QT
  widget = frame->get_label_widget();
#endif
  return widget;
}



LIVES_INLINE boolean lives_frame_set_shadow_type(LiVESFrame *frame, LiVESShadowType stype) {
#ifdef GUI_GTK
  gtk_frame_set_shadow_type(frame,stype);
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE LiVESWidget *lives_notebook_new(void) {
  LiVESWidget *nbook=NULL;
#ifdef GUI_GTK
  nbook=gtk_notebook_new();
#endif
#ifdef GUI_QT
  nbook = new LiVESNotebook;
#endif
  return nbook;
}



LIVES_INLINE LiVESWidget *lives_notebook_get_nth_page(LiVESNotebook *nbook, int pagenum) {
  LiVESWidget *page=NULL;
#ifdef GUI_GTK
  page=gtk_notebook_get_nth_page(nbook,pagenum);
#endif
#ifdef GUI_QT
  QWidget *qwidget = nbook->widget(pagenum);
  QVariant qv = qwidget->property("LiVESObject");
  if (qv.isValid()) {
    page = static_cast<LiVESWidget *>(qv.value<LiVESObject *>());
  }
#endif
  return page;
}



LIVES_INLINE int lives_notebook_get_current_page(LiVESNotebook *nbook) {
  int pagenum=-1;
#ifdef GUI_GTK
  pagenum=gtk_notebook_get_current_page(nbook);
#endif
#ifdef GUI_QT
  pagenum = nbook->currentIndex();
#endif
  return pagenum;
}



LIVES_INLINE boolean lives_notebook_set_current_page(LiVESNotebook *nbook, int pagenum) {
#ifdef GUI_GTK
  gtk_notebook_set_current_page(nbook,pagenum);
  return TRUE;
#endif
#ifdef GUI_QT
  nbook->setCurrentIndex(pagenum);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_notebook_set_tab_label(LiVESNotebook *nbook, LiVESWidget *child, LiVESWidget *tablabel) {
#ifdef GUI_GTK
  gtk_notebook_set_tab_label(nbook,child,tablabel);
  return TRUE;
#endif
#ifdef GUI_QT
  nbook->set_tab_label(child, tablabel);
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

  for (i=0; i<rows; i++) {
    gtk_grid_insert_row(grid,0);
  }

  for (i=0; i<cols; i++) {
    gtk_grid_insert_column(grid,0);
  }

  g_object_set_data(LIVES_WIDGET_OBJECT(grid),"rows",LIVES_INT_TO_POINTER(rows));
  g_object_set_data(LIVES_WIDGET_OBJECT(grid),"cols",LIVES_INT_TO_POINTER(cols));
  table=(LiVESWidget *)grid;
#else
  table=gtk_table_new(rows,cols,homogeneous);
#endif
#endif
#ifdef GUI_QT
  table = new LiVESTable;
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
#ifdef GUI_QT
  table->setHorizontalSpacing(spacing);
  return TRUE;
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
#ifdef GUI_QT
  table->setVerticalSpacing(spacing);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_table_set_row_homogeneous(LiVESTable *table, boolean homogeneous) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID
  gtk_grid_set_row_homogeneous(table,homogeneous);
  return TRUE;
#endif
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_table_set_column_homogeneous(LiVESTable *table, boolean homogeneous) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID
  gtk_grid_set_column_homogeneous(table,homogeneous);
  return TRUE;
#endif
#endif
  return FALSE;
}



LIVES_INLINE boolean lives_table_resize(LiVESTable *table, uint32_t rows, uint32_t cols) {
#ifdef GUI_GTK
#if LIVES_TABLE_IS_GRID  // required for grid remove row
  register int i;

  for (i=LIVES_POINTER_TO_INT(g_object_get_data(LIVES_WIDGET_OBJECT(table),"rows")); i<rows; i++) {
    gtk_grid_insert_row(table,i);
  }

  for (i=LIVES_POINTER_TO_INT(g_object_get_data(LIVES_WIDGET_OBJECT(table),"cols")); i<cols; i++) {
    gtk_grid_insert_column(table,i);
  }

  g_object_set_data(LIVES_WIDGET_OBJECT(table),"rows",LIVES_INT_TO_POINTER(rows));
  g_object_set_data(LIVES_WIDGET_OBJECT(table),"cols",LIVES_INT_TO_POINTER(cols));

#else
  gtk_table_resize(table,rows,cols);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_table_attach(LiVESTable *table, LiVESWidget *child, uint32_t left, uint32_t right,
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

#if GTK_CHECK_VERSION(3,12,0)
  gtk_widget_set_margin_start(child,xpad);
  gtk_widget_set_margin_end(child,xpad);
#else
  gtk_widget_set_margin_left(child,xpad);
  gtk_widget_set_margin_right(child,xpad);
#endif

  gtk_widget_set_margin_top(child,ypad);
  gtk_widget_set_margin_bottom(child,ypad);
#else
  gtk_table_attach(table,child,left,right,top,bottom,xoptions,yoptions,xpad,ypad);
#endif
  return TRUE;
#endif
#ifdef GUI_QT

  table->addWidget(static_cast<QWidget *>(child), top, left, bottom - top, right - left);


  QSizePolicy policy;
  if (xoptions&LIVES_EXPAND) {
    policy.setHorizontalPolicy(QSizePolicy::Expanding);
  } else {
    policy.setHorizontalPolicy(QSizePolicy::Preferred);
  }
  if (yoptions&LIVES_EXPAND) {
    policy.setVerticalPolicy(QSizePolicy::Expanding);
  } else {
    policy.setVerticalPolicy(QSizePolicy::Preferred);
  }
  child->setSizePolicy(policy);

  QRect qr = child->geometry();
  qr.setWidth(qr.width() + xpad);
  qr.setHeight(qr.height() + ypad);
  child->setGeometry(qr);

  return TRUE;
#endif

  return FALSE;
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
#ifdef GUI_QT
  cbutton = new LiVESColorButton(color);
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
#ifdef GUI_QT
  button->get_colour(color);
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
#ifdef GUI_QT
  button->set_colour(color);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE boolean lives_color_button_set_title(LiVESColorButton *button, const char *title) {
#ifdef GUI_GTK
  gtk_color_button_set_title(button,title);
  return TRUE;
#endif
#ifdef GUI_QT
  button->set_title(title);
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
#ifdef GUI_QT
  button->set_use_alpha(use_alpha);
  return TRUE;
#endif
  return FALSE;
}






LIVES_INLINE boolean lives_widget_get_pointer(LiVESXDevice *device, LiVESWidget *widget, int *x, int *y) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  // try: gdk_event_get_device (event)
  LiVESXWindow *xwin;
  if (widget==NULL) xwin=gdk_get_default_root_window();
  else xwin=lives_widget_get_xwindow(widget);
  if (xwin==NULL) {
    LIVES_ERROR("Tried to get pointer for windowless widget");
    return TRUE;
  }
  gdk_window_get_device_position(xwin,device,x,y,NULL);
#else
  gtk_widget_get_pointer(widget,x,y);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QPoint p = QCursor::pos(device);
  if (widget != NULL) p = widget->mapFromGlobal(p);
  *x = p.x();
  *y = p.y();
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE LiVESXDisplay *lives_widget_get_display(LiVESWidget *widget) {
  LiVESXDisplay *disp=NULL;
#ifdef GUI_GTK
  disp=gtk_widget_get_display(widget);
#endif
#ifdef GUI_QT
  QWidget *window = widget->window();
  window->winId();
  QWindow *qwindow = window->windowHandle();
  disp = qwindow->screen();
#endif
  return disp;
}


LIVES_INLINE LiVESXWindow *lives_display_get_window_at_pointer
(LiVESXDevice *device, LiVESXDisplay *display, int *win_x, int *win_y) {
  LiVESXWindow *xwindow=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (device==NULL) return NULL;
  xwindow=gdk_device_get_window_at_position(device,win_x,win_y);
#else
  xwindow=gdk_display_get_window_at_pointer(display,win_x,win_y);
#endif
#endif
#ifdef GUI_QT
  QWidget *widget = QApplication::widgetAt(QCursor::pos(display));
  widget->winId();
  xwindow = widget->windowHandle();
#endif
  return xwindow;

}


LIVES_INLINE boolean lives_display_get_pointer
(LiVESXDevice *device, LiVESXDisplay *display, LiVESXScreen **screen, int *x, int *y, LiVESXModifierType *mask) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (device==NULL) return TRUE;
  gdk_device_get_position(device,screen,x,y);
#else
  gdk_display_get_pointer(display,screen,x,y,mask);
#endif
  return TRUE;
#endif
#ifdef GUI_QT
  QPoint p = QCursor::pos(device);
  *x = p.x();
  *y = p.y();
  return TRUE;
#endif
  return FALSE;

}


LIVES_INLINE boolean lives_display_warp_pointer
(LiVESXDevice *device, LiVESXDisplay *display, LiVESXScreen *screen, int x, int y) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  if (device==NULL) return TRUE;
  gdk_device_warp(device,screen,x,y);
#else
#if GLIB_CHECK_VERSION(2,8,0)
  gdk_display_warp_pointer(display,screen,x,y);
#endif
#endif
  return TRUE;
#endif
#ifdef GTK_QT
  QCursor::setPos(display, x, y);
  return TRUE;
#endif
  return FALSE;
}


LIVES_INLINE lives_display_t lives_widget_get_display_type(LiVESWidget *widget) {
  lives_display_t dtype=LIVES_DISPLAY_TYPE_UNKNOWN;
#ifdef GUI_GTK
  LiVESXDisplay *display=gtk_widget_get_display(widget);
  display=display; // stop compiler complaining
  if (GDK_IS_X11_DISPLAY(display)) dtype=LIVES_DISPLAY_TYPE_X11;
  else if (GDK_IS_WIN32_DISPLAY(display)) dtype=LIVES_DISPLAY_TYPE_WIN32;
#endif
#ifdef GUI_QT
#ifdef Q_WS_X11
  dtype = LIVES_DISPLAY_TYPE_X11;
#endif
#ifdef Q_WS_WIN32
  dtype = LIVES_DISPLAY_TYPE_WIN32;
#endif
#endif
  return dtype;
}


LIVES_INLINE uint64_t lives_widget_get_xwinid(LiVESWidget *widget, const char *msg) {
  uint64_t xwin=-1;
#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
  if (lives_widget_get_display_type(widget)==LIVES_DISPLAY_TYPE_X11)
    xwin=(uint64_t)GDK_WINDOW_XID(lives_widget_get_xwindow(widget));
  else
#endif
#ifdef GDK_WINDOWING_WIN32
    if (lives_widget_get_display_type(widget)==LIVES_DISPLAY_TYPE_WIN32)
      xwin=(uint64_t)gdk_win32_drawable_get_handle(lives_widget_get_xwindow(widget));
    else
#endif
#endif
#ifdef GUI_QT
      if (LIVES_IS_WINDOW(widget)) xwin = (uint64_t)widget->effectiveWinId();
      else
#endif
        if (msg!=NULL) LIVES_WARN(msg);

  return xwin;
}


LIVES_INLINE uint32_t lives_timer_add(uint32_t interval, LiVESWidgetSourceFunc function, livespointer data) {
  uint32_t timer=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
  timer=g_timeout_add(interval,function,data);
#else
  timer=gtk_timeout_add(interval,function,data);
#endif
#endif
#ifdef GUI_QT
  LiVESTimer *ltimer = new LiVESTimer(interval, function, data);
  timer = ltimer->get_handle();
#endif
  return timer;
}


LIVES_INLINE boolean lives_timer_remove(uint32_t timer) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
  gtk_timeout_remove(timer);
  return TRUE;
#endif
#endif
#ifdef GUI_QT
  remove_static_timer(timer);
#endif
  return FALSE;
}



boolean lives_source_remove(ulong handle) {
#ifdef GUI_GTK
  g_source_remove(handle);
  return TRUE;
#endif
#ifdef GUI_QT
  lives_timer_remove((uint32_t)handle);
  return TRUE;
#endif
  return FALSE;
}



LIVES_INLINE uint32_t lives_accelerator_get_default_mod_mask() {
#ifdef GUI_GTK
  return gtk_accelerator_get_default_mod_mask();
#endif
#ifdef GUI_QT
  return 0;
#endif
}



LIVES_INLINE int lives_screen_get_width(LiVESXScreen *screen) {
#ifdef GUI_GTK
  return gdk_screen_get_width(screen);
#endif
#ifdef GUI_QT
  return screen->size().width();
#endif
  return 0;
}

LIVES_INLINE int lives_screen_get_height(LiVESXScreen *screen) {
#ifdef GUI_GTK
  return gdk_screen_get_height(screen);
#endif
#ifdef GUI_QT
  return screen->size().height();
#endif
  return 0;
}


// compound functions



LIVES_INLINE boolean lives_entry_set_editable(LiVESEntry *entry, boolean editable) {
  return lives_editable_set_editable(LIVES_EDITABLE(entry),editable);
}


static void set_label_state(LiVESWidget *widget, LiVESWidgetState state, livespointer labelp) {
  LiVESWidget *label=(LiVESWidget *)labelp;
  if (lives_widget_get_sensitive(widget)&&!lives_widget_get_sensitive(label)) {
    lives_widget_set_sensitive(label,TRUE);
  }
  if (!lives_widget_get_sensitive(widget)&&lives_widget_get_sensitive(label)) {
    lives_widget_set_sensitive(label,FALSE);
  }
}



void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  char *text=gtk_widget_get_tooltip_text(source);
  lives_widget_set_tooltip_text(dest,text);
  lives_free(text);
#else
  GtkTooltipsData *td=gtk_tooltips_data_get(source);
  if (td==NULL) return;
  gtk_tooltips_set_tip(td->tooltips, dest, td->tip_text, td->tip_private);
#endif
#endif
#ifdef GUI_QT
  dest->setToolTip(source->toolTip());
#endif
}


boolean lives_combo_populate(LiVESCombo *combo, LiVESList *list) {
  register int i;
  // remove any current list
  if (!lives_combo_set_active_index(combo,-1)) return FALSE;
  if (!lives_combo_remove_all_text(combo)) return FALSE;

  // add the new list
  for (i=0; i<lives_list_length(list); i++) {
    if (!lives_combo_append_text(LIVES_COMBO(combo),(const char *)lives_list_nth_data(list,i))) return FALSE;
  }
  return TRUE;
}





///// lives compounds


LiVESWidget *lives_volume_button_new(LiVESOrientation orientation, LiVESAdjustment *adj, double volume) {
  LiVESWidget *volume_scale=NULL;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  volume_scale=gtk_volume_button_new();
  gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_scale),volume);
  lives_scale_button_set_orientation(LIVES_SCALE_BUTTON(volume_scale),orientation);
#else
  if (orientation==LIVES_ORIENTATION_HORIZONTAL)
    volume_scale=lives_hscale_new(adj);
  else
    volume_scale=lives_vscale_new(adj);

  gtk_scale_set_draw_value(GTK_SCALE(volume_scale),FALSE);
#endif
#endif
#ifdef GUI_QT
  // TODO
  //  volume_scale = lives_scale_button_new(adj);
  //lives_scale_button_set_value(volume_scale, volume);
  //lives_scale_button_set_orientation (LIVES_SCALE_BUTTON(volume_scale),orientation);
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

  label=lives_label_new("");
  lives_label_set_text_with_mnemonic(LIVES_LABEL(label),text);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
  }

  if (mnemonic_widget!=NULL) lives_label_set_mnemonic_widget(LIVES_LABEL(label),mnemonic_widget);

  return label;
}


LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box,
    const char *tooltip) {
  LiVESWidget *checkbutton=NULL;

  // pack a themed check button into box

  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;

  checkbutton = lives_check_button_new();
  if (tooltip!=NULL) lives_widget_set_tooltip_text(checkbutton, tooltip);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    eventbox=lives_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,checkbutton);

    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic(labeltext,checkbutton);
    } else label=lives_standard_label_new(labeltext);

    lives_container_add(LIVES_CONTAINER(eventbox),label);

    lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(label_act_toggle),
                         checkbutton);

    widget_opts.last_label=label;

    if (widget_opts.apply_theme) {
      lives_widget_apply_theme(eventbox,LIVES_WIDGET_STATE_NORMAL);
    }
  }

  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new(FALSE, 0);
      if (!widget_opts.no_gui) {
        lives_widget_show(hbox);
      }
      lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);


    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

    lives_box_pack_start(LIVES_BOX(hbox), checkbutton, widget_opts.expand==LIVES_EXPAND_EXTRA, FALSE,
                         eventbox==NULL?0:widget_opts.packing_width);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

  }

  if (label!=NULL) {
    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(set_label_state),
                               label);
  }


  return checkbutton;
}




LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup,
    LiVESBox *box, const char *tooltip) {
  LiVESWidget *radiobutton=NULL;

  // pack a themed check button into box


  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;

  radiobutton = lives_radio_button_new(rbgroup);

  if (tooltip!=NULL) lives_widget_set_tooltip_text(radiobutton, tooltip);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic(labeltext,radiobutton);
    } else label=lives_standard_label_new(labeltext);

    widget_opts.last_label=label;

    eventbox=lives_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,radiobutton);
    lives_container_add(LIVES_CONTAINER(eventbox),label);

    lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(label_act_toggle),
                         radiobutton);

    if (widget_opts.apply_theme) {
      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    }
  }


  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new(FALSE, 0);
      if (!widget_opts.no_gui) {
        lives_widget_show(hbox);
      }
      lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

    lives_box_pack_start(LIVES_BOX(hbox), radiobutton, widget_opts.expand==LIVES_EXPAND_EXTRA, FALSE,
                         eventbox==NULL?0:widget_opts.packing_width);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

  if (label!=NULL) {
    lives_signal_connect_after(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(set_label_state),
                               label);
  }

  return radiobutton;
}


size_t calc_spin_button_width(double min, double max, int dp) {
  char *txt=lives_strdup_printf("%d",(int)max);
  size_t maxlen=strlen(txt);
  lives_free(txt);
  txt=lives_strdup_printf("%d",(int)min);
  if (strlen(txt)>maxlen) maxlen=strlen(txt);
  lives_free(txt);
  if (dp>0) maxlen+=3;
  return maxlen;
}


LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min,
    double max, double step, double page, int dp, LiVESBox *box,
    const char *tooltip) {
  LiVESWidget *spinbutton=NULL;

  // pack a themed check button into box


  LiVESWidget *eventbox=NULL;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;
  LiVESAdjustment *adj;

  boolean expand=FALSE;

  int maxlen;

  adj = lives_adjustment_new(val, min, max, step, page, 0.);
  spinbutton = lives_spin_button_new(adj, 1, dp);
  if (tooltip!=NULL) lives_widget_set_tooltip_text(spinbutton, tooltip);

  maxlen=calc_spin_button_width(min,max,dp);
  lives_entry_set_width_chars(LIVES_ENTRY(spinbutton),maxlen);

  lives_entry_set_activates_default(LIVES_ENTRY(spinbutton), TRUE);

#ifdef GUI_GTK
  gtk_spin_button_set_update_policy(LIVES_SPIN_BUTTON(spinbutton),GTK_UPDATE_ALWAYS);
  gtk_spin_button_set_numeric(LIVES_SPIN_BUTTON(spinbutton),TRUE);
#endif

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic(labeltext,spinbutton);
    } else label=lives_standard_label_new(labeltext);

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
      hbox = lives_hbox_new(FALSE, 0);
      if (!widget_opts.no_gui) {
        lives_widget_show(hbox);
      }
      lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
      expand=widget_opts.expand!=LIVES_EXPAND_NONE;
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    lives_box_pack_start(LIVES_BOX(hbox), spinbutton, widget_opts.expand==LIVES_EXPAND_EXTRA, FALSE, widget_opts.packing_width);

    if (expand) add_fill_to_box(LIVES_BOX(hbox));

    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
  }

  if (label!=NULL) {
    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(set_label_state),
                               label);
  }


  return spinbutton;
}






LiVESWidget *lives_standard_combo_new(const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box,
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
      label = lives_standard_label_new_with_mnemonic(labeltext,LIVES_WIDGET(entry));
    } else label = lives_standard_label_new(labeltext);

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
      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (widget_opts.expand==LIVES_EXPAND_DEFAULT) {
      LiVESWidget *label=lives_standard_label_new("");
      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, FALSE, 0);
    }

    if (!widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    lives_box_pack_start(LIVES_BOX(hbox), combo, widget_opts.expand!=LIVES_EXPAND_NONE,
                         widget_opts.expand==LIVES_EXPAND_EXTRA, eventbox==NULL?0:widget_opts.packing_width);
    if (widget_opts.swap_label&&eventbox!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);


    if (widget_opts.expand==LIVES_EXPAND_DEFAULT) {
      LiVESWidget *label=lives_standard_label_new("");
      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, FALSE, 0);
    }


  }

  lives_entry_set_editable(LIVES_ENTRY(entry),FALSE);
  lives_entry_set_activates_default(entry,TRUE);

  if (list!=NULL) {
    lives_combo_populate(LIVES_COMBO(combo),list);
    if (list!=NULL) lives_combo_set_active_index(LIVES_COMBO(combo),0);
  }

  if (label!=NULL) {
    lives_signal_connect_after(LIVES_GUI_OBJECT(combo), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(set_label_state),
                               label);
  }


  return combo;
}


LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, const char *txt, int dispwidth, int maxchars,
                                      LiVESBox *box,
                                      const char *tooltip) {

  LiVESWidget *entry=NULL;

  LiVESWidget *label=NULL;

  LiVESWidget *hbox=NULL;

  entry=lives_entry_new();

  if (tooltip!=NULL) lives_widget_set_tooltip_text(entry, tooltip);

  if (txt!=NULL)
    lives_entry_set_text(LIVES_ENTRY(entry),txt);

  if (dispwidth!=-1) lives_entry_set_width_chars(LIVES_ENTRY(entry),dispwidth);
  if (maxchars!=-1) lives_entry_set_max_length(LIVES_ENTRY(entry),maxchars);

  lives_entry_set_activates_default(LIVES_ENTRY(entry), TRUE);

  widget_opts.last_label=NULL;

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic(labeltext,entry);
    } else label = lives_standard_label_new(labeltext);

    widget_opts.last_label=label;

    if (tooltip!=NULL) lives_tooltips_copy(label,entry);
  }


  if (box!=NULL) {
    if (LIVES_IS_HBOX(box)) hbox=LIVES_WIDGET(box);
    else {
      hbox = lives_hbox_new(FALSE, 0);
      if (!widget_opts.no_gui) {
        lives_widget_show(hbox);
      }
      lives_box_pack_start(LIVES_BOX(box), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    lives_box_set_homogeneous(LIVES_BOX(hbox),FALSE);

    if (!widget_opts.swap_label&&label!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

    lives_box_pack_start(LIVES_BOX(hbox), entry, widget_opts.expand!=LIVES_EXPAND_NONE, dispwidth==-1, widget_opts.packing_width);

    if (widget_opts.swap_label&&label!=NULL)
      lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  }

  if (label!=NULL) {
    lives_signal_connect_after(LIVES_GUI_OBJECT(entry), LIVES_WIDGET_STATE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(set_label_state),
                               label);
  }


  return entry;
}



LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons, int width, int height) {
  LiVESWidget *dialog=NULL;

  dialog = lives_dialog_new();

  lives_widget_set_size_request(dialog, width, height);

  if (title!=NULL)
    lives_window_set_title(LIVES_WINDOW(dialog), title);

  lives_window_set_deletable(LIVES_WINDOW(dialog), FALSE);

  if (!widget_opts.non_modal)
    lives_window_set_resizable(LIVES_WINDOW(dialog), FALSE);

  lives_widget_set_hexpand(dialog,TRUE);
  lives_widget_set_vexpand(dialog,TRUE);

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(dialog, LIVES_WIDGET_STATE_NORMAL);

#if !GTK_CHECK_VERSION(3,0,0)
    lives_dialog_set_has_separator(LIVES_DIALOG(dialog),FALSE);
#endif
  }

  if (widget_opts.apply_theme) {
    funkify_dialog(dialog);
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width*2);
  }


  // do this before widget_show(), then call lives_window_center() afterwards
  lives_window_set_position(LIVES_WINDOW(dialog),LIVES_WIN_POS_CENTER_ALWAYS);

  if (add_std_buttons) {
    LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());
    LiVESWidget *cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);
    LiVESWidget *okbutton = lives_button_new_from_stock(LIVES_STOCK_OK,NULL);

    lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

    lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

    lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

    lives_widget_set_can_focus_and_default(cancelbutton);

    lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);

    lives_widget_set_can_focus_and_default(okbutton);
    lives_widget_grab_default(okbutton);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(dialog), LIVES_WIDGET_DELETE_EVENT,
                       LIVES_GUI_CALLBACK(return_true),
                       NULL);

  // must do this before setting modal !
  if (!widget_opts.no_gui) {
    lives_widget_show(dialog);
  }

  lives_window_center(LIVES_WINDOW(dialog));

  if (!widget_opts.non_modal)
    lives_window_set_modal(LIVES_WINDOW(dialog), TRUE);

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

  scrolledwindow = lives_scrolled_window_new(NULL, NULL);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(scrolledwindow), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);

  if (widget_opts.apply_theme) {
    lives_widget_set_hexpand(scrolledwindow,TRUE);
    lives_widget_set_vexpand(scrolledwindow,TRUE);
    lives_container_set_border_width(LIVES_CONTAINER(scrolledwindow), widget_opts.border_width);
  }

  if (child!=NULL) {

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3,0,0)
    if (!LIVES_IS_SCROLLABLE(child))
#else
    if (!LIVES_IS_TEXT_VIEW(child))
#endif
    {
      lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), child);
    } else {
      if (widget_opts.expand!=LIVES_EXPAND_NONE) {
        LiVESWidget *align;
        align=lives_alignment_new(.5,0.,0.,0.);
        lives_container_add(LIVES_CONTAINER(align), child);
        lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), align);
      } else {
        lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(scrolledwindow), child);
      }
    }
#endif
#ifdef GUI_QT
    lives_container_add(scrolledwindow, child);
#endif
  }

  swchild=lives_bin_get_child(LIVES_BIN(scrolledwindow));

#ifdef GTK
  if (GTK_IS_VIEWPORT(swchild))
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(swchild),LIVES_SHADOW_IN);

  if (width!=0&&height!=0) {
#if !GTK_CHECK_VERSION(3,0,0)
    lives_widget_set_size_request(scrolledwindow, width, height);
#else
    if (height!=-1) gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolledwindow),height);
    if (width!=-1) gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrolledwindow),width);
#endif
  }
#endif
#ifdef GUI_QT
  lives_widget_set_size_request(scrolledwindow, width, height);
#endif

  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(swchild, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_set_hexpand(swchild,TRUE);
    lives_widget_set_vexpand(swchild,TRUE);
    if (LIVES_IS_CONTAINER(child)) lives_container_set_border_width(LIVES_CONTAINER(child), widget_opts.border_width>>1);
  }

  return scrolledwindow;
}



LiVESWidget *lives_standard_expander_new(const char *ltext, boolean use_mnemonic, LiVESBox *parent, LiVESWidget *child) {
  LiVESWidget *expander=NULL;


#ifdef GUI_GTK
  if (use_mnemonic)
    expander=lives_expander_new_with_mnemonic(ltext);
  else
    expander=lives_expander_new(ltext);

  if (widget_opts.apply_theme) {
    LiVESWidget *label=lives_expander_get_label_widget(LIVES_EXPANDER(expander));
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(label, LIVES_WIDGET_STATE_PRELIGHT);
    lives_widget_apply_theme(expander, LIVES_WIDGET_STATE_PRELIGHT);
    lives_widget_apply_theme(expander, LIVES_WIDGET_STATE_NORMAL);
  }

#ifdef GUI_GTK
  lives_container_forall(LIVES_CONTAINER(expander),set_child_colour,LIVES_INT_TO_POINTER(TRUE));
#endif

  lives_box_pack_start(parent, expander, FALSE, FALSE, widget_opts.packing_height);

  lives_container_add(LIVES_CONTAINER(expander), child);
#endif

  return expander;
}




LIVES_INLINE LiVESWidget *lives_standard_file_button_new(boolean is_dir, const char *def_dir) {
  LiVESWidget *fbutton=NULL;
  LiVESWidget *image = lives_image_new_from_stock("gtk-open", LIVES_ICON_SIZE_BUTTON);
  fbutton = lives_button_new();
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(fbutton),"is_dir",LIVES_INT_TO_POINTER(is_dir));
  if (def_dir!=NULL) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(fbutton),"def_dir",(livespointer)def_dir);
  lives_container_add(LIVES_CONTAINER(fbutton), image);
  return fbutton;
}


LIVES_INLINE LiVESXCursor *lives_cursor_new_from_pixbuf(LiVESXDisplay *disp, LiVESPixbuf *pixbuf, int x, int y) {
  LiVESXCursor *cursor=NULL;
#ifdef GUI_GTK
  cursor=gdk_cursor_new_from_pixbuf(disp,pixbuf,x,y);
#endif
#ifdef GUI_QT
  QPixmap qpx;
  qpx.convertFromImage(*pixbuf);
  cursor = new QCursor(qpx, x, y);
#endif
  return cursor;
}



// utils

boolean widget_helper_init(void) {
#if GTK_CHECK_VERSION(3,10,0) || defined GUI_QT
  lives_snprintf(LIVES_STOCK_LABEL_CANCEL,32,"%s",(_("_Cancel")));
  lives_snprintf(LIVES_STOCK_LABEL_OK,32,"%s",(_("_OK")));
  lives_snprintf(LIVES_STOCK_LABEL_YES,32,"%s",(_("_Yes")));
  lives_snprintf(LIVES_STOCK_LABEL_NO,32,"%s",(_("_No")));
  lives_snprintf(LIVES_STOCK_LABEL_SAVE,32,"%s",(_("_Save")));
  lives_snprintf(LIVES_STOCK_LABEL_SAVE_AS,32,"%s",(_("Save _As")));
  lives_snprintf(LIVES_STOCK_LABEL_OPEN,32,"%s",(_("_Open")));
  lives_snprintf(LIVES_STOCK_LABEL_QUIT,32,"%s",(_("_Quit")));
  lives_snprintf(LIVES_STOCK_LABEL_APPLY,32,"%s",(_("_Apply")));
  lives_snprintf(LIVES_STOCK_LABEL_CLOSE,32,"%s",(_("_Close")));
  lives_snprintf(LIVES_STOCK_LABEL_REVERT,32,"%s",(_("_Revert")));
  lives_snprintf(LIVES_STOCK_LABEL_REFRESH,32,"%s",(_("_Refresh")));
  lives_snprintf(LIVES_STOCK_LABEL_DELETE,32,"%s",(_("_Delete")));
  lives_snprintf(LIVES_STOCK_LABEL_GO_FORWARD,32,"%s",(_("_Forward")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_FORWARD,32,"%s",(_("R_ewind")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_REWIND,32,"%s",(_("_Forward")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_PLAY,32,"%s",(_("_Play")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_PAUSE,32,"%s",(_("P_ause")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_STOP,32,"%s",(_("_Stop")));
  lives_snprintf(LIVES_STOCK_LABEL_MEDIA_RECORD,32,"%s",(_("_Record")));
#endif

  widget_opts = def_widget_opts;
  widget_opts.border_width*=widget_opts.scale;
  widget_opts.packing_width*=widget_opts.scale;
  widget_opts.packing_height*=widget_opts.scale;
  widget_opts.filler_len*=widget_opts.scale;

#ifdef GUI_GTK
  gtk_accel_map_add_entry("<LiVES>/save",LIVES_KEY_s,LIVES_CONTROL_MASK);
  gtk_accel_map_add_entry("<LiVES>/quit",LIVES_KEY_q,LIVES_CONTROL_MASK);
#endif

  return TRUE;
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
  g_object_unref(LIVES_GUI_OBJECT(cursor));
#else
  gdk_cursor_unref(cursor);
#endif
#endif
#ifdef GUI_QT
  delete cursor;
#endif
}


void lives_widget_apply_theme(LiVESWidget *widget, LiVESWidgetState state) {
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(widget, state, &palette->normal_fore);
    lives_widget_set_bg_color(widget, state, &palette->normal_back);
  }
}


void lives_widget_apply_theme2(LiVESWidget *widget, LiVESWidgetState state) {
  if (palette->style&STYLE_1) {
    //lives_widget_set_fg_color(widget, state, &palette->normal_fore);
    lives_widget_set_bg_color(widget, state, &palette->menu_and_bars);
  }
}



boolean lives_entry_set_completion_from_list(LiVESEntry *entry, LiVESList *xlist) {
#ifdef GUI_GTK
  GtkListStore *store;
  LiVESEntryCompletion *completion;
  store = gtk_list_store_new(1, LIVES_COL_TYPE_STRING);

  while (xlist != NULL) {
    LiVESTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, (char *)xlist->data, -1);
    xlist=xlist->next;
  }

  completion = gtk_entry_completion_new();
  gtk_entry_completion_set_model(completion, (GtkTreeModel *)store);
  gtk_entry_completion_set_text_column(completion, 0);
  gtk_entry_completion_set_inline_completion(completion, TRUE);
  gtk_entry_completion_set_popup_set_width(completion, TRUE);
  gtk_entry_completion_set_popup_completion(completion, TRUE);
  gtk_entry_completion_set_popup_single_match(completion,FALSE);
  gtk_entry_set_completion(entry, completion);
  return TRUE;
#endif
#ifdef GUI_QT
  QStringList qsl;
  for (int i = 0; xlist != NULL; i++) {
    qsl.append((QString::fromUtf8((const char *)xlist->data)));
    xlist = xlist->next;
  }
  QCompleter *qcmp = new QCompleter(qsl);
  QLineEdit *qe = static_cast<QLineEdit *>(entry);
  qe->setCompleter(qcmp);
#endif
  return FALSE;
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
    } else lives_window_set_position(LIVES_WINDOW(window),LIVES_WIN_POS_CENTER_ALWAYS);
  }
}



void lives_widget_get_fg_color(LiVESWidget *widget, LiVESWidgetColor *color) {
  lives_widget_get_fg_state_color(widget,LIVES_WIDGET_STATE_NORMAL,color);
}

void lives_widget_unparent(LiVESWidget *widget) {
  lives_container_remove(LIVES_CONTAINER(lives_widget_get_parent(widget)),widget);
}

boolean label_act_toggle(LiVESWidget *widget, LiVESXEventButton *event, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active(togglebutton, !lives_toggle_button_get_active(togglebutton));
  return FALSE;
}

boolean widget_act_toggle(LiVESWidget *widget, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active(togglebutton, TRUE);
  return FALSE;
}

LIVES_INLINE void toggle_button_toggle(LiVESToggleButton *tbutton) {
  if (lives_toggle_button_get_active(tbutton)) lives_toggle_button_set_active(tbutton,FALSE);
  else lives_toggle_button_set_active(tbutton,FALSE);
}


void set_child_colour(LiVESWidget *widget, livespointer set_allx) {
  boolean set_all=LIVES_POINTER_TO_INT(set_allx);

  if (!set_all&&LIVES_IS_BUTTON(widget)) return;

  if (LIVES_IS_CONTAINER(widget)) {
    lives_container_forall(LIVES_CONTAINER(widget),set_child_colour,set_allx);
    return;
  }

  if (set_all||LIVES_IS_LABEL(widget)) {
    lives_widget_apply_theme(widget, LIVES_WIDGET_STATE_NORMAL);
  }

  return;
}



char *lives_text_view_get_text(LiVESTextView *textview) {
  LiVESTextIter siter,eiter;
  LiVESTextBuffer *textbuf=lives_text_view_get_buffer(textview);
  lives_text_buffer_get_start_iter(textbuf,&siter);
  lives_text_buffer_get_end_iter(textbuf,&eiter);

  return lives_text_buffer_get_text(textbuf,&siter,&eiter,FALSE);
}


boolean lives_text_view_set_text(LiVESTextView *textview, const char *text, int len) {
  LiVESTextBuffer *textbuf=lives_text_view_get_buffer(textview);
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

  tbuf=lives_text_view_get_buffer(tview);
  if (tbuf!=NULL) {
    lives_text_buffer_get_end_iter(tbuf,&iter);
    mark=lives_text_buffer_create_mark(tbuf,NULL,&iter,FALSE);
    if (mark!=NULL) {
      if (lives_text_view_scroll_mark_onscreen(tview,mark))
        return lives_text_buffer_delete_mark(tbuf,mark);
    }
  }
  return FALSE;
}




int get_box_child_index(LiVESBox *box, LiVESWidget *tchild) {
  LiVESList *list=lives_container_get_children(LIVES_CONTAINER(box));
  int val=-1;
  if (list!=NULL) {
    val=lives_list_index(list,tchild);
    lives_list_free(list);
  }
  return val;
}


void lives_spin_button_configure(LiVESSpinButton *spinbutton,
                                 double value,
                                 double lower,
                                 double upper,
                                 double step_increment,
                                 double page_increment) {
  LiVESAdjustment *adj=lives_spin_button_get_adjustment(spinbutton);

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_configure(adj,value,lower,upper,step_increment,page_increment,0.);
#else
  g_object_freeze_notify(LIVES_WIDGET_OBJECT(adj));
  adj->upper=upper;
  adj->lower=lower;
  adj->value=value;
  adj->step_increment=step_increment;
  adj->page_increment=page_increment;
  g_object_thaw_notify(LIVES_WIDGET_OBJECT(adj));
#endif
#endif
#ifdef GUI_QT
  adj->set_lower(lower);
  adj->set_upper(upper);
  adj->set_value(value);
  adj->set_step_increment(step_increment);
  adj->set_page_increment(page_increment);
#endif
}



///// lives specific functions

#include "rte_window.h"
#include "ce_thumbs.h"

boolean lives_widget_context_update(void) {
  boolean mt_needs_idlefunc=FALSE;

  if (pthread_mutex_trylock(&mainw->gtk_mutex)) return FALSE;

  if (mainw->multitrack!=NULL&&mainw->multitrack->idlefunc>0) {

#ifdef GUI_GTK
    lives_source_remove(mainw->multitrack->idlefunc);
#endif

#ifdef GUI_QT
    lives_timer_remove(mainw->multitrack->idlefunc);
#endif

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
#ifdef GUI_GTK
    while (!mainw->is_exiting&&g_main_context_iteration(NULL,FALSE));
#endif
#ifdef GUI_QT
    QCoreApplication::processEvents();
#endif
  }

  if (!mainw->is_exiting&&mt_needs_idlefunc) mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);

  pthread_mutex_unlock(&mainw->gtk_mutex);

  return TRUE;

}



LiVESWidget *lives_menu_add_separator(LiVESMenu *menu) {
  LiVESWidget *separatormenuitem = lives_menu_item_new();
  if (separatormenuitem!=NULL) {
    lives_container_add(LIVES_CONTAINER(menu), separatormenuitem);
    lives_widget_set_sensitive(separatormenuitem, FALSE);
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
#ifdef GUI_QT
  return QApplication::desktop()->screenCount();
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
    } else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) {
      if (cstyle!=LIVES_CURSOR_NORMAL&&mainw->multitrack->cursor_style==cstyle) return;
      window=lives_widget_get_xwindow(mainw->multitrack->window);
    } else return;
  } else window=lives_widget_get_xwindow(widget);

  if (!LIVES_IS_XWINDOW(window)) return;

  switch (cstyle) {
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
    gdk_window_set_cursor(window, cursor);
  } else gdk_window_set_cursor(window,NULL);
  if (cursor!=NULL) lives_cursor_unref(cursor);
#endif

#ifdef GUI_QT
  if (widget==NULL) {
    if (mainw->multitrack==NULL&&mainw->is_ready) {
      if (cstyle!=LIVES_CURSOR_NORMAL&&mainw->cursor_style==cstyle) return;
      widget = mainw->LiVES;
    } else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) {
      if (cstyle!=LIVES_CURSOR_NORMAL&&mainw->multitrack->cursor_style==cstyle) return;
      widget = mainw->multitrack->window;
    } else return;
  }

  switch (cstyle) {
  case LIVES_CURSOR_NORMAL:
    widget->setCursor(Qt::ArrowCursor);
    break;
  case LIVES_CURSOR_BUSY:
    widget->setCursor(Qt::WaitCursor);
    break;
  case LIVES_CURSOR_CENTER_PTR:
    widget->setCursor(Qt::UpArrowCursor);
    break;
  case LIVES_CURSOR_HAND2:
    widget->setCursor(Qt::PointingHandCursor);
    break;
  case LIVES_CURSOR_SB_H_DOUBLE_ARROW:
    widget->setCursor(Qt::SizeHorCursor);
    break;
  case LIVES_CURSOR_CROSSHAIR:
    widget->setCursor(Qt::CrossCursor);
    break;
  case LIVES_CURSOR_TOP_LEFT_CORNER:
    widget->setCursor(Qt::SizeFDiagCursor);
    break;
  case LIVES_CURSOR_BOTTOM_RIGHT_CORNER:
    widget->setCursor(Qt::SizeBDiagCursor);
    break;
  default:
    return;
  }
#endif


}




void hide_cursor(LiVESXWindow *window) {
  //make the cursor invisible in playback windows
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,16,0)
  if (GDK_IS_WINDOW(window)) {
#if GTK_CHECK_VERSION(3,16,0)
    GdkCursor *cursor=gdk_cursor_new_for_display(gdk_window_get_display(window),GDK_BLANK_CURSOR);
#else
    GdkCursor *cursor=gdk_cursor_new(GDK_BLANK_CURSOR);
#endif
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
    source = gdk_bitmap_create_from_data(NULL, cursor_bits,
                                         1, 1);
    mask = gdk_bitmap_create_from_data(NULL, cursormask_bits,
                                       1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 0, 0);
    g_object_unref(source);
    g_object_unref(mask);
  }
  if (GDK_IS_WINDOW(window)) gdk_window_set_cursor(window, hidden_cursor);
#endif
#endif
#ifdef GUI_QT
  window->setCursor(Qt::BlankCursor);
#endif
}


void unhide_cursor(LiVESXWindow *window) {
  if (LIVES_IS_XWINDOW(window)) lives_xwindow_set_cursor(window,NULL);
}


void funkify_dialog(LiVESWidget *dialog) {
  if (prefs->funky_widgets) {
    LiVESWidget *frame=lives_frame_new(NULL);
    LiVESWidget *box=lives_vbox_new(FALSE,0);
    LiVESWidget *content=lives_dialog_get_content_area(LIVES_DIALOG(dialog));
    LiVESWidget *label=lives_label_new("");

    lives_container_set_border_width(LIVES_CONTAINER(dialog),0);

    if (widget_opts.apply_theme) {
      lives_widget_set_fg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    lives_object_ref(content);
    lives_widget_unparent(content);

    lives_container_add(LIVES_CONTAINER(dialog),frame);
    lives_container_add(LIVES_CONTAINER(frame),box);

    lives_box_pack_start(LIVES_BOX(box),content,TRUE,TRUE,0);

    lives_box_pack_start(LIVES_BOX(box),label,FALSE,TRUE,0);

    lives_widget_show_all(frame);

    lives_container_set_border_width(LIVES_CONTAINER(box), widget_opts.border_width*2);
  } else {
    lives_container_set_border_width(LIVES_CONTAINER(dialog), widget_opts.border_width*2);
  }

}



void get_border_size(LiVESWidget *win, int *bx, int *by) {
#ifdef GUI_GTK
  GdkRectangle rect;
  gint wx,wy;
  gdk_window_get_frame_extents(lives_widget_get_xwindow(win),&rect);
  gdk_window_get_origin(lives_widget_get_xwindow(win), &wx, &wy);
  *bx=wx-rect.x;
  *by=wy-rect.y;
#endif
#ifdef GUI_QT
  win->winId();
  QWindow *qwindow = win->windowHandle();
  QMargins qm = qwindow->frameMargins();
  *bx = qm.left() + qm.right();
  *by = qm.top() + qm.bottom();
#endif
}




/*
 * Set active string to the combo box
 */
boolean lives_combo_set_active_string(LiVESCombo *combo, const char *active_str) {
  return lives_entry_set_text(LIVES_ENTRY(lives_bin_get_child(LIVES_BIN(combo))),active_str);
}

LiVESWidget *lives_combo_get_entry(LiVESCombo *widget) {
  return lives_bin_get_child(LIVES_BIN(widget));
}


boolean lives_widget_set_can_focus_and_default(LiVESWidget *widget) {
  if (!lives_widget_set_can_focus(widget,TRUE)) return FALSE;
  return lives_widget_set_can_default(widget,TRUE);
}



void lives_general_button_clicked(LiVESButton *button, livespointer data_to_free) {
  // destroy the button top-level and free data
  lives_widget_destroy(lives_widget_get_toplevel(LIVES_WIDGET(button)));
  lives_widget_context_update();

  if (data_to_free!=NULL) lives_free(data_to_free);
}


LiVESWidget *add_hsep_to_box(LiVESBox *box) {
  LiVESWidget *hseparator = lives_hseparator_new();
  lives_box_pack_start(box, hseparator, FALSE, FALSE, widget_opts.packing_width>>1);
  if (!widget_opts.no_gui)
    lives_widget_show(hseparator);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(hseparator, LIVES_WIDGET_STATE_NORMAL);
  }
  return hseparator;
}


LiVESWidget *add_vsep_to_box(LiVESBox *box) {
  LiVESWidget *vseparator = lives_vseparator_new();
  lives_box_pack_start(box, vseparator, FALSE, FALSE, widget_opts.packing_height>>1);
  if (!widget_opts.no_gui)
    lives_widget_show(vseparator);
  if (widget_opts.apply_theme) {
    lives_widget_apply_theme(vseparator, LIVES_WIDGET_STATE_NORMAL);
  }
  return vseparator;
}


static char spaces[W_MAX_FILLER_LEN+1];
static boolean spaces_inited=FALSE;

LiVESWidget *add_fill_to_box(LiVESBox *box) {
  LiVESWidget *widget=NULL;

  LiVESWidget *blank_label;
  static int old_spaces=-1;
  static char *xspaces=NULL;

  if (!spaces_inited) {
    register int i;
    for (i=0; i<W_MAX_FILLER_LEN; i++) {
      lives_snprintf(spaces+i,1," ");
    }
  }

  if (widget_opts.filler_len>W_MAX_FILLER_LEN||widget_opts.filler_len<0) return NULL;

  if (widget_opts.filler_len!=old_spaces) {
    if (xspaces!=NULL) lives_free(xspaces);
    xspaces=lives_strndup(spaces,widget_opts.filler_len);
    old_spaces=widget_opts.filler_len;
  }

  blank_label = lives_standard_label_new(xspaces);

  lives_box_pack_start(box, blank_label, TRUE, TRUE, 0);
  lives_widget_set_hexpand(blank_label,TRUE);
  if (!widget_opts.no_gui)
    lives_widget_show(blank_label);
  widget=blank_label;

  return widget;
}


LIVES_INLINE boolean lives_button_box_set_button_width(LiVESButtonBox *bbox, LiVESWidget *button, int min_width) {
  lives_button_box_set_layout(bbox, LIVES_BUTTONBOX_SPREAD);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
  gtk_button_box_set_child_size(bbox,min_width/4,-1);
  return TRUE;
#endif
#endif
  lives_widget_set_size_request(button,min_width,-1);
  return TRUE;
#ifdef GUI_QT
  button->setMinimumWidth(min_width);
#endif
}




