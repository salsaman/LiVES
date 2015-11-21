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


#define RESIZE_ALL_NEEDS_CONVERT 1
#define LETTERBOX_NEEDS_COMPOSITE 1

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
weed_timecode_t q_dbl(double in, double fps);


weed_plant_t *quantise_events(weed_plant_t *in_list, double new_fps, boolean allow_gap);  ///< quantise frame events for a single clip

///////////////////////////////////////////////////////
int count_resampled_frames(int in_frames, double orig_fps, double resampled_fps);

/////////////////////////////////////////

// GUI functions
/// window change speed from Tools menu
void create_new_pb_speed(short type);

/// resample audio window
///
/// type 1 : show current and new,
/// type 2 : show new
_resaudw *create_resaudw(short type, render_details *rdet, LiVESWidget *top_vbox);

void on_change_speed_activate(LiVESMenuItem *, livespointer);
void on_change_speed_ok_clicked(LiVESButton *, livespointer);

boolean auto_resample_resize(int width, int height, double fps, int fps_num,
                             int fps_denom, int arate, int asigned, boolean swap_endian);
int reorder_frames(int rwidth, int rheight);
int deorder_frames(int old_framecount, boolean leave_bak); ///< leave_bak is a special mode for the clipboard

boolean resample_clipboard(double new_fps); ///< call this to resample clipboard video

#endif
