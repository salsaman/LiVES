// widget-helper.h
// LiVES
// (c) G. Finch 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_WIDGET_HELPER_H
#define HAS_LIVES_WIDGET_HELPER_H

#ifdef GUI_GTK
typedef GtkObject                         LiVESObject;
typedef GtkWidget                         LiVESWidget;
typedef GtkDialog                         LiVESDialog;
typedef GtkBox                            LiVESBox;

typedef GdkPixbuf                         LiVESPixbuf;

typedef gboolean                          boolean;
typedef GList                             LiVESList;
typedef GSList                            LiVESSList;


typedef GdkPixbufDestroyNotify            LiVESPixbufDestroyNotify;

typedef GdkInterpType                     LiVESInterpType;

#define LIVES_INTERP_BEST   GDK_INTERP_HYPER
#define LIVES_INTERP_NORMAL GDK_INTERP_BILINEAR
#define LIVES_INTERP_FAST   GDK_INTERP_NEAREST

#endif


#ifdef GUI_QT
typedef QImage                            LiVESPixbuf;
typedef bool                              boolean;
typedef int                               gint;
typedef uchar                             guchar;
typedef (void *)                          gpointer;  
typedef (void *)(LiVESPixbufDestroyNotify(uchar *, gpointer));

// etc.



#define LIVES_INTERP_BEST   Qt::SmoothTransformation
#define LIVES_INTERP_NORMAL Qt::SmoothTransformation
#define LIVES_INTERP_BEST   Qt::FastTransformation


#endif














// basic functions (wrappers for Toolkit functions)


int lives_pixbuf_get_width(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_height(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_has_alpha(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_rowstride(const LiVESPixbuf *pixbuf);
int lives_pixbuf_get_n_channels(const LiVESPixbuf *pixbuf);
guchar *lives_pixbuf_get_pixels(const LiVESPixbuf *pixbuf);
const guchar *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *pixbuf);
LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height);
LiVESPixbuf *lives_pixbuf_new_from_data (const unsigned char *buf, boolean has_alpha, int width, int height, 
					 int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn, 
					 gpointer destroy_fn_data);

LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, GError **error);
LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, boolean preserve_aspect_ratio,
						 GError **error);


LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height, 
				       LiVESInterpType interp_type);


LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog);

// compound functions (composed of basic functions)


LiVESWidget *lives_standard_check_button_new(const char *label, boolean use_mnemonic, LiVESBox *box, const char *tooltip);
LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *box, const char *tooltips);
LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *box, 
					    const char *tooltip);
LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip);

#endif

