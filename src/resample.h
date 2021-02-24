// resample.h
// LiVES
// (c) G. Finch 2004 - 2016 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions/structs for reordering, resampling video and audio

#ifndef HAS_LIVES_RESAMPLE_H
#define HAS_LIVES_RESAMPLE_H

#define RESIZE_ALL_NEEDS_CONVERT 0
#define LETTERBOX_NEEDS_COMPOSITE 1
#define LETTERBOX_NEEDS_CONVERT 1

/// resample audio window
typedef struct __resaudw {
  LiVESWidget *dialog;
  LiVESWidget *frame;
  LiVESWidget *vframe;
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
  LiVESWidget *vbox;
} _resaudw;

_resaudw *resaudw;

/// convert (int64_t)timecodes to multiples of (double)fps
#define q_gint64(in, fps) ((in) - (ticks_t)(remainder((double)(in) / TICKS_PER_SECOND_DBL, \
						      1. / (fps)) * TICKS_PER_SECOND_DBL + .5))

/// convert (double)seconds to multiples of (double)fps
#define q_dbl(in, fps) ((in) - remainder((in) / TICKS_PER_SECOND_DBL, 1. / (fps)) \
			* TICKS_PER_SECOND_DBL)

/// convert (double)seek_time to an integer number of audio samples
#define quant_asamps(seek, arate) (((seek) <= 0. || (arate) <= 0) ? (size_t)0 \
				   : (size_t)((seek) - remainder((seek), 1. / (arate))))

/// quantise (double) seek_time to the nearest audio sample
#define quant_aseek(seek, arate) (((arate) <= 0) ? 0. : (double)quant_asamps((seek), (arate)) \
				  / (double)(arate))

// quantise (double)seek to byte accurate offset: asampsize is in bytes
#define quant_abytes(seek, arate, achans, asampsize) ((off_t)((quant_asamps((seek), (arate)) * \
							       (off_t)((achans) * (asampsize)))))

ticks_t q_gint64_floor(ticks_t in, double fps);

void reorder_leave_back_set(boolean val);

#define SKJUMP_THRESH_RATIO 1.025 /// if fabs(recorded_vel / predicted_vel) < 1.0 +- SKJUMP_THRESH_RATIO then smooth the velocity
#define SKJUMP_THRESH_SECS 0.25 /// if fabs(rec_seek - predicted_seek) < SKJUMP_THRESH_SECS then smooth the seek time

///< quantise frame events for a single clip
weed_event_list_t *quantise_events(weed_event_list_t *in_list, double new_fps,
                                   boolean allow_gap) WARN_UNUSED;

///////////////////////////////////////////////////////
int count_resampled_frames(frames_t in_frames, double orig_fps, double resampled_fps);

/////////////////////////////////////////

// GUI functions

/// resample audio window
///
/// type 1 : show current and new,
/// type 2 : show new
_resaudw *create_resaudw(short type, render_details *, LiVESWidget *top_vbox);

void on_change_speed_activate(LiVESMenuItem *, livespointer);
void on_change_speed_ok_clicked(LiVESButton *, livespointer);

boolean auto_resample_resize(int width, int height, double fps, int fps_num,
                             int fps_denom, int arate, int asigned, boolean swap_endian);
int reorder_frames(int rwidth, int rheight);
int deorder_frames(frames_t old_framecount, boolean leave_bak); ///< leave_bak is a special mode for the clipboard

boolean resample_clipboard(double new_fps); ///< call this to resample clipboard video

#endif
