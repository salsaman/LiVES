// resample.c
// LiVES
// (c) G. Finch 2004 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for reordering, resampling video and audio

#include "main.h"
#include "resample.h"
#include "callbacks.h"
#include "effects.h"
#include "audio.h"
#include "cvirtual.h"

static int reorder_width = 0;
static int reorder_height = 0;
static boolean reorder_leave_back = FALSE;

void reorder_leave_back_set(boolean val) {reorder_leave_back = val;}

/////////////////////////////////////////////////////

LIVES_GLOBAL_INLINE ticks_t q_gint64(ticks_t in, double fps) {
  // quantise timecode to fps
  if (in > (ticks_t)0) return ((ticks_t)((double)in / (double)TICKS_PER_SECOND_DBL * (double)fps +
                                           (double).5) /
                                 (double)fps) * (ticks_t)TICKS_PER_SECOND; // quantise to frame timing
  if (in < (ticks_t)0) return ((ticks_t)((double)in / (double)TICKS_PER_SECOND_DBL * (double)fps -
                                           (double).5) /
                                 (double)fps) * (ticks_t)TICKS_PER_SECOND; // quantise to frame timing
  return (ticks_t)0;
}

LIVES_GLOBAL_INLINE ticks_t q_gint64_floor(ticks_t in, double fps) {
  if (in != 0) return (double)((ticks_t)((double)in / TICKS_PER_SECOND_DBL * fps + .000001)) / fps *
                        TICKS_PER_SECOND_DBL; // quantise to frame timing
  return 0;
}

LIVES_GLOBAL_INLINE ticks_t q_dbl(double in, double fps) {
  // quantise (double)in to fps
  if (in > 0.) return ((ticks_t)((double)in * (double)fps + (double).5) / (double)fps) *
                        (ticks_t)TICKS_PER_SECOND; // quantise to frame timing
  if (in < 0.) return ((ticks_t)((double)in * (double)fps - (double).5) / (double)fps) *
                        (ticks_t)TICKS_PER_SECOND; // quantise to frame timing
  return (ticks_t)0;
}


/// convert seek time to an integer number of samples
LIVES_GLOBAL_INLINE size_t quant_asamps(double seek, int arate) {
  size_t samps = (seek <= 0. || arate <= 0) ? 0 : (size_t)(seek * (double)arate + .99999);
  return samps;
}

/// convert seek time (secs) to an (almost) integer number of samples
LIVES_GLOBAL_INLINE double quant_aseek(double seek, int arate) {
  if (arate <= 0) return 0.;
  else {
    size_t samps = quant_asamps(seek, arate);
    return (double)samps / (double)arate;
  }
}

LIVES_GLOBAL_INLINE off_t quant_abytes(double seek, int arate, int achans, int asampsize) {
  size_t samps = quant_asamps(seek, arate);
  return samps * (size_t)(achans * asampsize);
}


LIVES_GLOBAL_INLINE int count_resampled_frames(int in_frames, double orig_fps, double resampled_fps) {
  int res_frames;
  if (resampled_fps < orig_fps) return ((res_frames = (int)((double)in_frames / orig_fps * resampled_fps)) < 1) ? 1 : res_frames;
  else return ((res_frames = (int)((double)in_frames / orig_fps * resampled_fps + .49999)) < 1) ? 1 : res_frames;
}

/////////////////////////////////////////////////////

boolean auto_resample_resize(int width, int height, double fps, int fps_num, int fps_denom, int arate,
                             int asigned, boolean swap_endian) {
  // do a block atomic: resample audio, then resample video/resize or joint resample/resize

  char *com, *msg = NULL;
  int current_file = mainw->current_file;
  boolean audio_resampled = FALSE;
  boolean video_resampled = FALSE;
  boolean video_resized = FALSE;
  boolean bad_header = FALSE;

  int frames = cfile->frames;

  reorder_leave_back = FALSE;

  if (asigned != 0 || (arate > 0 && arate != cfile->arate) || swap_endian) {
    cfile->undo1_int = arate;
    cfile->undo2_int = cfile->achans;
    cfile->undo3_int = cfile->asampsize;
    cfile->undo1_uint = cfile->signed_endian;

    if (asigned == 1 && (cfile->signed_endian & AFORM_UNSIGNED) == AFORM_UNSIGNED) cfile->undo1_uint ^= AFORM_UNSIGNED;
    else if (asigned == 2 && (cfile->signed_endian & AFORM_UNSIGNED) != AFORM_UNSIGNED) cfile->undo1_uint |= AFORM_UNSIGNED;

    if (swap_endian) {
      if (cfile->signed_endian & AFORM_BIG_ENDIAN) cfile->undo1_uint ^= AFORM_BIG_ENDIAN;
      else cfile->undo1_uint |= AFORM_BIG_ENDIAN;
    }

    on_resaudio_ok_clicked(NULL, NULL);
    if (mainw->error) return FALSE;
    audio_resampled = TRUE;
  }

  else {
    cfile->undo1_int = cfile->arate;
    cfile->undo2_int = cfile->achans;
    cfile->undo3_int = cfile->asampsize;
    cfile->undo4_int = cfile->arps;
    cfile->undo1_uint = cfile->signed_endian;
  }

  if (fps_denom > 0) {
    fps = (fps_num * 1.) / (fps_denom * 1.);
  }
  if (fps > 0. && fps != cfile->fps) {
    // FPS CHANGE...
    if ((width != cfile->hsize || height != cfile->vsize) && width * height > 0) {
      // CHANGING SIZE..

      // TODO: check if we have convert / composite installed

      if (fps > cfile->fps) {
        boolean rs_builtin;
        lives_rfx_t *resize_rfx;

        // we will have more frames...
        // ...do resize first
        if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate == -1 || prefs->enc_letterbox) {
          if (prefs->enc_letterbox && LETTERBOX_NEEDS_COMPOSITE && !capable->has_composite) {
            do_lb_composite_error();
            on_undo_activate(NULL, NULL);
            return FALSE;
          }

          if (prefs->enc_letterbox && LETTERBOX_NEEDS_CONVERT && !capable->has_convert) {
            do_lb_convert_error();
            on_undo_activate(NULL, NULL);
            return FALSE;
          }

          if (!prefs->enc_letterbox && RESIZE_ALL_NEEDS_CONVERT && !capable->has_convert) {
            do_ra_convert_error();
            on_undo_activate(NULL, NULL);
            return FALSE;
          }

          cfile->ohsize = cfile->hsize;
          cfile->ovsize = cfile->vsize;

          if (cfile->clip_type == CLIP_TYPE_FILE) {
            cfile->fx_frame_pump = 1;
          }

          if (!prefs->enc_letterbox) {
            com = lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\"", prefs->backend,
                                      cfile->handle, cfile->frames, width, height,
                                      get_image_ext_for_type(cfile->img_type));
            msg = lives_strdup_printf(_("Resizing frames 1 to %d"), cfile->frames);
          } else {
            int iwidth = cfile->hsize, iheight = cfile->vsize;
            calc_maxspect(width, height, &iwidth, &iheight);

            if (iwidth == cfile->hsize && iheight == cfile->vsize) {
              iwidth = -iwidth;
              iheight = -iheight;
            }

            reorder_leave_back = TRUE;
            com = lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\" %d %d", prefs->backend, cfile->handle,
                                      cfile->frames, width, height,
                                      get_image_ext_for_type(cfile->img_type), iwidth, iheight);
            msg = lives_strdup_printf(_("Resizing/letterboxing frames 1 to %d"), cfile->frames);
          }

          cfile->progress_start = 1;
          cfile->progress_end = cfile->frames;

          lives_rm(cfile->info_file);
          mainw->com_failed = FALSE;
          lives_system(com, FALSE);
          lives_free(com);

          if (mainw->com_failed) return FALSE;

          mainw->resizing = TRUE;
          rs_builtin = TRUE;
        } else {
          // use resize plugin
          int error;
          weed_plant_t *first_out;
          weed_plant_t *ctmpl;

          rs_builtin = FALSE;
          resize_rfx = mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx;
          first_out = get_enabled_channel((weed_plant_t *)resize_rfx->source, 0, FALSE);
          ctmpl = weed_get_plantptr_value(first_out, WEED_LEAF_TEMPLATE, &error);
          weed_set_int_value(ctmpl, WEED_LEAF_HOST_WIDTH, width);
          weed_set_int_value(ctmpl, WEED_LEAF_HOST_HEIGHT, height);
        }

        cfile->nokeep = TRUE;

        if ((rs_builtin && !do_progress_dialog(TRUE, TRUE, msg)) || (!rs_builtin && !on_realfx_activate_inner(1, resize_rfx))) {
          mainw->resizing = FALSE;
          lives_free(msg);
          cfile->undo_action = UNDO_ATOMIC_RESAMPLE_RESIZE;

          cfile->hsize = width;
          cfile->vsize = height;

          cfile->undo1_dbl = cfile->fps;
          cfile->undo_start = 1;
          cfile->undo_end = cfile->frames;
          cfile->fx_frame_pump = 0;
          on_undo_activate(NULL, NULL);
          cfile->nokeep = FALSE;
          return FALSE;
        }
        lives_free(msg);

        mainw->resizing = FALSE;
        cfile->fx_frame_pump = 0;

        cfile->hsize = width;
        cfile->vsize = height;

        if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
        if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;
        if (bad_header) do_header_write_error(mainw->current_file);

        cfile->undo1_dbl = fps;
        cfile->undo_start = 1;
        cfile->undo_end = frames;

        // now resample

        // special "cheat" mode for LiVES
        reorder_leave_back = TRUE;

        reorder_width = width;
        reorder_height = height;

        mainw->resizing = TRUE;
        on_resample_vid_ok(NULL, NULL);

        reorder_leave_back = FALSE;
        cfile->undo_action = UNDO_ATOMIC_RESAMPLE_RESIZE;
        if (mainw->error) {
          on_undo_activate(NULL, NULL);
          return FALSE;
        }

        video_resized = TRUE;
        video_resampled = TRUE;
      } else {
        // fewer frames
        // do resample *with* resize
        cfile->ohsize = cfile->hsize;
        cfile->ovsize = cfile->vsize;
        cfile->undo1_dbl = fps;

        // another special "cheat" mode for LiVES
        reorder_width = width;
        reorder_height = height;

        mainw->resizing = TRUE;
        on_resample_vid_ok(NULL, NULL);
        mainw->resizing = FALSE;

        reorder_width = reorder_height = 0;
        cfile->undo_action = UNDO_ATOMIC_RESAMPLE_RESIZE;
        cfile->hsize = width;
        cfile->vsize = height;

        if (mainw->error) {
          on_undo_activate(NULL, NULL);
          return FALSE;
        }
        if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
        if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;
        if (bad_header) do_header_write_error(mainw->current_file);

        video_resampled = TRUE;
        video_resized = TRUE;
      }
    } else {
      //////////////////////////////////////////////////////////////////////////////////
      cfile->undo1_dbl = fps;
      cfile->undo_start = 1;
      cfile->undo_end = cfile->frames;

      reorder_width = width;
      reorder_height = height;

      on_resample_vid_ok(NULL, NULL);

      reorder_width = reorder_height = 0;
      reorder_leave_back = FALSE;

      if (audio_resampled) cfile->undo_action = UNDO_ATOMIC_RESAMPLE_RESIZE;
      if (mainw->error) {
        on_undo_activate(NULL, NULL);
        return FALSE;
      }
      //////////////////////////////////////////////////////////////////////////////////////
      video_resampled = TRUE;
    }
  } else {
    boolean rs_builtin = TRUE;
    lives_rfx_t *resize_rfx;
    // NO FPS CHANGE
    if ((width != cfile->hsize || height != cfile->vsize) && width * height > 0) {
      // no fps change - just a normal resize
      cfile->undo_start = 1;
      cfile->undo_end = cfile->frames;

      if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate == -1 || prefs->enc_letterbox) {
        // use builtin resize

        if (prefs->enc_letterbox && LETTERBOX_NEEDS_COMPOSITE && !capable->has_composite) {
          do_lb_composite_error();
          on_undo_activate(NULL, NULL);
          return FALSE;
        }

        if (prefs->enc_letterbox && LETTERBOX_NEEDS_CONVERT && !capable->has_convert) {
          do_lb_convert_error();
          on_undo_activate(NULL, NULL);
          return FALSE;
        }

        if (!prefs->enc_letterbox && RESIZE_ALL_NEEDS_CONVERT && !capable->has_convert) {
          do_ra_convert_error();
          on_undo_activate(NULL, NULL);
          return FALSE;
        }

        if (cfile->clip_type == CLIP_TYPE_FILE) {
          cfile->fx_frame_pump = 1;
        }

        if (!prefs->enc_letterbox) {
          com = lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\"", prefs->backend,
                                    cfile->handle, cfile->frames, width, height,
                                    get_image_ext_for_type(cfile->img_type));
          msg = lives_strdup_printf(_("Resizing frames 1 to %d"), cfile->frames);
        } else {
          int iwidth = cfile->hsize, iheight = cfile->vsize;
          calc_maxspect(width, height, &iwidth, &iheight);

          if (iwidth == cfile->hsize && iheight == cfile->vsize) {
            iwidth = -iwidth;
            iheight = -iheight;
          }

          com = lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\" %d %d", prefs->backend,
                                    cfile->handle, cfile->frames, width, height,
                                    get_image_ext_for_type(cfile->img_type), iwidth, iheight);
          msg = lives_strdup_printf(_("Resizing/letterboxing frames 1 to %d"), cfile->frames);
        }

        cfile->progress_start = 1;
        cfile->progress_end = cfile->frames;

        cfile->ohsize = cfile->hsize;
        cfile->ovsize = cfile->vsize;

        cfile->undo1_dbl = cfile->fps;

        lives_rm(cfile->info_file);
        mainw->com_failed = FALSE;
        lives_system(com, FALSE);
        lives_free(com);

        if (mainw->com_failed) return FALSE;

        mainw->resizing = TRUE;
      } else {
        int error;
        weed_plant_t *first_out;
        weed_plant_t *ctmpl;

        rs_builtin = FALSE;
        resize_rfx = mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx;
        first_out = get_enabled_channel((weed_plant_t *)resize_rfx->source, 0, FALSE);
        ctmpl = weed_get_plantptr_value(first_out, WEED_LEAF_TEMPLATE, &error);
        weed_set_int_value(ctmpl, WEED_LEAF_HOST_WIDTH, width);
        weed_set_int_value(ctmpl, WEED_LEAF_HOST_HEIGHT, height);
      }

      cfile->nokeep = TRUE;

      if ((rs_builtin && !do_progress_dialog(TRUE, TRUE, msg)) || (!rs_builtin && !on_realfx_activate_inner(1, resize_rfx))) {
        mainw->resizing = FALSE;
        if (msg != NULL) lives_free(msg);
        cfile->hsize = width;
        cfile->vsize = height;
        if (audio_resampled) cfile->undo_action = UNDO_ATOMIC_RESAMPLE_RESIZE;
        else {
          cfile->undo_action = UNDO_RESIZABLE;
          set_undoable(_("Resize"), TRUE);
        }
        cfile->fx_frame_pump = 0;
        on_undo_activate(NULL, NULL);
        cfile->nokeep = FALSE;
        return FALSE;
      }

      cfile->hsize = width;
      cfile->vsize = height;

      lives_free(msg);
      mainw->resizing = FALSE;
      cfile->fx_frame_pump = 0;

      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_WIDTH, &cfile->hsize)) bad_header = TRUE;
      if (!save_clip_value(mainw->current_file, CLIP_DETAILS_HEIGHT, &cfile->vsize)) bad_header = TRUE;
      if (bad_header) do_header_write_error(mainw->current_file);

      if (audio_resampled) cfile->undo_action = UNDO_ATOMIC_RESAMPLE_RESIZE;
      else {
        cfile->undo_action = UNDO_RESIZABLE;
        set_undoable(_("Resize"), TRUE);
      }
      video_resized = TRUE;
      if (mainw->multitrack == NULL) {
        switch_to_file((mainw->current_file = 0), current_file);
      }
    }
  }

  if (cfile->undo_action == UNDO_ATOMIC_RESAMPLE_RESIZE) {
    // just in case we missed anything...

    set_undoable(_("Resample/Resize"), TRUE);
    if (!video_resized) {
      cfile->ohsize = cfile->hsize;
      cfile->ovsize = cfile->vsize;
    }
    if (!video_resampled) {
      cfile->undo1_dbl = cfile->fps;
    }
    cfile->undo_start = 1;
    cfile->undo_end = frames;
  }
  return TRUE;
}


