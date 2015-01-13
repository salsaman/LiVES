// resample.h
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions/structs for reordering, resampling video and audio

#ifndef HAS_LIVES_RESAMPLE_H
#define HAS_LIVES_RESAMPLE_H

#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#else
#include "../libweed/weed.h"
#endif

/// resample audio window
typedef struct __resaudw {
  LiVESWidget *dialog;
  LiVESWidget *entry_arate;
  LiVESWidget *entry_achans;
  LiVESWidget *entry_asamps;
  LiVESWidget *rb_signed;
  LiVESWidget *rb_unsigned;
  LiVESWidget *rb_bigend;
  LiVESWidget *rb_littleend;
  LiVESWidget *unlim_radiobutton;
  LiVESWidget *hour_spinbutton;
  LiVESWidget *minute_spinbutton;
  LiVESWidget *second_spinbutton;
  LiVESWidget *fps_spinbutton;
  LiVESWidget *aud_checkbutton;
  LiVESWidget *aud_hbox;
} _resaudw;


_resaudw *resaudw;

weed_timecode_t q_gint64(weed_timecode_t in, double fps);
weed_timecode_t q_gint64_floor(weed_timecode_t in, double fps);
weed_timecode_t q_dbl (gdouble in, gdouble fps);


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
_resaudw* create_resaudw (gshort type, render_details *rdet, LiVESWidget *top_vbox);

void on_change_speed_activate (LiVESMenuItem *, gpointer);
void on_change_speed_ok_clicked (LiVESButton *, gpointer);

gboolean auto_resample_resize (gint width, gint height, gdouble fps, gint fps_num, 
			       gint fps_denom, gint arate, gint asigned, gboolean swap_endian);
gint reorder_frames(int rwidth, int rheight);
gint deorder_frames(gint old_framecount, gboolean leave_bak); ///< leave_bak is a special mode for the clipboard

gboolean resample_clipboard(gdouble new_fps); ///< call this to resample clipboard video

#endif
