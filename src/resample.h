// resample.h
// LiVES
// (c) G. Finch 2004 - 2009 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions/structs for reordering, resampling video and audio

#ifndef __HAS_RESAMPLE_H
#define __HAS_RESAMPLE_H

#include "../libweed/weed.h"

/// resample audio window
typedef struct __resaudw {
  GtkWidget *dialog;
  GtkWidget *entry_arate;
  GtkWidget *entry_achans;
  GtkWidget *entry_asamps;
  GtkWidget *rb_signed;
  GtkWidget *rb_unsigned;
  GtkWidget *rb_bigend;
  GtkWidget *rb_littleend;
  GtkWidget *unlim_radiobutton;
  GtkWidget *hour_spinbutton;
  GtkWidget *minute_spinbutton;
  GtkWidget *second_spinbutton;
  GtkWidget *fps_spinbutton;
  GtkWidget *aud_checkbutton;
  GtkWidget *aud_hbox;
} _resaudw;


_resaudw *resaudw;

LIVES_INLINE weed_timecode_t q_gint64(weed_timecode_t in, double fps);
LIVES_INLINE weed_timecode_t q_gint64_floor(weed_timecode_t in, double fps);
LIVES_INLINE weed_timecode_t q_dbl (gdouble in, gdouble fps);


weed_plant_t *quantise_events (weed_plant_t *in_list, gdouble new_fps, gboolean allow_gap); ///< quantise frame events for a single clip

///////////////////////////////////////////////////////
gint count_resampled_frames (gint in_frames, gdouble orig_fps, gdouble resampled_fps);

/////////////////////////////////////////

// GUI functions
/// window change speed from Tools menu
void create_new_pb_speed (gshort type);

/// resample audio window
///
/// type 1 : show current and new, 
/// type 2 : show new
_resaudw* create_resaudw (gshort type, render_details *rdet, GtkWidget *top_vbox);

void on_change_speed_activate (GtkMenuItem *, gpointer);
void on_change_speed_ok_clicked (GtkButton *, gpointer);

gboolean auto_resample_resize (gint width,gint height,gdouble fps,gint fps_num,gint fps_denom, gint arate, gint asigned, gboolean swap_endian);
gint reorder_frames(void);
gint deorder_frames(gint old_framecount, gboolean leave_bak); ///< leave_bak is a special mode for the clipboard

gboolean resample_clipboard(gdouble new_fps); ///< call this to resample clipboard video

#endif