//////////////////////////////////////////////////////////////////

static boolean copy_with_check(weed_plant_t *event, weed_plant_t *out_list, weed_timecode_t tc, char *what, size_t bytes) {
  LiVESResponseType response;
  do {
    response = LIVES_RESPONSE_OK;
    if (event_copy_and_insert(event, tc, out_list) == NULL) {
      response = do_memory_error_dialog(what, bytes);
    }
  } while (response == LIVES_RESPONSE_RETRY);
  if (response == LIVES_RESPONSE_CANCEL) return FALSE;
  return TRUE;
}


/* weed_plant_t *pre_analyse(weed_plant_t *elist) { */
/*   weed_plant_t *event = get_first_event(elist), *nevent; */
/*   weed_timecode_t stc; */
/*   lives_audio_track_state_t *astate; */
/*   if (!WEED_EVENT_IS_AUDIO_FRAME(event)) event = get_next_audio_frame(event); */
/*   if (event) { */
/*     stc = weed_event_get_timecode(event); */
/*     atstate = aframe_to_atstate(event); */
/*     /// get the next audio frame */
/*     nevent = get_next_audio_frame(event); */
/*     if (nevent) { */
/*       ntc = weed_event_get_timecode(event); */
/*       ntstate = aframe_to_atstate(event); */

/*       /// analyse velocity and seek values */



/*     } */
/*   } */
/* } */


