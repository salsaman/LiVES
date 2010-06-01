// paramspecial.h
// LiVES
// (c) G. Finch 2004 - 2009 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// dynamic window generation from parameter arrays :-)
// special widgets

// TODO - refactor all of this using lives_param_special_t

typedef struct {
  gint height_param;
  gint width_param;
  GtkWidget *height_widget;
  GtkWidget *width_widget;
  GtkWidget *checkbutton;
  gulong width_func;
  gulong height_func;
} lives_special_aspect_t;


typedef struct {
  lives_rfx_t *rfx;
  gboolean added;
  gint type;
  gint xstart_param;
  gint ystart_param;
  gint xend_param;
  gint yend_param;
  gint *extra_params;
  gint num_extra;
  GtkWidget *xstart_widget;
  GtkWidget *ystart_widget;
  GtkWidget *xend_widget;
  GtkWidget *yend_widget;
  GtkWidget **extra_widgets;
} lives_special_framedraw_rect_t;

#define FD_NONE 0
#define FD_RECT_DEMASK 1
#define FD_RECT_MULTRECT 2
#define FD_SINGLEPOINT 3

typedef struct {
  lives_rfx_t *rfx;
  gint start_param;
  gint end_param;
  GtkWidget *start_widget;
  GtkWidget *end_widget;
} lives_special_mergealign_t;
////////////////////////////////

void init_special (void);

gint add_to_special (const gchar *special_string, lives_rfx_t *);

void check_for_special (lives_param_t *, gint num, GtkBox *, lives_rfx_t *);

void fd_connect_spinbutton(lives_rfx_t *);

void fd_tweak(lives_rfx_t *);

void after_aspect_width_changed (GtkSpinButton *, gpointer);

void after_aspect_height_changed (GtkToggleButton *, gpointer);

void special_cleanup (void);

lives_special_mergealign_t mergealign;
void setmergealign (void);

gboolean is_perchannel_multi(lives_rfx_t *rfx, gint i);

#include "framedraw.h"

#define RFX_EXTRA_WIDTH 200 /* extra width in pixels for framedraw */