/**
   @brief quantise from event_list_t *in_list to *out_list at the new rate of qfps

   This is called from 3 functions:
   - before rendering a recorded event_list
   - when entering multitrack, if the fps is mismatched
   - when resampling a clip (only frame events)

   @return new event list, or old event list if no changes are needed (i.e fps  is already correct), or NULL on error

   if old_list has no frames then we just return an empty list
   otherwise returned list will always have at least one event (a frame at timecode 0).

   if there is a timecode gap at the start of the old_list (i.e frame 1 has a non-zero timecode) then this well be eliminated in the
   out iist; i.e. frame 1 in out_list always has a timecode of 0.

   if the old event_list has a fixed fps, then we try to keep the duration as near as possible, so:

   nframes_new = MAX(nframes_old / old_fps * new_fps , 1)
   the value is rounded DOWN in the case that qfps < old_fps
   and rounded to the nearest integer in the case that qfps > old_fps

   e.g 210 frames @ 20.0 fps 	-> 157 frames @ 15.0 fps
   						-> 410 frames @ 39.0 fps
   @see count_resmpled_frames()

   i.e when the duration of the final frame is added, the total will always be >= duration of the old list


   if old_list has no fixed fps, then new_frames = (timecode of last frame  - timecode of first frame) * qfps
   rounded UP.

   /// algorithm:
   /// - get tc of 1st frame event in in_list
   /// tc of out_list starts at zero. If allow_gap is FALSE we add an offset_tc so out_list 0 coincides with tc of 1st frame in in_list.
   /// loop:
   /// - advance in_list until either we hit a frame event, or tc of NEXT event > out_tc
   ///  -- if tc of next frame is <= out_tc, we continue
   ///  -- update the state to current event
   ///
   /// - apply in_list state at out_tc, interpolating between last (current) in frame and next in frame
   /// - advance out_tc by 1. / out_fps
   ///  - goto loop

   /// inserting from scrap_file, we cannot interpolate frame numbers. So we just insert nearest frame
*/
weed_plant_t *quantise_events(weed_plant_t *in_list, double qfps, boolean allow_gap) {
  weed_timecode_t out_tc = 0, offset_tc = 0, in_tc, laud_tc = 0, nx_tc;
  weed_timecode_t end_tc;

  weed_plant_t *out_list;
  weed_event_t *naudio_event = NULL;
  weed_event_t *frame_event = NULL, *nframe_event = NULL;
  weed_event_t *last_frame_event;
  weed_event_t *event, *newframe = NULL;
  weed_event_t *init_event, *filter_map = NULL, *deinit_event;
  weed_event_t *prev_aframe, *xprev_aframe, *last_out_frame = NULL;
  weed_timecode_t recst_tc = 0;

  LiVESResponseType response;
  LiVESList *init_events = NULL, *deinit_events = NULL, *list;

  ticks_t tl;
  double *xaseeks = NULL, *naseeks = NULL, *naccels = NULL;
  double old_fps;
  char *what;

  boolean interpolate = TRUE;
  int *clips = NULL, *naclips = NULL, *nclips = NULL;
  int64_t  *frames = NULL,  *nframes = NULL;
  int *xaclips = NULL;

  int tracks, ntracks = 0, natracks = 0, xatracks = 0;
  int etype;
  int is_final = 0;
  register int i, j, k;

  if (in_list == NULL) return NULL;
  if (qfps < 1.) return NULL;

  old_fps = weed_get_double_value(in_list, WEED_LEAF_FPS, NULL);
  if (old_fps == qfps) return in_list;

  tl = (TICKS_PER_SECOND_DBL / qfps + .499999);
  what = lives_strdup(_("quantising the event list"));

  do {
    response = LIVES_RESPONSE_OK;
    out_list = weed_plant_new(WEED_PLANT_EVENT_LIST);
    if (out_list == NULL) {
      response = do_memory_error_dialog(what, 0);
    }
  } while (response == LIVES_RESPONSE_RETRY);
  if (response == LIVES_RESPONSE_CANCEL) {
    event_list_free(out_list);
    lives_free(what);
    return NULL;
  }

  /* if (old_fps == 0.) { */
  /*   /// in pre-analysis, we will look at the audio frames, and instead of correcting the audio veloicity, we will */
  /*   /// attempt to slightly modify (scale) the frame timings such that the audio hits the precise seek point  */
  /*   in_list = pre_analyse(in_list); */
  /* } */

  weed_set_voidptr_value(out_list, WEED_LEAF_FIRST, NULL);
  weed_set_voidptr_value(out_list, WEED_LEAF_LAST, NULL);
  weed_set_double_value(out_list, WEED_LEAF_FPS, qfps);

  event = get_first_event(in_list);
  if (event == NULL) goto q_done;

  last_frame_event = get_last_frame_event(in_list);
  if (last_frame_event == NULL) goto q_done;

  if (!allow_gap) offset_tc = get_event_timecode(get_first_frame_event(in_list));
  else out_tc = get_event_timecode(get_first_frame_event(in_list));
  end_tc = get_event_timecode(last_frame_event) - offset_tc;
  end_tc = q_gint64(end_tc + tl, qfps);

  // tl >>2 - make sure we don't round down
  for (; out_tc < end_tc || event != NULL; out_tc = q_gint64(out_tc + tl + (tl >> 2), qfps)) {
    weed_timecode_t stop_tc = out_tc + offset_tc;
    if (out_tc > end_tc) out_tc = end_tc;

    while (1) {
      /// in this mode we walk the event_list until we pass the output time, keeping track of
      /// state - frame and clip numbers, audio positions, param values, then insert everything at the output slot
      /// - we also look at the next frame to decide how to proceed
      /// - for normal clips and audio, we can interpolate between the two
      /// - for the scrap file, we cannot interpolate, so we insert whichever frame is nearest, unless we already inserted
      ///   the last frame and the next frame would be dropped

      /// values which we maintain: current init_events (cancelled by a deinit)
      /// current filter map (cancelled by a new filter_map)
      /// current deinits (cancelled by cancelling an init_event)
      /// current param changes (cancelled by another pchange for same fx / param or a deinit)
      /// audio seeks

      /// events are added in the standard ordering, i.e filter_inits, param changes, filter map, frame, filter_deinits
      if (event != NULL) in_tc = get_event_timecode(event);

      if (event != NULL && (is_final == 2 || (in_tc <= stop_tc && is_final != 1))) {
        /// update the state until we pass out_tc
        etype = weed_event_get_type(event);
        //g_print("got event type %d at tc %ld, out = %ld\n", etype, in_tc, out_tc);

        switch (etype) {
        case WEED_EVENT_HINT_MARKER: {
          int marker_type = weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, NULL);
          if (marker_type == EVENT_MARKER_BLOCK_START || marker_type == EVENT_MARKER_BLOCK_UNORDERED
              || marker_type == EVENT_MARKER_RECORD_START) {
            // if event_list started as a recording then this will have been set for previews, but we now need to discard it
            interpolate = FALSE;
            lives_freep((void **)&xaclips);
            lives_freep((void **)&xaseeks);
            lives_freep((void **)&naclips);
            lives_freep((void **)&naseeks);
            xatracks = natracks = 0;
            lives_list_free(init_events);
            lives_list_free(deinit_events);
            init_events = deinit_events = NULL;
            filter_map = NULL;
#ifdef SMOOTH_AUD_VEL
            recst_tc = get_event_timecode(event);
#endif
          }
          if ((allow_gap && marker_type == EVENT_MARKER_RECORD_START)
              || marker_type == EVENT_MARKER_BLOCK_START || marker_type == EVENT_MARKER_BLOCK_UNORDERED) {
            if (!copy_with_check(event, out_list, out_tc, what, 0)) {
              event_list_free(out_list);
              out_list = NULL;
              goto q_done;
            }
          }
        }
        break;
        case WEED_EVENT_HINT_FRAME:
          interpolate = TRUE;
          nframe_event = get_next_frame_event(event);
          if (nframe_event == NULL) is_final = 1;

          /// now we have a choice: we can either insert this frame at out_tc with the current fx state, or with the state at out_tc
#define KEEP_FRAME_STATE
#ifdef KEEP_FRAME_STATE
          if (!is_final) {
            if (weed_event_get_timecode(nframe_event) > stop_tc) stop_tc = -1; // force insertion now
          }
#endif
          /* // interpolate unadded audio */
          if (natracks > 0) {
            // advance the seek value, so when we do add the audio vals, the seek is to the right time
            // we use const. accel calculated when picked up
            double dt = (double)(in_tc - laud_tc) / TICKS_PER_SECOND_DBL;
            for (i = 0; i < natracks; i += 2) {
              double vel = naseeks[i + 1];
              if (naseeks[i + 1] != 0.) {
                naseeks[i] += vel * dt;
              }
            }
          }

          if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
            // update unadded audio state (natracks) from in_list
            // TODO: make use of aframe_to_atstate();
            int *aclips;
            double *aseeks;
            int atracks = weed_frame_event_get_audio_tracks(event, &aclips, &aseeks);
            for (i = 0; i < atracks; i += 2) {
              for (j = 0; j < natracks; j += 2) {
                if (naclips[j] == aclips[i]) {
                  // replace (superceded)
                  naclips[j + 1] = aclips[i + 1];
                  naseeks[j] = aseeks[i];
                  naseeks[j + 1] = aseeks[i + 1];
                  break;
                }
              }

              if (j == natracks) {
                natracks += 2;
                // append
                naclips = (int *)lives_realloc(naclips, natracks * sizint);
                naseeks = (double *)lives_realloc(naseeks, natracks * sizdbl);
                naccels = (double *)lives_realloc(naccels, (natracks >> 1) * sizdbl);
                naclips[natracks - 2] = aclips[j];
                naclips[natracks - 1] = aclips[j + 1];
                naseeks[natracks - 2] = aseeks[j];
                naseeks[natracks - 1] = aseeks[j + 1];
                naccels[(natracks >> 1) - 1] = 0.;
              }
            }
            lives_free(aclips);
            lives_free(aseeks);
            if (naudio_event == event) naudio_event = NULL;
          }
          /// laud_tc is last frame in_tc, frame_event is last frame
          laud_tc = in_tc;
          frame_event = event;
          if (event == nframe_event) nframe_event = NULL;
          break;
        case WEED_EVENT_HINT_FILTER_INIT:
          // add to filter_inits list
          weed_leaf_delete(event, WEED_LEAF_HOST_TAG);
          init_events = lives_list_prepend(init_events, event);
          break;
        case WEED_EVENT_HINT_FILTER_DEINIT:
          /// if init_event is in list, discard it + this event
          init_event = weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
          for (list = init_events; list != NULL; list = list->next) {
            if (list->data == init_event) {
              if (list->prev != NULL) list->prev->next = list->next;
              else init_events = list->next;
              if (list->next != NULL) list->next->prev = list->prev;
              list->next = list->prev = NULL;
              lives_list_free(list);
              break;
            }
          }
          if (list == NULL) {
            if (!is_final) deinit_events = lives_list_prepend(deinit_events, event);
            else {
              //g_print("adding deinit at %lld\n", out_tc);
              if (!copy_with_check(event, out_list, out_tc, what, 0)) {
                event_list_free(out_list);
                out_list = NULL;
                goto q_done;
              }
            }
          }
          break;
        case WEED_EVENT_HINT_PARAM_CHANGE:
          if (is_final) break;
          /// param changes just get inserted at whatever timcode, as long as their init_event isnt in the "to be added" list
          init_event = weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
          for (list = init_events; list != NULL; list = list->next) {
            if (list->data == init_event) break;
          }
          if (list == NULL) {
            if (!copy_with_check(event, out_list, in_tc - offset_tc, what, 0)) {
              event_list_free(out_list);
              out_list = NULL;
              goto q_done;
            }
          }
          break;
        case WEED_EVENT_HINT_FILTER_MAP:
          /// replace current filter map
          filter_map = event;
          break;
        default:
          /// probably a marker; ignore
          break;
        }
        if (event != NULL) event = get_next_event(event);
      } else {
        weed_timecode_t frame_tc;
        /// insert the state
        if (init_events != NULL) {
          void **pchanges;
          int nchanges;
          // insert filter_inits + init pchanges
          for (list = init_events; list != NULL; list = list->next) {
            init_event = (weed_event_t *)list->data;
            //g_print("ins init %p\n", init_event);
            if (!copy_with_check(init_event, out_list, out_tc, what, 0)) {
              event_list_free(out_list);
              out_list = NULL;
              lives_list_free(init_events);
              goto q_done;
            }
            // insert init pchanges
            pchanges = weed_get_voidptr_array_counted(init_event, WEED_LEAF_IN_PARAMETERS, &nchanges);
            init_event = get_last_event(out_list);
            for (i = 0; i < nchanges; i++) {
              weed_event_t *pchange = (weed_event_t *)pchanges[i];
              if (!copy_with_check(pchange, out_list, out_tc, what, 0)) {
                event_list_free(out_list);
                out_list = NULL;
                lives_list_free(init_events);
                lives_free(pchanges);
                goto q_done;
              }
              pchanges[i] = (void *)get_last_event(out_list);
            }
            weed_set_voidptr_array(init_event, WEED_LEAF_IN_PARAMETERS, nchanges, pchanges);
          }
          lives_list_free(init_events);
          init_events = NULL;
          lives_free(pchanges);
        }

        if (filter_map != NULL && deinit_events == NULL) {
          //g_print("ins filter map\n");
          if (!copy_with_check(filter_map, out_list, out_tc, what, 0)) {
            event_list_free(out_list);
            out_list = NULL;
            goto q_done;
          }
          filter_map = NULL;
        }

        /// INSERT A FRAME AT OUT_TC

        tracks = weed_frame_event_get_tracks(frame_event, &clips, &frames);
        frame_tc = get_event_timecode(frame_event);

        // frame_event is always <= out_tc, nframe_event is always > out_tc
        if (nframe_event == NULL) nframe_event = get_next_frame_event(frame_event);
        ntracks = weed_frame_event_get_tracks(nframe_event, &nclips, &nframes);
        nx_tc = get_event_timecode(nframe_event);

        if (nframe_event != NULL) {
          if (mainw->scrap_file != -1 && (nclips[0] == mainw->scrap_file || clips[0] == mainw->scrap_file)) {
            if (nx_tc - (out_tc + offset_tc) < out_tc + offset_tc - frame_tc) {
              // scrap file
              frame_event = nframe_event;
              lives_free(clips);
              lives_free(frames);
              frames = nframes;
              clips = nclips;
              tracks = ntracks;
            }
          } else {
            if (old_fps == 0. && interpolate) {
              /// interpolate frames if possible
              double ratio = (double)(out_tc - frame_tc) / (double)(nx_tc - frame_tc);
              for (i = 0; i < tracks; i++) {
                if (i >= ntracks) break;
                if (clips[i] == nclips[i]) {
                  frames[i] = (int64_t)((double)frames[i] + (double)(nframes[i] - frames[i]) * ratio);
                }
              }
            }
            lives_free(nclips);
            lives_free(nframes);
          }
        }

        /// now we insert the frame
        do {
          response = LIVES_RESPONSE_OK;
          //g_print("frame with %d tracks %d %d  going in at %ld\n", tracks, clips[0], frames[0], out_tc);
          if (insert_frame_event_at(out_list, out_tc, tracks, clips, frames, &newframe) == NULL) {
            response = do_memory_error_dialog(what, 0);
          }
        } while (response == LIVES_RESPONSE_RETRY);
        if (response == LIVES_RESPONSE_CANCEL) {
          event_list_free(out_list);
          lives_free(what);
          return NULL;
        }
        if (weed_plant_has_leaf(frame_event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET)) {
          weed_leaf_dup(newframe, frame_event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET);
        }
        weed_leaf_dup(newframe, frame_event, WEED_LEAF_OVERLAY_TEXT);

        lives_freep((void **)&frames);
        lives_freep((void **)&clips);

        if (natracks > 0) {
          /// insert the audio state
          // filter naclips, remove any zeros for avel when track is not in xaclips
          for (i = 0; i < natracks; i += 2) {
            if (naseeks[i + 1] == 0.) {
              for (j = 0; j < xatracks; j += 2) {
                if (xaclips[j] == naclips[i] && xaseeks[j + 1] != 0.) break;
              }
              if (j == xatracks) {
                natracks -= 2;
                if (natracks == 0) {
                  lives_freep((void **)&naclips);
                  lives_freep((void **)&naseeks);
                  lives_freep((void **)&naccels);
                } else {
                  for (k = i; k < natracks; k += 2) {
                    naclips[k] = naclips[k + 2];
                    naclips[k + 1] = naclips[k + 3];
                    naseeks[k] = naseeks[k + 2];
                    naseeks[k + 1] = naseeks[k + 3];
                    naccels[k >> 1] = naccels[(k >> 1) + 1];
                  }
                  naclips = (int *)lives_realloc(naclips, natracks * sizint);
                  naseeks = (double *)lives_realloc(naseeks, natracks * sizdbl);
                  naccels = (double *)lives_realloc(naccels, (natracks >> 1) * sizdbl);
		  // *INDENT-OFF*
		}}}}}
	// *INDENT-ON*

        if (natracks > 0) {
          /// if there is still audio to be added, update the seek posn to out_tc
          /// laud_tc is last frame in_tc
          double dt = (double)(out_tc + offset_tc - laud_tc) / TICKS_PER_SECOND_DBL;
          for (i = 0; i < natracks; i += 2) {
            double vel = naseeks[i + 1];
            if (naseeks[i + 1] != 0.) {
              int in_arate = mainw->files[naclips[i + 1]]->arps;
              naseeks[i] += vel * dt;
              naseeks[i] = quant_aseek(naseeks[i], in_arate);
            }
          }

          weed_set_int_array(newframe, WEED_LEAF_AUDIO_CLIPS, natracks, naclips);
          weed_set_double_array(newframe, WEED_LEAF_AUDIO_SEEKS, natracks, naseeks);

#define SMOOTH_AUD_VEL
          /// the timecode of each audio frame is adjusted to the quantised time, and we update the seek position accordingly
          /// however, when playing back, any velocity change will come slightly later than when recorded; thus
          /// the player seek pos will be slightly off.
          /// To remedy this we can very slightly adjust the velocity at the prior frame, so that the seek is correct when
          /// arriving at the current audio frame
          /// TODO: needs fixing, does not work as intended - not sure why
#ifdef SMOOTH_AUD_VEL
          prev_aframe = get_prev_audio_frame_event(newframe);
          if (prev_aframe) {
            for (i = 0; i < natracks; i += 2) {
              // check each track in natracks (currently active) to see if it is also in xatracks (all active)
              boolean gottrack = FALSE;
              if (naseeks[i + 1] == 0.) continue; ///< audio was off, currently we don't store the seek point there (we SHOULD: TODO)
              for (k = 0; k < xatracks; k += 2) {
                if (xaclips[k] == naclips[i]) {
                  //. track is in xatracks, so there must be a prev audio frame for the track; if the clips match then we will find
                  // the audio frame event and maybe adjust the velocity
                  if (xaclips[k + 1] == naclips[i + 1]) gottrack = TRUE;
                  break;
                }
              }
              if (!gottrack) continue;

              /// find the prior audio frame for the track
              xprev_aframe = prev_aframe;
              while (xprev_aframe != NULL) {
                weed_timecode_t ptc = get_event_timecode(xprev_aframe);
                int *paclips;
                double *paseeks;
                int patracks;
                if (ptc < recst_tc) break;

                patracks = weed_frame_event_get_audio_tracks(xprev_aframe, &paclips, &paseeks);

                for (j = 0; j < patracks; j += 2) {
                  if (paclips[j] == naclips[i]) {
                    if (paclips[j + 1] == naclips[i + 1]) {
                      if (paseeks[j + 1] > 0.) {
                        double dt = (double)(out_tc - ptc) / TICKS_PER_SECOND_DBL;
                        double dtl = (double)(out_tc - get_event_timecode(last_out_frame)) / TICKS_PER_SECOND_DBL;
                        /// what we will do here is insert an extra audio event at the previous out_frame.
                        /// the seek will be calculated from old_val, and we will adjust the velocity so we hit the seek value at this frame
                        // adjust velocity by seek_delta / frame_duration
                        int in_arate = mainw->files[naclips[i + 1]]->arps;
                        double seek = paseeks[j] + paseeks[j + 1] * (dt - dtl);
                        double nvel = (naseeks[i] - seek) / dtl;

                        /// velocity should be close to seek_time / clock_time, else this was a jump
                        if ((paseeks[j + 1] < naseeks[i + 1] && nvel > paseeks[j + 1] * (1. - SKJUMP_THRESH_RATIO)
                             && nvel <= naseeks[i + 1] * (1. + SKJUMP_THRESH_RATIO))
                            || (paseeks[j + 1] > naseeks[i + 1] && nvel < paseeks[j + 1]
                                * (1. + SKJUMP_THRESH_RATIO) && nvel >= naseeks[i + 1] * (1. - SKJUMP_THRESH_RATIO))) {
                          insert_audio_event_at(last_out_frame, paclips[j], paclips[j + 1], seek, nvel);
                        } else {
                          // if velocity change is too great then we may adjust the seek a little instead
                          if (fabs(naseeks[i] - (seek + paseeks[j + 1] * dtl)) < SKJUMP_THRESH_SECS) {
                            naseeks[i] = quant_aseek(paseeks[j] + paseeks[j + 1] * dt, in_arate);
                            weed_set_double_array(newframe, WEED_LEAF_AUDIO_SEEKS, natracks, naseeks);
			    // *INDENT-OFF*
			  }}}}
		    else break; // different audio on track
		  }}
		// *INDENT-ON*
                lives_freep((void **)&paclips);
                lives_freep((void **)&paseeks);
                if (j == patracks) xprev_aframe = get_prev_audio_frame_event(xprev_aframe);
                else break;
              }
            }
          }
#endif
          /// merge natracks with xatracks
          for (i = 0; i < natracks; i += 2) {
            for (j = 0; j < xatracks; j += 2) {
              if (naclips[i] == xaclips[j]) {
                xaclips[j + 1] = naclips[i + 1];
                xaseeks[j] = naseeks[i];
                xaseeks[j + 1] = naseeks[i + 1];
                break;
              }
            }
            if (j == xatracks) {
              xatracks += 2;
              xaclips = lives_realloc(xaclips, xatracks * sizint);
              xaseeks = lives_realloc(xaseeks, xatracks * sizdbl);
              xaclips[xatracks - 2] = naclips[i];
              xaclips[xatracks - 1] = naclips[i + 1];
              xaseeks[xatracks - 2] = naseeks[i];
              xaseeks[xatracks - 1] = naseeks[i + 1];
            }
          }
          natracks = 0;
          lives_freep((void **)&naclips);
          lives_freep((void **)&naseeks);
          lives_freep((void **)&naccels);
        }

        lives_freep((void **)&frames);
        lives_freep((void **)&clips);
#ifdef SMOOTH_AUD_VEL
        last_out_frame = newframe;
#endif

        /// frame insertion done

        if (deinit_events != NULL) {
          // insert filter_deinits
          for (list = deinit_events; list != NULL; list = list->next) {
            deinit_event = (weed_event_t *)list->data;
            //g_print("ins deinit %p\n", deinit_event);
            if (!copy_with_check(deinit_event, out_list, out_tc, what, 0)) {
              event_list_free(out_list);
              out_list = NULL;
              goto q_done;
            }
          }
          lives_list_free(deinit_events);
          deinit_events = NULL;
        }

        if (filter_map != NULL) {
          //g_print("ins filter map\n");
          if (!copy_with_check(filter_map, out_list, out_tc, what, 0)) {
            event_list_free(out_list);
            out_list = NULL;
            goto q_done;
          }
          filter_map = NULL;
        }
        if (is_final == 1) {
          is_final = 2;
        } else break; /// increase out_tc
      }
    } /// end of the in_list
  } /// end of out_list

  //g_print("RES: %p and %ld, %ld\n", event, out_tc + tl, end_tc);
  if (filter_map != NULL) {
    // insert final filter_map
    if (!copy_with_check(filter_map, out_list, end_tc, what, 0)) {
      event_list_free(out_list);
      out_list = NULL;
      goto q_done;
    }
  }

  if (get_first_frame_event(out_list) == NULL) {
    // make sure we have at least one frame
    if ((event = get_last_frame_event(in_list)) != NULL) {
      do {
        response = LIVES_RESPONSE_OK;
        lives_freep((void **)&clips);
        lives_freep((void **)&frames);
        tracks = weed_frame_event_get_tracks(event, &clips, &frames);
        if (insert_frame_event_at(out_list, 0., tracks, clips, frames, NULL) == NULL) {
          response = do_memory_error_dialog(what, 0);
        }
      } while (response == LIVES_RESPONSE_RETRY);
      if (response == LIVES_RESPONSE_CANCEL) {
        event_list_free(out_list);
        out_list = NULL;
        goto q_done;
      }
    }
  }

  /// for completeness we should add closers for all active audio tracks,
  /// however this will be done in event_list_rectify() when necessary (or ideally, the player would add the closers and
  /// record the offsets)

q_done:
  lives_list_free(init_events);
  lives_list_free(deinit_events);
  lives_free(what);
  return out_list;
}


//////////////////////////////////////////////////////////////////

static void on_reorder_activate(int rwidth, int rheight) {
  char *msg;

  uint32_t chk_mask = WARN_MASK_LAYOUT_ALTER_FRAMES | WARN_MASK_LAYOUT_ALTER_AUDIO;
  if (!check_for_layout_errors(NULL, mainw->current_file, 1, 0, &chk_mask)) {
    return;
  }

  cfile->old_frames = cfile->frames;

  //  we  do the reorder in reorder_frames()
  // this will clear event_list and set it in event_list_back
  if ((cfile->frames = reorder_frames(rwidth, rheight)) < 0) {
    // reordering error
    if (!(cfile->undo_action == UNDO_RESAMPLE)) {
      cfile->frames = -cfile->frames;
    }
    unbuffer_lmap_errors(FALSE);
    return;
  }

  if (mainw->cancelled != CANCEL_NONE) {
    return;
  }

  if (cfile->start > cfile->frames) {
    cfile->start = cfile->frames;
  }

  if (cfile->end > cfile->frames) {
    cfile->end = cfile->frames;
  }

  cfile->event_list = NULL;
  cfile->next_event = NULL;

  save_clip_value(mainw->current_file, CLIP_DETAILS_FRAMES, &cfile->frames);

  switch_to_file(mainw->current_file, mainw->current_file);
  if (mainw->current_file > 0) {
    d_print_done();
    msg = lives_strdup_printf(_("Length of video is now %d frames.\n"), cfile->frames);
  } else {
    msg = lives_strdup_printf(_("Clipboard was resampled to %d frames.\n"), cfile->frames);
  }

  d_print(msg);
  lives_free(msg);

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

  if (mainw->sl_undo_mem != NULL && cfile->stored_layout_frame != 0) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }
}


void on_resample_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // show the playback rate - real audio rate is cfile->arps
  mainw->fx1_val = cfile->arate;
  mainw->fx2_val = cfile->achans;
  mainw->fx3_val = cfile->asampsize;
  mainw->fx4_val = cfile->signed_endian;
  resaudw = create_resaudw(1, NULL, NULL);
  lives_widget_show(resaudw->dialog);
}


void on_resaudio_ok_clicked(LiVESButton * button, LiVESEntry * entry) {
  char *com;

  int arate, achans, asampsize, arps;
  int asigned = 1, aendian = 1;
  int cur_signed, cur_endian;
  int i;

  if (button != NULL) {
    arps = arate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
    achans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
    asampsize = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      asigned = 0;
    }
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      aendian = 0;
    }

    lives_widget_destroy(resaudw->dialog);
    lives_widget_context_update();
    lives_free(resaudw);

    if (arate <= 0) {
      do_error_dialog(_("\n\nNew rate must be greater than 0\n"));
      return;
    }
  } else {
    // called from on_redo or other places
    arate = arps = cfile->undo1_int;
    achans = cfile->undo2_int;
    asampsize = cfile->undo3_int;
    asigned = !(cfile->undo1_uint & AFORM_UNSIGNED);
    aendian = !(cfile->undo1_uint & AFORM_BIG_ENDIAN);
  }

  uint32_t chk_mask = WARN_MASK_LAYOUT_ALTER_FRAMES | WARN_MASK_LAYOUT_ALTER_AUDIO;
  if (!check_for_layout_errors(NULL, mainw->current_file, 1, 0, &chk_mask)) {
    return;
  }

  // store old values for undo/redo
  cfile->undo1_int = cfile->arate;
  cfile->undo2_int = cfile->achans;
  cfile->undo3_int = cfile->asampsize;
  cfile->undo4_int = cfile->arps;
  cfile->undo1_uint = cfile->signed_endian;

  cur_signed = !(cfile->signed_endian & AFORM_UNSIGNED);
  cur_endian = !(cfile->signed_endian & AFORM_BIG_ENDIAN);

  if (!(arate == cfile->arate && arps == cfile->arps && achans == cfile->achans && asampsize == cfile->asampsize &&
        asigned == cur_signed && aendian == cur_endian)) {
    if (cfile->arps != cfile->arate) {
      double audio_stretch = (double)cfile->arps / (double)cfile->arate;
      // pb rate != real rate - stretch to pb rate and resample
      lives_rm(cfile->info_file);
      com = lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d %.4f", prefs->backend,
                                cfile->handle, cfile->arps,
                                cfile->achans, cfile->asampsize, cur_signed, cur_endian, arps, cfile->achans, cfile->asampsize,
                                cur_signed, cur_endian, audio_stretch);
      mainw->com_failed = FALSE;
      lives_system(com, FALSE);
      if (mainw->com_failed) {
        unbuffer_lmap_errors(FALSE);
        return;
      }
      do_progress_dialog(TRUE, FALSE, _("Resampling audio")); // TODO - allow cancel ??
      lives_free(com);
      cfile->arate = cfile->arps = arps;
    } else {
      lives_rm(cfile->info_file);
      com = lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d", prefs->backend,
                                cfile->handle, cfile->arps,
                                cfile->achans, cfile->asampsize, cur_signed, cur_endian, arps, achans, asampsize, asigned, aendian);
      mainw->com_failed = FALSE;
      mainw->cancelled = CANCEL_NONE;
      mainw->error = FALSE;
      lives_rm(cfile->info_file);
      lives_system(com, FALSE);
      check_backend_return(cfile, NULL);
      if (mainw->com_failed) {
        unbuffer_lmap_errors(FALSE);
        return;
      }
      do_progress_dialog(TRUE, FALSE, _("Resampling audio"));
      lives_free(com);
    }
  }

  if (cfile->audio_waveform != NULL) {
    for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
    lives_freep((void **)&cfile->audio_waveform);
    lives_freep((void **)&cfile->aw_sizes);
  }

  cfile->arate = arate;
  cfile->achans = achans;
  cfile->asampsize = asampsize;
  cfile->arps = arps;
  cfile->signed_endian = get_signed_endian(asigned, aendian);
  cfile->changed = TRUE;

  cfile->undo_action = UNDO_AUDIO_RESAMPLE;
  mainw->error = FALSE;
  reget_afilesize(mainw->current_file);

  if (cfile->afilesize == 0l) {
    do_error_dialog(_("LiVES was unable to resample the audio as requested.\n"));
    on_undo_activate(NULL, NULL);
    set_undoable(_("Resample Audio"), FALSE);
    mainw->error = TRUE;
    unbuffer_lmap_errors(FALSE);
    return;
  }
  set_undoable(_("Resample Audio"), !prefs->conserve_space);

  save_clip_values(mainw->current_file);

  switch_to_file(mainw->current_file, mainw->current_file);

  d_print("");  // force printing of switch message

  d_print(_("Audio was resampled to %d Hz, %d channels, %d bit"), arate, achans, asampsize);

  if (cur_signed != asigned) {
    if (asigned == 1) {
      d_print(_(", signed"));
    } else {
      d_print(_(", unsigned"));
    }
  }
  if (cur_endian != aendian) {
    if (aendian == 1) {
      d_print(_(", little-endian"));
    } else {
      d_print(_(", big-endian"));
    }
  }
  d_print("\n");

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

  if (mainw->sl_undo_mem != NULL && cfile->stored_layout_audio > 0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }
}


static void on_resaudw_achans_changed(LiVESWidget * widg, livespointer user_data) {
  _resaudw *resaudw = (_resaudw *)user_data;
  //char *tmp;

  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widg))) {
    lives_widget_set_sensitive(resaudw->rb_signed, FALSE);
    lives_widget_set_sensitive(resaudw->rb_unsigned, FALSE);
    lives_widget_set_sensitive(resaudw->rb_bigend, FALSE);
    lives_widget_set_sensitive(resaudw->rb_littleend, FALSE);
    lives_widget_set_sensitive(resaudw->entry_arate, FALSE);
    lives_widget_set_sensitive(resaudw->entry_asamps, FALSE);
    lives_widget_set_sensitive(resaudw->entry_achans, FALSE);
    if (prefsw != NULL) {
      lives_widget_set_sensitive(prefsw->pertrack_checkbutton, FALSE);
      lives_widget_set_sensitive(prefsw->backaudio_checkbutton, FALSE);
    } else if (rdet != NULL) {
      lives_widget_set_sensitive(rdet->pertrack_checkbutton, FALSE);
      lives_widget_set_sensitive(rdet->backaudio_checkbutton, FALSE);
    }
  } else {
    if (atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps))) != 8) {
      lives_widget_set_sensitive(resaudw->rb_bigend, TRUE);
      lives_widget_set_sensitive(resaudw->rb_littleend, TRUE);
    }
    lives_widget_set_sensitive(resaudw->entry_arate, TRUE);
    lives_widget_set_sensitive(resaudw->entry_asamps, TRUE);
    lives_widget_set_sensitive(resaudw->entry_achans, TRUE);
    if (prefsw != NULL) {
      lives_widget_set_sensitive(prefsw->pertrack_checkbutton, TRUE);
      lives_widget_set_sensitive(prefsw->backaudio_checkbutton, TRUE);
    }
    if (rdet != NULL) {
      lives_widget_set_sensitive(rdet->backaudio_checkbutton, TRUE);
      lives_widget_set_sensitive(rdet->pertrack_checkbutton, TRUE);
    }
  }
}


void on_resaudw_asamps_changed(LiVESWidget * irrelevant, livespointer rubbish) {
  if (atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps))) == 8) {
    lives_widget_set_sensitive(resaudw->rb_bigend, FALSE);
    lives_widget_set_sensitive(resaudw->rb_littleend, FALSE);
    lives_widget_set_sensitive(resaudw->rb_signed, FALSE);
    lives_widget_set_sensitive(resaudw->rb_unsigned, TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned), TRUE);
  } else {
    lives_widget_set_sensitive(resaudw->rb_bigend, TRUE);
    lives_widget_set_sensitive(resaudw->rb_littleend, TRUE);
    if (atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps))) == 16) {
      lives_widget_set_sensitive(resaudw->rb_signed, TRUE);
      lives_widget_set_sensitive(resaudw->rb_unsigned, FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
    }
  }
}


void on_resample_video_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // change speed from the menu
  create_new_pb_speed(2);
  mainw->fx1_val = cfile->fps;
}


void on_resample_vid_ok(LiVESButton * button, LiVESEntry * entry) {
  weed_plant_t *real_back_list = NULL;
  weed_plant_t *new_event_list = NULL;
  double oundo1_dbl = cfile->undo1_dbl;
  LiVESResponseType response;
  ticks_t in_time = 0;
  double old_fps = cfile->fps;
  char *msg;
  char *what;
  boolean ratio_fps;
  boolean bad_header = FALSE;
  int old_frames;
  int ostart = cfile->start;
  int oend = cfile->end;

  mainw->error = FALSE;

  if (button != NULL) {
    lives_general_button_clicked(button, NULL);
    if (mainw->fx1_val == 0.) mainw->fx1_val = 1.;
  } else {
    mainw->fx1_val = cfile->undo1_dbl;
  }

  if (mainw->current_file < 0 || cfile->frames == 0) return;

  if (mainw->fx1_val == cfile->fps && cfile->event_list == NULL) return;

  real_back_list = cfile->event_list;
  what = lives_strdup(_("creating the event list for resampling"));

  if (cfile->event_list == NULL) {
    for (int64_t i64 = 1; i64 <= (int64_t)cfile->frames; i64++) {
      do {
        response = LIVES_RESPONSE_OK;
        new_event_list = append_frame_event(new_event_list, in_time, 1, &(mainw->current_file), &i64);
        if (new_event_list == NULL) {
          response = do_memory_error_dialog(what, 0);
        }
      } while (response == LIVES_RESPONSE_RETRY);
      if (response == LIVES_RESPONSE_CANCEL) {
        lives_free(what);
        return;
      }
      in_time += (ticks_t)(1. / cfile->fps * TICKS_PER_SECOND_DBL + .5);
    }
    cfile->event_list = new_event_list;
  }
  cfile->undo1_dbl = cfile->fps;

  if (cfile->event_list_back != NULL) event_list_free(cfile->event_list_back);
  cfile->event_list_back = cfile->event_list;

  //QUANTISE
  new_event_list = quantise_events(cfile->event_list_back, mainw->fx1_val, real_back_list != NULL);
  cfile->event_list = new_event_list;

  if (real_back_list == NULL) event_list_free(cfile->event_list_back);
  cfile->event_list_back = NULL;

  if (cfile->event_list == NULL) {
    cfile->event_list = real_back_list;
    cfile->undo1_dbl = oundo1_dbl;
    mainw->error = TRUE;
    return;
  }

  if (mainw->multitrack != NULL) return;

  ratio_fps = check_for_ratio_fps(mainw->fx1_val);

  // we have now quantised to fixed fps; we have come here from reorder

  if (ratio_fps) {
    // got a ratio
    msg = lives_strdup_printf(_("Resampling video at %.8f frames per second..."), mainw->fx1_val);
  } else {
    msg = lives_strdup_printf(_("Resampling video at %.3f frames per second..."), mainw->fx1_val);
  }
  if (mainw->current_file > 0) {
    d_print(msg);
  }
  lives_free(msg);

  old_frames = cfile->frames;

  // must set these before calling reorder
  cfile->start = (int)((cfile->start - 1.) / old_fps * mainw->fx1_val + 1.);
  if ((cfile->end = (int)((cfile->end * mainw->fx1_val) / old_fps + .49999)) < cfile->start) cfile->end = cfile->start;

  cfile->undo_action = UNDO_RESAMPLE;
  // REORDER
  // this calls reorder_frames, which sets event_list_back==event_list, and clears event_list
  on_reorder_activate(reorder_width, reorder_height);

  if (cfile->frames <= 0 || mainw->cancelled != CANCEL_NONE) {
    // reordering error...
    cfile->event_list = real_back_list;
    if (cfile->event_list_back != NULL) event_list_free(cfile->event_list_back);
    cfile->event_list_back = NULL;
    cfile->frames = old_frames;
    cfile->start = ostart;
    cfile->end = oend;
    load_end_image(cfile->end);
    load_start_image(cfile->start);
    cfile->undo1_dbl = oundo1_dbl;
    sensitize();
    mainw->error = TRUE;
    if (cfile->frames < 0) do_error_dialog(_("Reordering error !\n"));
    return;
  }

  if (cfile->event_list_back != NULL) event_list_free(cfile->event_list_back);
  cfile->event_list_back = real_back_list;

  cfile->ratio_fps = ratio_fps;
  cfile->pb_fps = cfile->fps = mainw->fx1_val;
  cfile->old_frames = old_frames;

  set_undoable(_("Resample"), TRUE);
  if (cfile->clip_type == CLIP_TYPE_FILE && cfile->ext_src) {
    lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
    double dfps = (double)cdata->fps;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &dfps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->fps)) bad_header = TRUE;
  } else {
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->pb_fps)) bad_header = TRUE;
  }

  if (bad_header) do_header_write_error(mainw->current_file);

  switch_to_file(mainw->current_file, mainw->current_file);
}


///////// GUI stuff /////////////////////////////////////////////////////

_resaudw *create_resaudw(short type, render_details * rdet, LiVESWidget * top_vbox) {
  // type 1 == resample
  // type 2 == insert silence
  // type 3 == enter multitrack
  // type 4 == prefs/multitrack
  // type 5 == new clip record/record to selection with no existing audio
  // type 6 == record to clip with no existing audio
  // type 7 == record to clip with existing audio (show time only)
  // type 8 == grab external window, with audio
  // type 9 == grab external, no audio
  // type 10 == change inside multitrack / render to clip (embedded) [resets to 3]
  // type 11 == rte audio gen as rfx

  LiVESWidget *dialog_vbox = NULL;
  LiVESWidget *vboxx;
  LiVESWidget *vbox2;
  LiVESWidget *frame;
  LiVESWidget *combo_entry2;
  LiVESWidget *combo_entry3;
  LiVESWidget *combo_entry1;
  LiVESWidget *vseparator;
  LiVESWidget *radiobutton_u1;
  LiVESWidget *radiobutton_s1;
  LiVESWidget *vbox;
  LiVESWidget *radiobutton_b1;
  LiVESWidget *radiobutton_l1;
  LiVESWidget *combo4;
  LiVESWidget *combo5;
  LiVESWidget *combo6;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;
  LiVESWidget *hseparator;
  LiVESWidget *radiobutton;
  LiVESWidget *hbox;
  LiVESWidget *hbox2;

  LiVESAccelGroup *accel_group = NULL;

  LiVESSList *s1_group = NULL;
  LiVESSList *e1_group = NULL;
  LiVESSList *s2_group = NULL;
  LiVESSList *e2_group = NULL;
  LiVESSList *rbgroup = NULL;

  LiVESList *channels = NULL;
  LiVESList *sampsize = NULL;
  LiVESList *rate = NULL;

  double secs = 0.;

  char *tmp;

  int hours = 0, mins = 0;
  int aendian;

  boolean chans_fixed = FALSE;
  boolean is_8bit;

  _resaudw *resaudw = (_resaudw *)(lives_malloc(sizeof(_resaudw)));

  if (type == 10) {
    chans_fixed = TRUE;
    type = 3;
  }

  if (type > 5 && type != 11 && mainw->rec_end_time != -1.) {
    hours = (int)(mainw->rec_end_time / 3600.);
    mins = (int)((mainw->rec_end_time - (hours * 3600.)) / 60.);
    secs = mainw->rec_end_time - hours * 3600. - mins * 60.;
  }

  channels = lives_list_append(channels, (livespointer)"1");
  channels = lives_list_append(channels, (livespointer)"2");

  sampsize = lives_list_append(sampsize, (livespointer)"8");
  sampsize = lives_list_append(sampsize, (livespointer)"16");

  rate = lives_list_append(rate, (livespointer)"5512");
  rate = lives_list_append(rate, (livespointer)"8000");
  rate = lives_list_append(rate, (livespointer)"11025");
  rate = lives_list_append(rate, (livespointer)"22050");
  rate = lives_list_append(rate, (livespointer)"32000");
  rate = lives_list_append(rate, (livespointer)"44100");
  rate = lives_list_append(rate, (livespointer)"48000");
  rate = lives_list_append(rate, (livespointer)"88200");
  rate = lives_list_append(rate, (livespointer)"96000");
  rate = lives_list_append(rate, (livespointer)"128000");

  if (type < 3 || type > 4) {
    char *title = NULL;

    if (type == 1) {
      title = lives_strdup(_("Resample Audio"));
    } else if (type == 2) {
      title = lives_strdup(_("Insert Silence"));
    } else if (type == 5 || type == 11 || type == 6 || type == 7) {
      title = lives_strdup(_("New Clip Audio"));
    } else if (type == 9 || type == 8) {
      title = lives_strdup(_("External Clip Settings"));
    }

    resaudw->dialog = lives_standard_dialog_new(title, FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
    lives_signal_handlers_disconnect_by_func(resaudw->dialog, LIVES_GUI_CALLBACK(return_true), NULL);
    lives_free(title);

    accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    lives_window_add_accel_group(LIVES_WINDOW(resaudw->dialog), accel_group);

    dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(resaudw->dialog));

    vboxx = lives_vbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), vboxx, TRUE, TRUE, 0);
  } else vboxx = top_vbox;

  if (type == 1) {
    frame = lives_standard_frame_new(_("Current"), 0., FALSE);

    lives_box_pack_start(LIVES_BOX(vboxx), frame, FALSE, TRUE, 0);

    hbox2 = lives_hbox_new(FALSE, 0);
    lives_container_add(LIVES_CONTAINER(frame), hbox2);
    lives_container_set_border_width(LIVES_CONTAINER(hbox2), widget_opts.packing_width);

    tmp = lives_strdup_printf("%d", (int)mainw->fx1_val);

    combo_entry2 = lives_standard_entry_new(_("Rate (Hz) "), tmp, 10, 6, LIVES_BOX(hbox2), NULL);
    lives_free(tmp);

    lives_editable_set_editable(LIVES_EDITABLE(combo_entry2), FALSE);
    lives_widget_set_can_focus(combo_entry2, FALSE);

    tmp = lives_strdup_printf("%d", (int)mainw->fx2_val);
    combo_entry3 = lives_standard_entry_new(_("Channels"), tmp, 6, 2, LIVES_BOX(hbox2), NULL);
    lives_free(tmp);

    lives_editable_set_editable(LIVES_EDITABLE(combo_entry3), FALSE);
    lives_widget_set_can_focus(combo_entry3, FALSE);

    tmp = lives_strdup_printf("%d", (int)mainw->fx3_val);
    combo_entry1 = lives_standard_entry_new(_("Sample Size "), tmp, 6, 2, LIVES_BOX(hbox2), NULL);
    lives_free(tmp);

    lives_editable_set_editable(LIVES_EDITABLE(combo_entry1), FALSE);
    lives_widget_set_can_focus(combo_entry1, FALSE);

    vseparator = lives_vseparator_new();
    lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_s1 = lives_standard_radio_button_new(_("Signed"), &s1_group, LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_u1 = lives_standard_radio_button_new(_("Unsigned"), &s1_group, LIVES_BOX(hbox), NULL);

    aendian = mainw->fx4_val;

    if (aendian & AFORM_UNSIGNED) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_u1), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_s1), TRUE);
    }

    lives_widget_set_sensitive(radiobutton_u1, FALSE);
    lives_widget_set_sensitive(radiobutton_s1, FALSE);

    vseparator = lives_vseparator_new();
    lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_l1 = lives_standard_radio_button_new(_("Little Endian"), &e1_group, LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_b1 = lives_standard_radio_button_new(_("Big Endian"), &e1_group, LIVES_BOX(hbox), NULL);

    if (aendian & AFORM_BIG_ENDIAN) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_b1), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_l1), TRUE);
    }

    lives_widget_set_sensitive(radiobutton_b1, FALSE);
    lives_widget_set_sensitive(radiobutton_l1, FALSE);

  }

  resaudw->aud_checkbutton = NULL;

  if (type < 9 || type == 11) {
    if (type >= 3 && type != 11) tmp = lives_strdup(_("Audio"));
    else if (type == 2) tmp = lives_strdup(_("New Audio Details"));
    else tmp = lives_strdup(_("New"));

    frame = lives_standard_frame_new(tmp, 0., FALSE);
    lives_free(tmp);

    if (type == 4) lives_box_pack_start(LIVES_BOX(vboxx), frame, FALSE, FALSE, widget_opts.packing_height);
    else lives_box_pack_start(LIVES_BOX(vboxx), frame, FALSE, TRUE, 0);

    vbox2 = lives_vbox_new(FALSE, 0);
    lives_container_add(LIVES_CONTAINER(frame), vbox2);

    if (type > 2 && type < 5 && !chans_fixed) {
      resaudw->aud_hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(vbox2), resaudw->aud_hbox, FALSE, FALSE, 0);

      resaudw->aud_checkbutton = lives_standard_check_button_new(_("_Enable audio"), FALSE, LIVES_BOX(resaudw->aud_hbox), NULL);

      if (rdet != NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), rdet->achans > 0);
      else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton),
                                            mainw->multitrack == NULL ? prefs->mt_def_achans > 0 : cfile->achans > 0);
      if (type == 4) {
        lives_signal_connect(LIVES_GUI_OBJECT(resaudw->aud_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
      }
    }

    hbox2 = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox2), hbox2, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(hbox2), widget_opts.border_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, widget_opts.packing_width);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(hbox), widget_opts.border_width);

    combo4 = lives_standard_combo_new(_("Rate (Hz) "), rate, LIVES_BOX(hbox), NULL);

    resaudw->entry_arate = lives_combo_get_entry(LIVES_COMBO(combo4));

    lives_entry_set_width_chars(LIVES_ENTRY(resaudw->entry_arate), 6);
    if (type == 7) lives_widget_set_sensitive(combo4, FALSE);

    if (type < 3 || (type > 4 && type < 8) || type == 11) tmp = lives_strdup_printf("%d", (int)mainw->fx1_val);
    else if (type == 8) tmp = lives_strdup_printf("%d", DEFAULT_AUDIO_RATE);
    else if (type == 3) tmp = lives_strdup_printf("%d", rdet->arate);
    else tmp = mainw->multitrack == NULL ||
                 cfile->achans == 0 ? lives_strdup_printf("%d", prefs->mt_def_arate) : lives_strdup_printf("%d", cfile->arate);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_arate), tmp);
    lives_free(tmp);

    if (type == 4) {
      lives_signal_connect(LIVES_GUI_OBJECT(combo4), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
    }

    combo5 = lives_standard_combo_new((type >= 3 && type != 11 ? (_("_Channels")) : (_("Channels"))),
                                      channels, LIVES_BOX(hbox), NULL);

    if (type == 7) lives_widget_set_sensitive(combo5, FALSE);

    resaudw->entry_achans = lives_combo_get_entry(LIVES_COMBO(combo5));
    lives_entry_set_width_chars(LIVES_ENTRY(resaudw->entry_achans), 2);

    if (type < 3 || (type > 4 && type < 8) || type == 11) tmp = lives_strdup_printf("%d", (int)mainw->fx2_val);
    else if (type == 8) tmp = lives_strdup_printf("%d", DEFAULT_AUDIO_CHANS);
    else if (type == 3) tmp = lives_strdup_printf("%d", rdet->achans);
    else tmp = lives_strdup_printf("%d", mainw->multitrack == NULL ||
                                     cfile->achans == 0 ? (prefs->mt_def_achans == 0 ? DEFAULT_AUDIO_CHANS : prefs->mt_def_achans) : cfile->achans);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_achans), tmp);
    lives_free(tmp);

    if (chans_fixed) {
      lives_widget_set_sensitive(resaudw->entry_achans, FALSE);
      lives_widget_set_sensitive(combo5, FALSE);
    }

    if (type == 4) {
      lives_signal_connect(LIVES_GUI_OBJECT(combo5), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
    }

    combo6 = lives_standard_combo_new((type >= 3 && type != 11 ? (_("_Sample Size")) : (_("Sample Size"))),
                                      sampsize, LIVES_BOX(hbox), NULL);

    if (type == 7) lives_widget_set_sensitive(combo6, FALSE);

    resaudw->entry_asamps = lives_combo_get_entry(LIVES_COMBO(combo6));
    lives_entry_set_max_length(LIVES_ENTRY(resaudw->entry_asamps), 2);
    lives_editable_set_editable(LIVES_EDITABLE(resaudw->entry_asamps), FALSE);
    lives_entry_set_width_chars(LIVES_ENTRY(resaudw->entry_asamps), 2);

    if (type < 3 || (type > 4 && type < 8) || type == 11) tmp = lives_strdup_printf("%d", (int)mainw->fx3_val);
    else if (type == 8) tmp = lives_strdup_printf("%d", DEFAULT_AUDIO_SAMPS);
    else if (type == 3) tmp = lives_strdup_printf("%d", rdet->asamps);
    else tmp = lives_strdup_printf("%d", mainw->multitrack == NULL || cfile->achans == 0 ? prefs->mt_def_asamps : cfile->asampsize);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_asamps), tmp);

    if (!strcmp(tmp, "8")) is_8bit = TRUE;
    else is_8bit = FALSE;

    lives_free(tmp);

    if (type == 4) {
      lives_signal_connect(LIVES_GUI_OBJECT(combo6), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
    }

    vseparator = lives_vseparator_new();
    if (type != 4) lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    if (type != 4) lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_signed = lives_standard_radio_button_new(_("Signed"), &s2_group, LIVES_BOX(hbox), NULL);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
    if (type == 7 || is_8bit) lives_widget_set_sensitive(resaudw->rb_signed, FALSE);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_unsigned = lives_standard_radio_button_new(_("Unsigned"), &s2_group, LIVES_BOX(hbox), NULL);

    if (type == 7 || !is_8bit) lives_widget_set_sensitive(resaudw->rb_unsigned, FALSE);

    if (type < 3 || (type > 4 && type < 8) || type == 11) aendian = mainw->fx4_val;
    else if (type == 8) aendian = DEFAULT_AUDIO_SIGNED16 | ((capable->byte_order == LIVES_BIG_ENDIAN) ? AFORM_BIG_ENDIAN : 0);
    else if (type == 3) aendian = rdet->aendian;
    else aendian = mainw->multitrack == NULL || cfile->achans == 0 ? prefs->mt_def_signed_endian : cfile->signed_endian;

    if (aendian & AFORM_UNSIGNED) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
    }

    if (type == 4) {
      lives_signal_connect(LIVES_GUI_OBJECT(resaudw->rb_signed), LIVES_WIDGET_TOGGLED_SIGNAL,
                           LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
      lives_signal_connect(LIVES_GUI_OBJECT(resaudw->rb_unsigned), LIVES_WIDGET_TOGGLED_SIGNAL,
                           LIVES_GUI_CALLBACK(apply_button_set_enabled),
                           NULL);
    }

    vseparator = lives_vseparator_new();
    lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_littleend = lives_standard_radio_button_new(_("Little Endian"), &e2_group, LIVES_BOX(hbox), NULL);

    if (type == 7) lives_widget_set_sensitive(resaudw->rb_littleend, FALSE);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_bigend = lives_standard_radio_button_new(_("Big Endian"), &e2_group, LIVES_BOX(hbox), NULL);

    if (type == 7) lives_widget_set_sensitive(resaudw->rb_bigend, FALSE);

    if (aendian & AFORM_BIG_ENDIAN) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend), TRUE);
    }

    if (!strcmp(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)), "8")) {
      lives_widget_set_sensitive(resaudw->rb_littleend, FALSE);
      lives_widget_set_sensitive(resaudw->rb_bigend, FALSE);
    }

    lives_signal_connect(LIVES_GUI_OBJECT(resaudw->entry_asamps), LIVES_WIDGET_CHANGED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_resaudw_asamps_changed),
                         NULL);
  }

  if (type == 4) {
    lives_signal_connect(LIVES_GUI_OBJECT(resaudw->rb_littleend), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(apply_button_set_enabled),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(resaudw->rb_bigend), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  }

  if (type > 7 && type != 11) {
    frame = lives_standard_frame_new(_("Video"), 0., FALSE);
    lives_box_pack_start(LIVES_BOX(vboxx), frame, TRUE, TRUE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_container_add(LIVES_CONTAINER(frame), hbox);
    lives_container_set_border_width(LIVES_CONTAINER(hbox), widget_opts.border_width);

    resaudw->fps_spinbutton = lives_standard_spin_button_new(_("_Frames Per Second "),
                              prefs->default_fps, 1., FPS_MAX, 1., 1., 3, LIVES_BOX(hbox), NULL);
  }

  if (type > 4 && type != 11) {
    lives_box_set_spacing(LIVES_BOX(dialog_vbox), widget_opts.packing_height * 3);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

    if (type != 6 && type != 7) {
      radiobutton = lives_standard_radio_button_new(_("Record for maximum:  "), &rbgroup, LIVES_BOX(hbox), NULL);

      resaudw->hour_spinbutton = lives_standard_spin_button_new(_(" hours  "), hours,
                                 0., hours > 23 ? hours : 23, 1., 1., 0, LIVES_BOX(hbox), NULL);

      resaudw->minute_spinbutton = lives_standard_spin_button_new(_(" minutes  "), mins, 0., 59., 1., 10., 0, LIVES_BOX(hbox), NULL);

      resaudw->second_spinbutton = lives_standard_spin_button_new(_(" seconds  "), secs, 0., 59., 1., 10., 0, LIVES_BOX(hbox), NULL);

      hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

      resaudw->unlim_radiobutton = lives_standard_radio_button_new(_("Unlimited"), &rbgroup, LIVES_BOX(hbox), NULL);

      lives_signal_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_rb_audrec_time_toggled),
                           (livespointer)resaudw);

      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton), (type == 5 || type > 7) && type != 11);

    }

    if (type < 8 || type == 11) {
      hseparator = lives_hseparator_new();
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hseparator, TRUE, TRUE, 0);

      label = lives_standard_label_new(_("Click OK to begin recording, or Cancel to quit."));

      lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
    }
  }

  if (type < 3 || type > 4) {
    cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(resaudw->dialog), LIVES_STOCK_CANCEL, NULL,
                   LIVES_RESPONSE_CANCEL);

    if (accel_group != NULL) lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
          LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(resaudw->dialog), LIVES_STOCK_OK, NULL,
               LIVES_RESPONSE_OK);

    lives_button_grab_default_special(okbutton);

    if (type < 8 || type == 11) {
      lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                           LIVES_GUI_CALLBACK(lives_general_button_clicked),
                           resaudw);

      if (type == 1) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_resaudio_ok_clicked),
                             NULL);
      } else if (type == 2 || type == 11) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_ins_silence_details_clicked),
                             NULL);
      } else if (type == 5) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_recaudclip_ok_clicked),
                             LIVES_INT_TO_POINTER(0));
      } else if (type == 6 || type == 7) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_recaudclip_ok_clicked),
                             LIVES_INT_TO_POINTER(1));
      }

    }

    lives_widget_show_all(resaudw->dialog);
  } else {
    if (resaudw->aud_checkbutton != NULL) {
      lives_signal_connect_after(LIVES_GUI_OBJECT(resaudw->aud_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_resaudw_achans_changed),
                                 (livespointer)resaudw);
      on_resaudw_achans_changed(resaudw->aud_checkbutton, (livespointer)resaudw);
    }
  }

  lives_widget_show_all(vboxx);

  lives_list_free(channels);
  lives_list_free(sampsize);
  lives_list_free(rate);

  return resaudw;
}


void on_change_speed_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // change speed from the menu
  create_new_pb_speed(1);
  mainw->fx1_bool = mainw->fx2_bool = FALSE;
  mainw->fx1_val = cfile->fps;
}


void on_change_speed_ok_clicked(LiVESButton * button, livespointer user_data) {
  double arate = cfile->arate / cfile->fps;
  char *msg;
  boolean bad_header = FALSE;
  int new_frames = count_resampled_frames(cfile->frames, mainw->fx1_val, cfile->fps);

  // change playback rate
  if (button != NULL) {
    lives_general_button_clicked(button, NULL);
  }

  if (mainw->fx2_bool) {
    mainw->fx1_val = (double)((int)((double)cfile->frames / mainw->fx2_val * 1000. + .5)) / 1000.;
    if (mainw->fx1_val < 1.) mainw->fx1_val = 1.;
    if (mainw->fx1_val > FPS_MAX) mainw->fx1_val = FPS_MAX;
  }

  char *tmp = lives_strdup(_("Changing the clip fps"));
  uint32_t chk_mask = WARN_MASK_LAYOUT_DELETE_FRAMES | WARN_MASK_LAYOUT_SHIFT_FRAMES
                      | WARN_MASK_LAYOUT_ALTER_FRAMES;
  if (mainw->fx1_bool) chk_mask |= WARN_MASK_LAYOUT_DELETE_AUDIO | WARN_MASK_LAYOUT_SHIFT_AUDIO
                                     | WARN_MASK_LAYOUT_ALTER_AUDIO;
  if (!check_for_layout_errors(tmp, mainw->current_file, 1, new_frames, &chk_mask)) {
    lives_free(tmp);
    return;
  }
  lives_free(tmp);

  if (button == NULL) {
    mainw->fx1_bool = !(cfile->undo1_int == cfile->arate);
    mainw->fx1_val = cfile->undo1_dbl;
  }

  set_undoable(_("Speed Change"), TRUE);
  cfile->undo1_dbl = cfile->fps;
  cfile->undo1_int = cfile->arate;
  cfile->undo_action = UNDO_CHANGE_SPEED;

  if (mainw->fx1_val == 0.) mainw->fx1_val = 1.;

  // update the frame rate
  cfile->pb_fps = cfile->fps = mainw->fx1_val;

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->pb_fps);

  if (mainw->fx1_bool) {
    cfile->arate = (int)(arate * cfile->fps + .5);
    msg = lives_strdup_printf(_("Changed playback speed to %.3f frames per second and audio to %d Hz.\n"), cfile->fps,
                              cfile->arate);
  } else {
    msg = lives_strdup_printf(_("Changed playback speed to %.3f frames per second.\n"), cfile->fps);
  }
  d_print(msg);
  lives_free(msg);

  cfile->ratio_fps = FALSE;

  if (cfile->clip_type == CLIP_TYPE_FILE && cfile->ext_src) {
    lives_clip_data_t *cdata = ((lives_decoder_t *)cfile->ext_src)->cdata;
    double dfps = (double)cdata->fps;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &dfps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->fps)) bad_header = TRUE;
  } else {
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_FPS, &cfile->fps)) bad_header = TRUE;
    if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_FPS, &cfile->pb_fps)) bad_header = TRUE;
  }

  if (!save_clip_value(mainw->current_file, CLIP_DETAILS_PB_ARATE, &cfile->arate)) bad_header = TRUE;
  if (bad_header) do_header_write_error(mainw->current_file);

  switch_to_file(mainw->current_file, mainw->current_file);

  if (chk_mask != 0) popup_lmap_errors(NULL, LIVES_INT_TO_POINTER(chk_mask));

  if (mainw->sl_undo_mem != NULL && cfile->stored_layout_frame != 0) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }
}


int reorder_frames(int rwidth, int rheight) {
  int new_frames = cfile->old_frames;
  int cur_frames = cfile->frames;
  char **array;
  char *com;

  if (rwidth * rheight == 0) com = lives_strdup_printf("%s reorder \"%s\" \"%s\" %d 0 0 %d %d", prefs->backend, cfile->handle,
                                     get_image_ext_for_type(cfile->img_type), !mainw->endian,
                                     reorder_leave_back, cfile->frames);
  else {
    if (!prefs->enc_letterbox) {
      com = lives_strdup_printf("%s reorder \"%s\" \"%s\" %d %d %d 0 %d", prefs->backend, cfile->handle,
                                get_image_ext_for_type(cfile->img_type), !mainw->endian, rwidth, rheight, cfile->frames);
    } else {
      int iwidth = cfile->hsize, iheight = cfile->vsize;
      calc_maxspect(rwidth, rheight, &iwidth, &iheight);

      if (iwidth == cfile->hsize && iheight == cfile->vsize) {
        iwidth = -iwidth;
        iheight = -iheight;
      }

      else {
        if (LETTERBOX_NEEDS_COMPOSITE && !capable->has_composite) {
          do_lb_composite_error();
          return -cur_frames;
        }

        if (LETTERBOX_NEEDS_CONVERT && !capable->has_convert) {
          do_lb_convert_error();
          return -cur_frames;
        }
      }

      com = lives_strdup_printf("%s reorder \"%s\" \"%s\" %d %d %d %d %d %d %d", prefs->backend, cfile->handle,
                                get_image_ext_for_type(cfile->img_type), !mainw->endian, rwidth, rheight,
                                reorder_leave_back, cfile->frames, iwidth, iheight);
    }
  }

  cfile->frames = 0;

  cfile->progress_start = 1;
  cfile->progress_end = save_event_frames(); // we convert cfile->event_list to a block and save it

  if (cfile->progress_end == -1) return -cur_frames; // save_event_frames failed

  if (cur_frames > cfile->progress_end) cfile->progress_end = cur_frames;

  cfile->next_event = NULL;
  if (cfile->event_list != NULL) {
    if (cfile->event_list_back != NULL) event_list_free(cfile->event_list_back);
    cfile->event_list_back = cfile->event_list;
    cfile->event_list = NULL;
  }

  lives_rm(cfile->info_file);
  mainw->error = FALSE;
  mainw->com_failed = FALSE;
  lives_system(com, FALSE);
  if (mainw->com_failed) return -cur_frames;

  if (cfile->undo_action == UNDO_RESAMPLE) {
    if (mainw->current_file > 0) {
      cfile->nopreview = cfile->nokeep = TRUE;
      if (!do_progress_dialog(TRUE, TRUE, _("Resampling video"))) {
        cfile->nopreview = cfile->nokeep = FALSE;
        return cur_frames;
      }
      cfile->nopreview = cfile->nokeep = FALSE;
    } else {
      do_progress_dialog(TRUE, FALSE, _("Resampling clipboard video"));
    }
  } else {
    cfile->nopreview = cfile->nokeep = TRUE;
    if (!do_progress_dialog(TRUE, TRUE, _("Reordering frames"))) {
      cfile->nopreview = cfile->nokeep = FALSE;
      return cur_frames;
    }
    cfile->nopreview = cfile->nokeep = FALSE;
  }
  lives_free(com);

  if (mainw->error) {
    if (mainw->cancelled != CANCEL_ERROR) do_error_dialog(_("\n\nLiVES was unable to reorder the frames."));
    deorder_frames(new_frames, FALSE);
    new_frames = -new_frames;
  } else {
    array = lives_strsplit(mainw->msg, "|", 2);

    new_frames = atoi(array[1]);
    lives_strfreev(array);

    if (cfile->frames > new_frames) {
      new_frames = cfile->frames;
    }
  }

  return new_frames;
}


int deorder_frames(int old_frames, boolean leave_bak) {
  char *com;
  ticks_t time_start;
  int perf_start, perf_end;

  if (cfile->event_list != NULL) return cfile->frames;

  cfile->event_list = cfile->event_list_back;
  cfile->event_list_back = NULL;

  if (cfile->event_list == NULL) {
    perf_start = 1;
    perf_end = old_frames;
  } else {
    time_start = get_event_timecode(get_first_event(cfile->event_list));
    perf_start = (int)(cfile->fps * (double)time_start / TICKS_PER_SECOND_DBL) + 1;
    perf_end = perf_start + count_events(cfile->event_list, FALSE, 0, 0) - 1;
  }
  com = lives_strdup_printf("%s deorder \"%s\" %d %d %d \"%s\" %d", prefs->backend, cfile->handle,
                            perf_start, cfile->frames, perf_end,
                            get_image_ext_for_type(cfile->img_type), leave_bak);

  lives_rm(cfile->info_file);
  mainw->com_failed = FALSE;
  lives_system(com, TRUE);
  if (mainw->com_failed) return cfile->frames;

  do_progress_dialog(TRUE, FALSE, _("Deordering frames"));
  lives_free(com);

  // check for EOF

  if (cfile->frame_index_back != NULL) {
    int current_frames = cfile->frames;
    cfile->frames = old_frames;
    restore_frame_index_back(mainw->current_file);
    cfile->frames = current_frames;
  }

  return old_frames;
}


boolean resample_clipboard(double new_fps) {
  // resample the clipboard video - if we already did it once, it is
  // quicker the second time
  char *com;
  int current_file = mainw->current_file;

  mainw->no_switch_dprint = TRUE;

  if (clipboard->undo1_dbl == new_fps && !prefs->conserve_space) {
    int new_frames;
    double old_fps = clipboard->fps;

    if (new_fps == clipboard->fps) {
      mainw->no_switch_dprint = FALSE;
      return TRUE;
    }

    // we already resampled to this fps
    new_frames = count_resampled_frames(clipboard->frames, clipboard->fps, new_fps);

    mainw->current_file = 0;

    // copy .mgk to .img_ext and .img_ext to .bak (i.e redo the resample)
    com = lives_strdup_printf("%s redo \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle, 1, new_frames,
                              get_image_ext_for_type(cfile->img_type));
    lives_rm(cfile->info_file);
    mainw->com_failed = FALSE;
    lives_system(com, FALSE);

    if (mainw->com_failed) {
      mainw->no_switch_dprint = FALSE;
      d_print_failed();
      return FALSE;
    }

    cfile->progress_start = 1;
    cfile->progress_end = new_frames;
    cfile->old_frames = cfile->frames;
    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE, FALSE, _("Resampling clipboard video"));
    lives_free(com);
    cfile->frames = new_frames;
    cfile->undo_action = UNDO_RESAMPLE;
    cfile->fps = cfile->undo1_dbl;
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->fps);
    cfile->undo1_dbl = old_fps;
    d_print(_("Clipboard was resampled to %d frames.\n"), cfile->frames);
    mainw->current_file = current_file;
  } else {
    if (clipboard->undo1_dbl < clipboard->fps) {
      int old_frames = count_resampled_frames(clipboard->frames, clipboard->fps, clipboard->undo1_dbl);
      mainw->current_file = 0;
      com = lives_strdup_printf("%s undo \"%s\" %d %d \"%s\"", prefs->backend, cfile->handle, old_frames + 1, cfile->frames,
                                get_image_ext_for_type(cfile->img_type));
      lives_rm(cfile->info_file);
      lives_system(com, FALSE);
      cfile->progress_start = old_frames + 1;
      cfile->progress_end = cfile->frames;
      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE, FALSE, _("Resampling clipboard video"));
      lives_free(com);
    }

    // resample to cfile fps
    mainw->current_file = current_file;
    clipboard->undo1_dbl = new_fps;

    if (new_fps == clipboard->fps) {
      mainw->no_switch_dprint = FALSE;
      return TRUE;
    }

    mainw->current_file = 0;
    on_resample_vid_ok(NULL, NULL);
    mainw->current_file = current_file;
    if (clipboard->fps != new_fps) {
      d_print(_("resampling error..."));
      mainw->error = 1;
      mainw->no_switch_dprint = FALSE;
      return FALSE;
    }
    // clipboard->fps now holds new_fps, clipboard->undo1_dbl holds orig fps
    // BUT we will later undo this, then clipboard->fps will hold orig fps,
    // clipboard->undo1_dbl will hold resampled fps

  }

  mainw->no_switch_dprint = FALSE;
  return TRUE;
}
