// player.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "callbacks.h"
#include "resample.h"
#include "effects-weed.h"
#include "effects.h"
#include "cvirtual.h"
#include "diagnostics.h"
#include "paramwindow.h"
#include "ce_thumbs.h"

LIVES_GLOBAL_INLINE int lives_set_status(int status) {
  mainw->status |= status;
  return status;
}

LIVES_GLOBAL_INLINE int lives_unset_status(int status) {
  mainw->status &= ~status;
  return status;
}

LIVES_GLOBAL_INLINE boolean lives_has_status(int status) {return (mainw->status & status) ? TRUE : FALSE;}

int lives_get_status(void) {
  // TODO - replace fields with just status bits
  if (!mainw) return LIVES_STATUS_NOTREADY;

  if (!mainw->is_ready) {
    mainw->status = LIVES_STATUS_NOTREADY;
    return mainw->status;
  }

  lives_unset_status(LIVES_STATUS_NOTREADY);

  if (mainw->is_exiting) lives_set_status(LIVES_STATUS_EXITING);
  else lives_unset_status(LIVES_STATUS_EXITING);

  if (mainw->fatal) lives_set_status(LIVES_STATUS_FATAL);
  else lives_unset_status(LIVES_STATUS_FATAL);

  if (mainw->error) lives_set_status(LIVES_STATUS_ERROR);
  else lives_unset_status(LIVES_STATUS_ERROR);

  if (mainw->is_processing) lives_set_status(LIVES_STATUS_PROCESSING);
  else lives_unset_status(LIVES_STATUS_PROCESSING);

  if (LIVES_IS_PLAYING) lives_set_status(LIVES_STATUS_PLAYING);
  else lives_unset_status(LIVES_STATUS_PLAYING);

  if (LIVES_IS_RENDERING) lives_set_status(LIVES_STATUS_RENDERING);
  else lives_unset_status(LIVES_STATUS_RENDERING);

  if (mainw->preview || mainw->preview_rendering) lives_set_status(LIVES_STATUS_PREVIEW);
  else lives_unset_status(LIVES_STATUS_PREVIEW);

  return mainw->status;
}


void get_player_size(int *opwidth, int *opheight) {
  // calc output size for display
  int rwidth, rheight;

  ///// external playback plugin
  if (mainw->ext_playback) {
    // playback plugin (therefore fullscreen / separate window)
    if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
      if (mainw->vpp->capabilities & VPP_CAN_RESIZE) {
        // plugin can resize, max is the screen size
        get_play_screen_size(opwidth, opheight);
      } else {
        // ext plugin can't resize, use its fixed size
        *opwidth = mainw->vpp->fwidth;
        *opheight = mainw->vpp->fheight;
      }
    } else {
      // remote display
      if (!(mainw->vpp->capabilities & VPP_CAN_RESIZE)) {
        // cant resize, we use the width it gave us if it can't resize
        *opwidth = mainw->vpp->fwidth;
        *opheight = mainw->vpp->fheight;
      } else {
        // else the clip size
        *opwidth = cfile->hsize;
        *opheight = cfile->vsize;
      }
    }
    goto align;
  }

  if (lives_get_status() != LIVES_STATUS_RENDERING && mainw->play_window) {
    // playback in separate window
    // use values set in resize_play_window

    *opwidth = rwidth = lives_widget_get_allocation_width(mainw->preview_image);// - H_RESIZE_ADJUST;
    *opheight = rheight = lives_widget_get_allocation_height(mainw->preview_image);// - V_RESIZE_ADJUST;

    /* *opwidth = mainw->pwidth; */
    /* *opheight = mainw->pheight; */

    if (mainw->multitrack && prefs->letterbox_mt) {
      rwidth = *opwidth;
      rheight = *opheight;
      *opwidth = cfile->hsize;
      *opheight = cfile->vsize;
      calc_maxspect(rwidth, rheight, opwidth, opheight);
    }
    goto align;
  }

  /////////////////////////////////////////////////////////////////////////////////
  // multitrack: we ignore double size, and fullscreen unless playing in the separate window
  if (mainw->multitrack) {
    *opwidth = mainw->multitrack->play_width;
    *opheight = mainw->multitrack->play_height;
    goto align;
  }

  ////////////////////////////////////////////////////////////////////////////////////
  // clip edit mode
  if (mainw->status == LIVES_STATUS_RENDERING) {
    *opwidth = cfile->hsize;
    *opheight = cfile->vsize;
    *opwidth = (*opwidth >> 2) << 2;
    *opheight = (*opheight >> 1) << 1;
    mainw->pwidth = *opwidth;
    mainw->pheight = *opheight;
    return;
  }

#define SCRN_BRDR 2.

  if (!mainw->fs) {
    // embedded player
    *opwidth = rwidth = lives_widget_get_allocation_width(mainw->play_image);// - H_RESIZE_ADJUST;
    *opheight = rheight = lives_widget_get_allocation_height(mainw->play_image);// - V_RESIZE_ADJUST;
  } else {
    // try to get exact inner size of the main window
    *opwidth = mainw->ce_frame_width;
    *opheight = mainw->ce_frame_height;
  }

 align:
  *opwidth = (*opwidth >> 3) << 3;
  *opheight = (*opheight >> 1) << 1;
  mainw->pwidth = *opwidth;
  mainw->pheight = *opheight;
}


void player_desensitize(void) {
  lives_widget_set_sensitive(mainw->rte_defs_menu, FALSE);
  lives_widget_set_sensitive(mainw->utilities_submenu, FALSE);
  lives_widget_set_sensitive(mainw->rfx_submenu, FALSE);
  lives_widget_set_sensitive(mainw->import_theme, FALSE);
  lives_widget_set_sensitive(mainw->export_theme, FALSE);
}


void player_sensitize(void) {
  lives_widget_set_sensitive(mainw->rte_defs_menu, TRUE);
  lives_widget_set_sensitive(mainw->utilities_submenu, TRUE);
  lives_widget_set_sensitive(mainw->rfx_submenu, TRUE);
  lives_widget_set_sensitive(mainw->import_theme, TRUE);
  lives_widget_set_sensitive(mainw->export_theme, TRUE);
}


void track_decoder_free(int i, int oclip) {
  if (mainw->old_active_track_list[i] == oclip) {
    if (mainw->track_decoders[i]) {
      boolean can_free = TRUE;
      if (!mainw->multitrack || mainw->active_track_list[i] > 0)  {
        if (oclip < 0 || IS_VALID_CLIP(oclip)) {
          if (IS_VALID_CLIP(oclip)) {
            lives_clip_t *xoclip = mainw->files[oclip];
            if (mainw->track_decoders[i] != (lives_decoder_t *)xoclip->ext_src) {
              // remove the clone for oclip
              if (xoclip->n_altsrcs > 0
                  && mainw->track_decoders[i] == (lives_decoder_t *)xoclip->alt_srcs[0]) {
                if (oclip == mainw->blend_file && mainw->blend_layer
                    && weed_plant_has_leaf(mainw->blend_layer, LIVES_LEAF_ALTSRC)) {
                  weed_leaf_delete(mainw->blend_layer, LIVES_LEAF_ALTSRC);
                }
                lives_free(xoclip->alt_srcs);
                xoclip->alt_srcs = NULL;
                lives_free(xoclip->alt_src_types);
                xoclip->alt_src_types = NULL;
                xoclip->n_altsrcs = 0;
              }
            } else can_free = FALSE;
          }
        }
      }
      if (can_free) clip_decoder_free(mainw->track_decoders[i]);
      else chill_decoder_plugin(oclip); /// free buffers to relesae memory
      mainw->track_decoders[i] = NULL;
    }
    if (i >= 0) mainw->old_active_track_list[i] = 0;
  }
}


LIVES_GLOBAL_INLINE void init_track_decoders(void) {
  int i;
  for (i = 0; i < MAX_TRACKS; i++) {
    mainw->track_decoders[i] = NULL;
    mainw->old_active_track_list[i] = mainw->active_track_list[i] = 0;
  }
}


LIVES_GLOBAL_INLINE void free_track_decoders(void) {
  for (int i = 0; i < MAX_TRACKS; i++) {
    if (mainw->track_decoders[i]) {
      mainw->old_active_track_list[i] = mainw->active_track_list[i];
      track_decoder_free(i, mainw->active_track_list[i]);
      mainw->active_track_list[i] = 0;
    }
  }
}


static boolean check_for_overlay_text(weed_layer_t *layer) {
  if (mainw->urgency_msg && prefs->show_urgency_msgs) {
    ticks_t timeout = lives_alarm_check(LIVES_URGENCY_ALARM);
    if (!timeout) {
      lives_freep((void **)&mainw->urgency_msg);
      return FALSE;
    }
    render_text_overlay(layer, mainw->urgency_msg, DEF_OVERLAY_SCALING);
    return TRUE;
  }

  if ((mainw->overlay_msg && prefs->show_overlay_msgs) || mainw->lockstats) {
    if (mainw->lockstats) {
      lives_freep((void **)&mainw->overlay_msg);
      show_sync_callback(NULL, NULL, 0, 0, LIVES_INT_TO_POINTER(1));
      if (mainw->overlay_msg) {
        render_text_overlay(layer, mainw->overlay_msg, DEF_OVERLAY_SCALING);
        if (prefs->render_overlay && mainw->record && !mainw->record_paused) {
          weed_plant_t *event = get_last_frame_event(mainw->event_list);
          if (event) weed_set_string_value(event, WEED_LEAF_OVERLAY_TEXT, mainw->overlay_msg);
        }
      }
      return TRUE;
    } else {
      if (!mainw->preview_rendering) {
        ticks_t timeout = lives_alarm_check(mainw->overlay_alarm);
        if (timeout == 0) {
          lives_freep((void **)&mainw->overlay_msg);
          return FALSE;
        }
      }
      render_text_overlay(layer, mainw->overlay_msg, DEF_OVERLAY_SCALING);
      if (mainw->preview_rendering) lives_freep((void **)&mainw->overlay_msg);
      return TRUE;
    }
  }
  return FALSE;
}


static boolean do_cleanup(weed_layer_t *layer, int success) {
  /// cleanup any resources after showing a frame. This run as a thread,
  /// so the main thread can return to the player
  /// if we return FALSE, this indicates that mainw->frame_layer has refs still
  lives_clip_t *sfile = NULL;
  int clip = -1;
  double fps = DEF_FPS;
  frames_t frame = 0;

  if (layer) {
    clip = lives_layer_get_clip(layer);
    frame = lives_layer_get_frame(layer);
    if (IS_VALID_CLIP(clip)) {
      sfile = mainw->files[clip];
      fps = sfile->pb_fps;
    }
  }

  if (success) {
    char *tmp;
    // format is now msg|timecode|fgclip|fgframe|fgfps|
    lives_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH, (const char *)
                 (tmp = lives_strdup_printf("%.8f|%d|%d|%.3f|",
                                            (double)mainw->currticks / TICKS_PER_SECOND_DBL,
                                            clip, frame, fps)));
    lives_free(tmp);
  }

  if (layer) {
    int refs;
    check_layer_ready(layer);
    // remove any ref we added, then free layer
    refs = weed_layer_unref(layer);
    if (layer == mainw->frame_layer) {
      if (refs >= 0) return FALSE;
      mainw->frame_layer = NULL;
    }
  }
  return TRUE;
}


#define USEC_WAIT_FOR_SYNC 500000 // 0.5 secxsy

static boolean avsync_check(void) {
  int count = USEC_WAIT_FOR_SYNC / 100, rc = 0;
  struct timespec ts;
  lives_clip_t *sfile = NULL;
#ifdef VALGRIND_ON
  count *= 10;
#else
  if (mainw->debug) count *= 10;
#endif

  if (mainw->foreign || !LIVES_IS_PLAYING || AUD_SRC_EXTERNAL || prefs->force_system_clock
      || (mainw->event_list && !(mainw->record || mainw->record_paused)) || prefs->audio_player == AUD_PLAYER_NONE
      || !is_realtime_aplayer(prefs->audio_player) || (CURRENT_CLIP_IS_VALID && cfile->play_paused)) {
    mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    return TRUE;
  }

  if (!mainw->audio_seek_ready) {
    int aclip = get_aplay_clipno();
    if (CLIP_HAS_AUDIO(aclip)) {
      sfile = mainw->files[aclip];
    }
    else mainw->audio_seek_ready = TRUE;
  }

  if (!mainw->video_seek_ready) {
    await_audio_queue(LIVES_DEFAULT_TIMEOUT);
#ifdef ENABLE_JACK
    if (!mainw->foreign && mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
      /// try to improve sync by delaying audio pb start
      if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
                         && !mainw->record_paused && mainw->jackd->is_paused)) {
        mainw->video_seek_ready = TRUE;
        if (!mainw->clip_switched) mainw->force_show = TRUE;
        return TRUE;
      }
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (!mainw->foreign && mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE) {
      /// try to improve sync by delaying audio pb start
      if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
                         && !mainw->record_paused && mainw->pulsed->is_paused)) {
        mainw->video_seek_ready = TRUE;
        if (!mainw->clip_switched) mainw->force_show = TRUE;
        return TRUE;
      }
    }
#endif
  }

  clock_gettime(CLOCK_REALTIME, &ts);

  mainw->video_seek_ready = TRUE;

  if (sfile) {
    if (sfile->last_play_sequence != mainw->play_sequence) count *= 10;
    pthread_mutex_lock(&mainw->avseek_mutex);
    while (!mainw->audio_seek_ready && --count) {
      ts.tv_nsec += 100000;   // def. 100 usec (* 5000 = 0.5 sec)
      if (ts.tv_nsec >= ONE_BILLION) {
	ts.tv_sec++;
	ts.tv_nsec -= ONE_BILLION;
      }
      rc = pthread_cond_timedwait(&mainw->avseek_cond, &mainw->avseek_mutex, &ts);
      if (rc != ETIMEDOUT) count++;
    }
  }
  pthread_mutex_unlock(&mainw->avseek_mutex);

  if (!mainw->audio_seek_ready && rc == ETIMEDOUT) {
    mainw->cancelled = handle_audio_timeout();
    return FALSE;
  }

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd &&
      (prefs->rec_opts & REC_AUDIO) && AUD_SRC_INTERNAL
      && mainw->rec_aclip != mainw->ascrap_file) {
    jack_get_rec_avals(mainw->jackd);
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed &&
      (prefs->rec_opts & REC_AUDIO) && AUD_SRC_INTERNAL
      && mainw->rec_aclip != mainw->ascrap_file) {
    pulse_get_rec_avals(mainw->pulsed);
  }
#endif

  if (sfile) {
    lives_time_source_t time_source = LIVES_TIME_SOURCE_NONE;
    mainw->fps_mini_measure = 0;
    mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
    mainw->fps_mini_ticks = mainw->last_startticks = mainw->startticks = mainw->currticks - sfile->async_delta;
    sfile->async_delta = 0;
  }

  pthread_mutex_lock(&mainw->avseek_mutex);
  pthread_cond_signal(&mainw->avseek_cond);
  pthread_mutex_unlock(&mainw->avseek_mutex);

  return TRUE;
}


boolean record_setup(ticks_t actual_ticks) {
  if (mainw->record_starting) {
    if (!mainw->event_list) {
      mainw->event_list = lives_event_list_new(NULL, NULL);
    }

    // mark record start
    mainw->event_list = append_marker_event(mainw->event_list, actual_ticks,
                                            EVENT_MARKER_RECORD_START);

    if (prefs->rec_opts & REC_EFFECTS) {
      // add init events and pchanges for all active fx
      add_filter_init_events(mainw->event_list, actual_ticks);
    }
    
    if (!await_audio_queue(LIVES_SHORT_TIMEOUT)) {
      mainw->cancelled = handle_audio_timeout();
      if (mainw->cancelled != CANCEL_NONE) return FALSE;
    }
    mainw->record = TRUE;
    mainw->record_paused = FALSE;
  }
  return TRUE;
}


boolean load_frame_image(frames_t frame) {
  // this is where we do the actual load/record of a playback frame
  // it is called every 1/fps from do_progress_dialog() via process_one() in dialogs.c

  // for the multitrack window we set mainw->frame_image; this is used to display the
  // preview image

  // NOTE: we should be careful if load_frame_image() is called from anywhere inside load_frame_image()
  // e.g. by calling g_main_context_iteration() --> user presses sepwin button --> load_frame_image() is called
  // this is because mainw->frame_layer is global and may get freed() /unreffed before exit from load_frame_image()
  // thus: - ref mainw->frame_layer (twice), store it in another variable, set mainw->frame_layer to NULL
  //  call this func, restore from backup var, unref twice

  // if mainw->frame_layer still hokds refs, we return FALSE, caller should unref and set to NULL

  void **pd_array = NULL, **retdata = NULL;
  LiVESPixbuf *pixbuf = NULL;

  char *framecount = NULL, *tmp;
  char *fname_next = NULL, *info_file = NULL;
  const char *img_ext = NULL;

  LiVESInterpType interp;

  boolean was_preview = FALSE;
  boolean rec_after_pb = FALSE;
  boolean success = FALSE;
  boolean size_ok = TRUE;
  boolean player_v2 = FALSE;
  boolean unreffed;

  int retval;
  int layer_palette, cpal;

  static int old_pwidth = 0, old_pheight = 0;
  int opwidth = 0, opheight = 0;
  int pwidth, pheight;
  int lb_width = 0, lb_height = 0;
  int bad_frame_count = 0;
  int fg_file = mainw->playing_file;
  int tgamma = WEED_GAMMA_UNKNOWN;
  boolean was_letterboxed = FALSE;

  mainw->scratch = SCRATCH_NONE;

#define BFC_LIMIT 1000
  if (LIVES_UNLIKELY(cfile->frames == 0 && !mainw->foreign && !mainw->is_rendering
                     && AUD_SRC_INTERNAL)) {
    // playing a clip with zero frames...
    if (mainw->record && !mainw->record_paused) {
      // add blank frame
      weed_plant_t *event = get_last_event(mainw->event_list);
      weed_plant_t *event_list = insert_blank_frame_event_at(mainw->event_list, lives_get_relative_ticks(mainw->origsecs,
													 mainw->orignsecs),
							     &event);
      if (!mainw->event_list) mainw->event_list = event_list;
      if (mainw->rec_aclip != -1 && (prefs->rec_opts & REC_AUDIO) && !mainw->record_starting) {
        // we are recording, and the audio clip changed; add audio
        if (mainw->rec_aclip == mainw->ascrap_file) {
          mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
	    (double)(mainw->files[mainw->ascrap_file]->arps * mainw->files[mainw->ascrap_file]->achans *
		     mainw->files[mainw->ascrap_file]->asampsize >> 3);
          mainw->rec_avel = 1.;
        }
        if (!mainw->mute) {
          insert_audio_event_at(event, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel);
          mainw->rec_aclip = -1;
        }
      }
    }
    if (!mainw->fs && !mainw->faded) get_play_times();

    return TRUE;
  }

  if (!mainw->foreign) {
    if (prefs->autotrans_amt >= 0.) set_trans_amt(prefs->autotrans_key - 1,
						  prefs->autotrans_mode >= 0 ? prefs->autotrans_mode
						  : rte_key_getmode(prefs->autotrans_key - 1),
						  &prefs->autotrans_amt);
    mainw->actual_frame = frame;
    if (!mainw->preview_rendering && (!((was_preview = mainw->preview) || mainw->is_rendering))) {
      /////////////////////////////////////////////////////////

      // normal play

      if (LIVES_UNLIKELY(mainw->nervous) && clip_can_reverse(mainw->playing_file)) {
        // nervous mode
        if ((mainw->actual_frame += (-10 + (int)(21.*rand() / (RAND_MAX + 1.0)))) > cfile->frames ||
            mainw->actual_frame < 1) mainw->actual_frame = frame;
	else {
	  if (mainw->actual_frame > cfile->frames) mainw->actual_frame = frame;
	  else if (AUD_SRC_INTERNAL && AV_CLIPS_EQUAL &&
		   !(prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS)
		   && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
	    resync_audio(mainw->playing_file, (double)mainw->actual_frame);
	  }
	}
        mainw->record_frame = mainw->actual_frame;
      }

      if (mainw->opening_loc || !CURRENT_CLIP_IS_NORMAL) {
        framecount = lives_strdup_printf("%9d", mainw->actual_frame);
      } else {
        framecount = lives_strdup_printf("%9d / %d", mainw->actual_frame, cfile->frames);
      }

      /////////////////////////////////////////////////

      // record performance
      if ((mainw->record && !mainw->record_paused)) {
        //int fg_frame = mainw->record_frame;
        int bg_file = (IS_VALID_CLIP(mainw->blend_file) && (prefs->tr_self ||
							    (mainw->blend_file != mainw->current_file)))
	  ? mainw->blend_file : -1;
        /* int bg_frame = (bg_file > 0 */
        /*                 && (prefs->tr_self || (bg_file != mainw->current_file))) */
        /*                ? mainw->files[bg_file]->frameno : 0; */

        // should we record the output from the playback plugin ?
        if (mainw->record && (prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
            (mainw->vpp->capabilities & VPP_CAN_RETURN)) {
          rec_after_pb = TRUE;
        }

        if (rec_after_pb || !CURRENT_CLIP_IS_NORMAL ||
            (prefs->rec_opts & REC_EFFECTS && bg_file != -1 && !IS_NORMAL_CLIP(bg_file))) {
          // TODO - handle non-opening of scrap_file
          if (mainw->scrap_file == -1) open_scrap_file();
          fg_file = mainw->scrap_file;
          //fg_frame = mainw->files[mainw->scrap_file]->frames + 1;
          mainw->scrap_file_size = mainw->files[mainw->scrap_file]->f_size;
          bg_file = -1;
          //bg_frame = 0;
        } else mainw->scrap_file_size = -1;

        if (framecount) lives_free(framecount);

        if (!mainw->elist_eom) {
          /* TRANSLATORS: rec(ord) */
          framecount = lives_strdup_printf((tmp = _("rec %9d / %d")), mainw->actual_frame,
                                           cfile->frames > mainw->actual_frame
                                           ? cfile->frames : mainw->actual_frame);
        } else {
          //pthread_mutex_unlock(&mainw->event_list_mutex);
          /* TRANSLATORS: out of memory (rec(ord)) */
          framecount = lives_strdup_printf((tmp = _("!rec %9d / %d")),
                                           mainw->actual_frame, cfile->frames);
        }
        lives_free(tmp);
      } else {
        if (mainw->toy_type != LIVES_TOY_NONE) {
          if (mainw->toy_type == LIVES_TOY_MAD_FRAMES && !mainw->fs && CURRENT_CLIP_IS_NORMAL) {
            int current_file = mainw->current_file;
            if (mainw->toy_go_wild) {
              int other_file;
              for (int i = 0; i < 11; i++) {
                other_file = (1 + (int)((double)(mainw->clips_available) * rand() / (RAND_MAX + 1.0)));
                other_file = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->cliplist, other_file));
                if (mainw->files[other_file]) {
                  // steal a frame from another clip
                  mainw->current_file = other_file;
                }
              }
            }
            load_end_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
            load_start_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
            mainw->current_file = current_file;
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    if (was_preview) {
      // preview
      if (mainw->proc_ptr && mainw->proc_ptr->frames_done > 0 &&
          frame >= (mainw->proc_ptr->frames_done - cfile->progress_start + cfile->start)) {
        if (cfile->opening) {
          mainw->proc_ptr->frames_done = cfile->opening_frames
	    = get_frame_count(mainw->current_file, cfile->opening_frames);
        }
      }
      if (mainw->proc_ptr && mainw->proc_ptr->frames_done > 0 &&
          frame >= (mainw->proc_ptr->frames_done - cfile->progress_start + cfile->start)) {
        mainw->cancelled = CANCEL_PREVIEW_FINISHED;
        goto lfi_done;
      }

      // play preview
      if (cfile->opening || (cfile->next_event && !mainw->proc_ptr)) {
        fname_next = make_image_file_name(cfile, frame + 1, get_image_ext_for_type(cfile->img_type));
        if (!mainw->fs && !prefs->hide_framebar && !mainw->is_rendering) {
          lives_freep((void **)&framecount);
          if (CURRENT_CLIP_HAS_VIDEO && cfile->frames != 123456789) {
            framecount = lives_strdup_printf("%9d / %d", frame, cfile->frames);
          } else {
            framecount = lives_strdup_printf("%9d", frame);
          }
        }
        if (mainw->toy_type != LIVES_TOY_NONE) {
          // TODO - move into toys.c
          if (mainw->toy_type == LIVES_TOY_MAD_FRAMES && !mainw->fs) {
            if (cfile->opening_only_audio) {
              load_end_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
              load_start_image(1 + (int)((double)cfile->frames * rand() / (RAND_MAX + 1.0)));
            } else {
              load_end_image(1 + (int)((double)frame * rand() / (RAND_MAX + 1.0)));
              load_start_image(1 + (int)((double)frame * rand() / (RAND_MAX + 1.0)));
            }
          }
        }
      } else {
        if (mainw->is_rendering || mainw->is_generating) {
          if (cfile->old_frames > 0) {
            img_ext = LIVES_FILE_EXT_MGK;
          } else {
            img_ext = get_image_ext_for_type(cfile->img_type);
          }
        } else {
          if (!mainw->keep_pre) {
            img_ext = LIVES_FILE_EXT_MGK;
          } else {
            img_ext = LIVES_FILE_EXT_PRE;
          }
        }
        fname_next = make_image_file_name(cfile, frame + 1, img_ext);
      }
      mainw->actual_frame = frame;

      // maybe the performance finished and we weren't looping
      if ((mainw->actual_frame < 1 || mainw->actual_frame > cfile->frames) &&
          CURRENT_CLIP_IS_NORMAL && (!mainw->is_rendering || mainw->preview)) {
        goto lfi_done;
      }
    }

    // limit max frame size unless we are saving to disk or rendering
    // frame_layer will in any case be equal to or smaller than this depending on maximum source frame size

    /* if (!(mainw->record && !mainw->record_paused && mainw->scrap_file != -1 */
    /*       && fg_file == mainw->scrap_file && !rec_after_pb)) { */
    if (!rec_after_pb) {
      get_player_size(&opwidth, &opheight);
    } else opwidth = opheight = 0;

    ////////////////////////////////////////////////////////////
    // load a frame from disk buffer

    if (mainw->preview && !mainw->frame_layer && (!mainw->event_list || cfile->opening)) {
      info_file = lives_build_filename(prefs->workdir, cfile->handle, LIVES_STATUS_FILE_NAME, NULL);
    }

    do {
      //wait_for_cleaner();
      boolean rndr = FALSE;
      if (mainw->frame_layer) {
        // free the old mainw->frame_layer
        check_layer_ready(mainw->frame_layer); // ensure all threads are complete
        weed_layer_free(mainw->frame_layer);
        mainw->frame_layer = NULL;
      }

      if ((mainw->is_rendering && !(mainw->proc_ptr && mainw->preview))) {
        rndr = TRUE;
      }

      //|| (!mainw->multitrack && mainw->num_tr_applied && IS_VALID_CLIP(mainw->blend_file))) {
      // here if we are rendering from multitrack, previewing a recording, or applying realtime effects to a selection
      //if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
      if (rndr && mainw->scrap_file != -1 && mainw->clip_index[0] == mainw->scrap_file && mainw->num_tracks == 1) {
        // pull from scrap_file -
        // do not apply fx, just pull frame
        mainw->frame_layer = lives_layer_new_for_frame(mainw->clip_index[0], mainw->frame_index[0]);
        weed_layer_ref(mainw->frame_layer);
        pull_frame_threaded(mainw->frame_layer, NULL, (weed_timecode_t)mainw->currticks, 0, 0);
      } else {
        // here we are rendering / playing and we can have any number of clips in a stack
        // we must ensure that if we play multiple copies of the same FILE_TYPE clip, each copy has its own
        // decoder - otherwise we would be continually jumping around inside the same decoder

        // we achieve this by creating a 'clone' of the original decoder for each subsequent copy of the original
        // -> clone decoder implies that we can skip things like getting frame count, frame size, fps, etc.
        // since we alraedy know this - thus we can very quickly create a new clone

        // the algorithm here decides which tracks get the original decoders (created with the clip), and which get clones
        // - maintain a running list (int array) of the previous tracks / clips (mainw->old_active_track_list)
        // - compare this with with the current stack (mainw->active_track_list)
        // - first, set all the values of mainw->ext_src_used to FALSE for each entry in active_track_list
        // - next walk the new list sequence:
        // -- if an entry in the current list == same entry in old list, mark the original as used
        //
        // - again walk the new list sequence:
        // -- if an entry in the current list != same entry in old list:
        //      - free any old clone for this track
        //      - if no new clip, we are done, otherwise
        //      - check if the decoder for new clip is in use
        //     -- if not, we use the original decoder, and mark it as "used";
        //     -- otherwise we create a new clone for this track
        //
        // mainw->track_decoders points to the decoder for each track, this can either be NULL, the original decoder, or the clone
        //
        // in clip editor (VJ) mode, this is only necessary if self transitions are enabled - then we must do some additional management
        // if blend_file == playing_file, then the blend_file ONLY must get a clone
        // e.g if playing_file is switched to equal blend_file - without this, blend_file would maintain the original,
        //         playing_file would get the clone - in this case we swap the track decoders, so blend_file gets the clone, and
        //          playing_file gets the original
        // - if either playing_file or blend_file are changed, if they then have differing values, blend_file
        //     can reqlinquish the clone and get the original, we must also take care that if all transitions are switched off,
        //   blend_file releases its clone before evaporating

        weed_timecode_t tc = rndr ? mainw->cevent_tc : 0;
        weed_layer_t **layers = NULL;
        int oclip, nclip, i;

      recheck:

        if (!mainw->multitrack) {
          mainw->num_tracks = 1;
          mainw->active_track_list[0] = mainw->playing_file;
          mainw->clip_index[0] = mainw->playing_file;
          mainw->frame_index[0] = mainw->actual_frame;

          if (mainw->num_tr_applied && IS_VALID_CLIP(mainw->blend_file)) {
            mainw->num_tracks = 2;
            mainw->active_track_list[1] = mainw->blend_file;
            mainw->clip_index[1] = mainw->blend_file;
            mainw->frame_index[1] = mainw->files[mainw->blend_file]->frameno;;
          }
        }
        if (rndr) {
          layers =
            (weed_layer_t **)lives_calloc((mainw->num_tracks + 1), sizeof(weed_layer_t *));
          // get list of active tracks from mainw->filter map
          if (mainw->multitrack)
            get_active_track_list(mainw->clip_index, mainw->num_tracks, mainw->filter_map);
        }

        lives_memset(mainw->ext_src_used, 0, MAX_FILES * sizint);

        for (i = 0; i < mainw->num_tracks; i++) {
          oclip = mainw->old_active_track_list[i];
          if (oclip > 0 && oclip == (nclip = mainw->active_track_list[i])) {
            // check if ext_src survives old->new
            if (mainw->track_decoders[i] == mainw->files[oclip]->ext_src) {
              mainw->ext_src_used[oclip] = TRUE;
            }
          }
        }

        for (i = 0; i < mainw->num_tracks; i++) {
          if (rndr && layers) {
            layers[i] = lives_layer_new_for_frame(mainw->clip_index[i], mainw->frame_index[i]);
            weed_layer_ref(layers[i]);
            weed_set_int_value(layers[i], WEED_LEAF_CURRENT_PALETTE, (mainw->clip_index[i] == -1 ||
								      mainw->files[mainw->clip_index[i]]->img_type ==
								      IMG_TYPE_JPEG) ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32);
          }

          if ((oclip = mainw->old_active_track_list[i]) != (nclip = mainw->active_track_list[i])) {
            // now using threading, we want to start pulling all pixel_data for all active layers here
            // however, we may have more than one copy of the same clip -
            // in this case we want to create clones of the decoder plugin
            // this is to prevent constant seeking between different frames in the clip
            if (mainw->track_decoders[i]) track_decoder_free(i, oclip);

            if (nclip > 0) {
              if (mainw->files[nclip]->clip_type == CLIP_TYPE_FILE) {
                if (!mainw->ext_src_used[nclip]) {
                  mainw->track_decoders[i] = (lives_decoder_t *)mainw->files[nclip]->ext_src;
                }
                if (mainw->track_decoders[i] == (lives_decoder_t *)mainw->files[nclip]->ext_src) {
                  mainw->ext_src_used[nclip] = TRUE;
                } else {
                  // add new clone for nclip
                  reset_timer_info();
                  mainw->track_decoders[i] = clone_decoder(nclip);
                  show_timer_info();
                  //g_print("CLONING\n");
                  if (!mainw->multitrack) {
                    if (IS_VALID_CLIP(mainw->blend_file)) {
                      lives_clip_t *bfile = mainw->files[mainw->blend_file];
                      if (i == 0) {
                        mainw->track_decoders[1] = mainw->track_decoders[0];
                        mainw->track_decoders[0] = (lives_decoder_t *)mainw->files[nclip]->ext_src;
                      }
                      bfile->n_altsrcs = 1;
                      bfile->alt_srcs = lives_calloc(1, sizeof(void *));
                      bfile->alt_srcs[0] = mainw->track_decoders[1];
                      bfile->alt_src_types = lives_calloc(1, sizint);
                      bfile->alt_src_types[0] = LIVES_EXT_SRC_DECODER;
                    }
                  }
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*

          mainw->old_active_track_list[i] = mainw->active_track_list[i];

          if (rndr) {
            if (nclip > 0) {
              // set alt src in layer
              weed_set_voidptr_value(layers[i], WEED_LEAF_HOST_DECODER,
                                     (void *)mainw->track_decoders[i]);
            } else {
              weed_layer_pixel_data_free(layers[i]);
            }
          }
        }

        if (rndr) {
          layers[i] = NULL;
          mainw->frame_layer = weed_apply_effects(layers, mainw->filter_map,
                                                  tc, opwidth, opheight, mainw->pchains);

          for (i = 0; layers[i]; i++) {
            weed_layer_unref(layers[i]);
            if (layers[i] != mainw->frame_layer) {
              check_layer_ready(layers[i]);
              weed_layer_free(layers[i]);
            }
          }

          lives_free(layers);

          if (mainw->internal_messaging) {
            // this happens if we are calling from multitrack, or apply rte.  We get our mainw->frame_layer and exit.
            // we add an extra refcount, which should case the fn to return FALSE
            lives_freep((void **)&framecount);
            lives_freep((void **)&info_file);
            weed_layer_ref(mainw->frame_layer);
            weed_layer_ref(mainw->frame_layer);
            goto lfi_done;
          }
        }

        else {
          if (prefs->dev_show_timing)
            g_printerr("pull_frame @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
          // normal playback in the clip editor, or applying a non-realtime effect
          if (!mainw->preview || lives_file_test(fname_next, LIVES_FILE_TEST_EXISTS)) {
            // TODO: some types of preview with CLIP_TYPE_FILE may need to avoid this
            // this section is intended only for rendered fx previews
            mainw->frame_layer = lives_layer_new_for_frame(mainw->current_file, mainw->actual_frame);
            weed_layer_ref(mainw->frame_layer);
            if (!img_ext) img_ext = get_image_ext_for_type(cfile->img_type);
            if (mainw->preview && !mainw->frame_layer
                && (!mainw->event_list || cfile->opening)) {
              if (!pull_frame_at_size(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks,
                                      cfile->hsize, cfile->vsize, WEED_PALETTE_END)) {
                if (mainw->frame_layer) {
                  weed_layer_unref(mainw->frame_layer);
                  weed_layer_free(mainw->frame_layer);
                  mainw->frame_layer = NULL;
                }

                if (cfile->clip_type == CLIP_TYPE_DISK &&
                    cfile->opening && cfile->img_type == IMG_TYPE_PNG
                    && sget_file_size(fname_next) <= 0) {
                  if (++bad_frame_count > BFC_LIMIT) {
                    mainw->cancelled = check_for_bad_ffmpeg();
                    bad_frame_count = 0;
                  } else lives_usleep(prefs->sleep_time);
                }
              }
            } else {
              // check first if got handed an external layer to play
              weed_layer_ref(mainw->ext_layer);
              if (mainw->ext_layer) {
                mainw->frame_layer = weed_layer_copy(NULL, mainw->ext_layer);
                weed_layer_unref(mainw->ext_layer);
              } else {
                // then check if we a have preloaded (cached) frame
                boolean got_preload = FALSE;
                if (mainw->frame_layer_preload && mainw->pred_clip == mainw->playing_file
                    && mainw->pred_frame != 0 && is_layer_ready(mainw->frame_layer_preload)) {
                  frames_t delta = (labs(mainw->pred_frame) - mainw->actual_frame) * sig(cfile->pb_fps);
                  /* g_print("THANKS for %p,! %d %ld should be %d, right  --  %d",
		  // mainw->frame_layer_preload, mainw->pred_clip, */
                  /*         mainw->pred_frame, mainw->actual_frame, delta); */
                  if (1) {
                    check_layer_ready(mainw->frame_layer_preload);
                    if (weed_layer_get_pixel_data(mainw->frame_layer_preload)) {
                      // depending on frame value we either make a deep or shallow copy of the cache frame
                      //g_print("YAH !\n");
                      got_preload = TRUE;
                      if (mainw->pred_frame > 0) {
                        // +ve value...make a deep copy, e.g we got the frame too early
                        // and we may need to reshow it several times
                        if (mainw->frame_layer) {
                          check_layer_ready(mainw->frame_layer);
                          weed_layer_free(mainw->frame_layer);
                        }
			mainw->pred_frame = -mainw->pred_frame;
                        mainw->frame_layer = weed_layer_copy(NULL, mainw->frame_layer_preload);
                        weed_layer_ref(mainw->frame_layer);
			weed_leaf_dup(mainw->frame_layer, mainw->frame_layer_preload, WEED_LEAF_CLIP);
			weed_leaf_dup(mainw->frame_layer, mainw->frame_layer_preload, WEED_LEAF_FRAME);
                      } else {
                        // +ve value...make a shallow copy and steal the frame data
                        // - the case whan we know we only need to use it once
                        // saving a memcpy
                        weed_layer_copy(mainw->frame_layer, mainw->frame_layer_preload);
                        weed_layer_nullify_pixel_data(mainw->frame_layer_preload);
                      }
                    }
                  }
                  if (prefs->show_dev_opts) {
                    if (delta < 0) g_printerr("cached frame    TOO LATE, got %ld, wanted %d !!!\n",
					      labs(mainw->pred_frame), mainw->actual_frame);
                  }
                  //if (delta > 0) g_print("    waiting...\n");
		  mainw->actual_frame = labs(mainw->pred_frame);
                }
                if (!got_preload) {
                  // if we didn't have a preloaded frame, we kick off a thread here to load it
                  pull_frame_threaded(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
                  //pull_frame(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks);
                }
                if ((mainw->rte || (mainw->is_rendering && !mainw->event_list))
                    && (mainw->current_file != mainw->scrap_file) && !mainw->multitrack) {
                  /// will set mainw->blend_layer
                  if (!get_blend_layer((weed_timecode_t)mainw->currticks)) {
                    layers = NULL; // should be anyway
                    goto recheck;
                  }
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*

          if (prefs->dev_show_timing)
            g_printerr("pull_frame done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
          if ((!cfile->next_event && mainw->is_rendering && !mainw->clip_switched &&
               (!mainw->multitrack || (!mainw->multitrack->is_rendering && !mainw->is_generating))) ||
              ((!mainw->multitrack || (mainw->multitrack && mainw->multitrack->is_rendering)) &&
               mainw->preview && !mainw->frame_layer)) {
            // preview ended
            if (!cfile->opening) mainw->cancelled = CANCEL_NO_MORE_PREVIEW;
            if (mainw->cancelled) {
              lives_free(fname_next);
              lives_freep((void **)&info_file);
              goto lfi_done;
            }
            // in case we are opening via non-instant means. We keep trying until the next frame appears.
            mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
          }

          img_ext = NULL;

          if (mainw->internal_messaging) {
            // here we are rendering to an effect or timeline, need to keep mainw->frame_layer and return
            lives_freep((void **)&framecount);
            lives_freep((void **)&info_file);
            check_layer_ready(mainw->frame_layer);
            weed_layer_ref(mainw->frame_layer);
            weed_layer_ref(mainw->frame_layer);
            goto lfi_done;
          }

          if (!mainw->frame_layer && (!mainw->preview || (mainw->multitrack && !cfile->opening))) {
            lives_freep((void **)&info_file);
            goto lfi_done;
          }

          if (mainw->preview && !mainw->frame_layer && (!mainw->event_list || cfile->opening)) {
            FILE *fp;
            // non-realtime effect preview
            // check effect to see if it finished yet
            if ((fp = fopen(info_file, "r"))) {
              clear_mainw_msg();
              do {
                retval = 0;
                lives_fgets(mainw->msg, MAINW_MSG_SIZE, fp);
                if (THREADVAR(read_failed) && THREADVAR(read_failed) == fileno(fp) + 1) {
                  THREADVAR(read_failed) = 0;
                  retval = do_read_failed_error_s_with_retry(info_file, NULL);
                }
              } while (retval == LIVES_RESPONSE_RETRY);
              fclose(fp);
              if (!lives_strncmp(mainw->msg, "completed", 9) || !strncmp(mainw->msg, "error", 5)) {
                // effect completed whilst we were busy playing a preview
                if (mainw->preview_box) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play"));
                lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play"));
                if (cfile->opening && !cfile->is_loaded) {
                  if (mainw->toy_type == LIVES_TOY_TV) {
                    on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
                  }
                }
                mainw->preview = FALSE;
              } else lives_nanosleep(LIVES_FORTY_WINKS);
            } else lives_nanosleep(LIVES_FORTY_WINKS);

            // or we reached the end of the preview
            if ((!cfile->opening && frame >= (mainw->proc_ptr->frames_done
                                              - cfile->progress_start + cfile->start)) ||
                (cfile->opening && (mainw->toy_type == LIVES_TOY_TV ||
                                    !mainw->preview || mainw->effects_paused))) {
              if (mainw->toy_type == LIVES_TOY_TV) {
                // force a loop (set mainw->cancelled to CANCEL_KEEP_LOOPING to play selection again)
                mainw->cancelled = CANCEL_KEEP_LOOPING;
              } else mainw->cancelled = CANCEL_NO_MORE_PREVIEW;
              lives_free(fname_next);
              // end of playback, so this is no longer needed
              lives_freep((void **)&info_file);
              goto lfi_done;
            } else if (mainw->preview || cfile->opening) lives_widget_context_update();
          }
        }
      }
    }
    // when playing backend previews, keep going until we run out of frames or the user cancels

    while (!mainw->frame_layer && mainw->cancelled == CANCEL_NONE
           && cfile->clip_type == CLIP_TYPE_DISK);

    lives_freep((void **)&info_file);

    if (LIVES_UNLIKELY((!mainw->frame_layer) || mainw->cancelled > 0)) {
      // NULL frame or user cancelled
      if (mainw->frame_layer) {
        check_layer_ready(mainw->frame_layer);
        weed_layer_free(mainw->frame_layer);
        mainw->frame_layer = NULL;
      }
      goto lfi_done;
    }

    if (was_preview) lives_free(fname_next);

    // OK. Here is the deal now. We have a layer from the current file, current frame.
    // (or at least we sent out a thread to fetch it).
    // We will pass this into the effects, and we will get back a layer.
    // The palette of the effected layer could be any Weed palette.
    // We will pass the layer to all playback plugins.
    // Finally we may want to end up with a GkdPixbuf (unless the playback plugin is VPP_DISPLAY_LOCAL
    // and we are in full screen mode).

    if (!prefs->vj_mode && (mainw->current_file != mainw->scrap_file || mainw->multitrack)
        && mainw->pwidth > 0 && mainw->pheight > 0
        && !(mainw->is_rendering && !(mainw->proc_ptr && mainw->preview))
        && !cfile->opening && !mainw->resizing && CURRENT_CLIP_IS_NORMAL
        && !is_virtual_frame(mainw->current_file, mainw->actual_frame)
        && is_layer_ready(mainw->frame_layer)) {
      // if we are pulling the frame from an image and playing back normally, check the size is what it should be
      // this used to cause problems with some effects, but that may no longer be the case with the layers model
      int wl = weed_layer_get_width_pixels(mainw->frame_layer);
      int hl = weed_layer_get_height(mainw->frame_layer);
      if ((wl != cfile->hsize && wl != mainw->pwidth)
          || (hl != cfile->vsize && hl != mainw->pheight)) {
        break_me("bad frame size");
        mainw->size_warn = mainw->current_file;
        size_ok = FALSE;
      }
      //cfile->hsize = wl;
      //cfile->vsize = hl;
    }

    if (size_ok) {
      // if frame size is OK we apply real time effects
      if ((mainw->rte || (mainw->is_rendering && !mainw->event_list))
          && (mainw->current_file != mainw->scrap_file || mainw->multitrack)
          && !mainw->preview) {
        if (prefs->dev_show_timing)
          g_printerr("rte start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        mainw->frame_layer = on_rte_apply(mainw->frame_layer, opwidth, opheight, (weed_timecode_t)mainw->currticks);
      }
    }

    if (prefs->dev_show_timing)
      g_printerr("rte done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    ////////////////////////

    // save to scrap_file now if we have to
    if (mainw->record && !mainw->record_paused && mainw->scrap_file != -1 && fg_file == mainw->scrap_file) {
      if (!rec_after_pb) {
#ifndef NEW_SCRAPFILE
        check_layer_ready(mainw->frame_layer);
        save_to_scrap_file(mainw->frame_layer);
#endif
        lives_freep((void **)&framecount);
      }
      get_player_size(&opwidth, &opheight);
    }

    if (mainw->ext_playback && (mainw->vpp->capabilities & VPP_CAN_RESIZE)
        && ((((!prefs->letterbox && !mainw->multitrack) || (mainw->multitrack && !prefs->letterbox_mt)))
            || (mainw->vpp->capabilities & VPP_CAN_LETTERBOX))) {
      // here we are outputting video through a video playback plugin which can resize: thus we just send whatever we have
      // we need only to convert the palette to whatever was agreed with the plugin when we called set_palette()
      // in plugins.c
      //
      // some plugins can resize and letterbox, otherwise we need to add borders and then let it resize

      weed_layer_t *frame_layer = NULL, *return_layer = NULL;
      int lwidth, lheight;
      int ovpppalette = mainw->vpp->palette;

      /// check if function exists - it accepts rowstrides
      if (mainw->vpp->play_frame) player_v2 = TRUE;

      //g_print("clr1 start  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      check_layer_ready(mainw->frame_layer);
      //g_print("clr1 done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      // check again to make sure our palette is still valid
      layer_palette = weed_layer_get_palette(mainw->frame_layer);
      if (!weed_palette_is_valid(layer_palette)) goto lfi_done;

      // some plugins allow changing the palette on the fly
      if ((mainw->vpp->capabilities & VPP_CAN_CHANGE_PALETTE)
          && mainw->vpp->palette != layer_palette) vpp_try_match_palette(mainw->vpp, mainw->frame_layer);

      if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
          ((weed_palette_is_rgb(layer_palette) &&
            !(weed_palette_is_rgb(mainw->vpp->palette))) ||
           (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
        // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
        // so copy layer and convert, retaining original
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      } else frame_layer = mainw->frame_layer;

      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
        // render the timecode for multitrack playback
        if (!check_for_overlay_text(frame_layer)) {
          if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
            frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);
          }
        }
      }

      if (prefs->apply_gamma) {
        // gamma correction
        if (weed_palette_is_rgb(mainw->vpp->palette)) {
          if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
            // some playback plugins may prefer linear gamma
            tgamma = WEED_GAMMA_LINEAR;
          else {
            tgamma = WEED_GAMMA_SRGB;
          }
        }
      }

      // final palette conversion to whatever the playback plugin needs
      if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
      if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                      mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgamma)) {
        goto lfi_done;
      }
      if (prefs->dev_show_timing)
        g_print("cl palette done %d to %d @ %f\n", weed_layer_get_palette(frame_layer), mainw->vpp->palette,
                lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (!player_v2) {
        // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
        if (!compact_rowstrides(frame_layer)) {
          goto lfi_done;
        }
      }
      if (prefs->dev_show_timing)
        g_print("comp rs done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if (mainw->stream_ticks == -1) mainw->stream_ticks = mainw->currticks;

      if (rec_after_pb) {
        // record output from playback plugin

        int retwidth = mainw->pwidth / weed_palette_get_pixels_per_macropixel(mainw->vpp->palette);
        int retheight = mainw->pheight;

        return_layer = weed_layer_create(retwidth, retheight, NULL, ovpppalette);

        if (weed_palette_is_yuv(ovpppalette)) {
          weed_set_int_value(return_layer, WEED_LEAF_YUV_CLAMPING, mainw->vpp->YUV_clamping);
          weed_set_int_value(return_layer, WEED_LEAF_YUV_SUBSPACE, mainw->vpp->YUV_subspace);
          weed_set_int_value(return_layer, WEED_LEAF_YUV_SAMPLING, mainw->vpp->YUV_sampling);
        }

        // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        if (create_empty_pixel_data(return_layer, FALSE, TRUE))
          retdata = weed_layer_get_pixel_data_planar(return_layer, NULL);
        else return_layer = NULL;
      }

      // chain any data to the playback plugin
      if (!(mainw->preview || mainw->is_rendering)) {
        // chain any data pipelines
        if (mainw->pconx) {
          pconx_chain_data(FX_DATA_KEY_PLAYBACK_PLUGIN, 0, FALSE);
        }
        if (mainw->cconx) cconx_chain_data(FX_DATA_KEY_PLAYBACK_PLUGIN, 0);
      }

      if (prefs->apply_gamma) {
        // gamma correction
        gamma_convert_layer(tgamma, frame_layer);
      }
      if (prefs->dev_show_timing)
        g_print("gamma conv done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (return_layer) weed_leaf_dup(return_layer, frame_layer, WEED_LEAF_GAMMA_TYPE);
      lb_width = lwidth = weed_layer_get_width(frame_layer);
      lb_height = lheight = weed_layer_get_height(frame_layer);
      pwidth = mainw->pwidth;
      pheight = mainw->pheight;

      // x_range and y_range define the ratio how much image to fill
      // so:
      // lb_width / pwidth, lb_height / pheight --> original unscaled image, centered in output
      // 1, 1 ---> image will be stretched to fill all window
      // get_letterbox_sizes ---> lb_width / pwidth , lb_height / pheight -> letterboxed

      if ((!mainw->multitrack && prefs->letterbox) || (mainw->multitrack && prefs->letterbox_mt)) {
        // plugin can resize but may not be able to letterbox...
        if (mainw->vpp->capabilities & VPP_CAN_LETTERBOX) {
          // plugin claims it can letterbox, so request that
          // - all we need to do is set the ratios of the image in the frame to center it in the player
          // the player will resize the image to fill the reduced area
          get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, FALSE);
          weed_set_double_value(frame_layer, "x_range", (double)lb_width / (double)pwidth);
          weed_set_double_value(frame_layer, "y_range", (double)lb_height / (double)pheight);
        } else {
          // plugin can resize but not letterbox - we will just letterbox to the player a.r
          // then let it resize to the screen size
          // (if the image is larger than the screen however, we will shrink it to fit)
          interp = get_interp_value(prefs->pb_quality, TRUE);
          get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, TRUE);
          if (!letterbox_layer(frame_layer, pwidth, pheight, lb_width, lb_height, interp,
                               mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
        }
      } else {
        if (player_v2) {
          // no letterboxing -> stretch it to fill window
          weed_set_double_value(frame_layer, "x_range", 1.0);
          weed_set_double_value(frame_layer, "y_range", 1.0);
        }
      }

      lwidth = weed_layer_get_width(frame_layer);
      if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
        // TODO - do conversion before letterboxing
        gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
      }

      // RENDER VIA PLUGIN
      if (!player_v2) pd_array = weed_layer_get_pixel_data_planar(frame_layer, NULL);
      if ((player_v2 && !(*mainw->vpp->play_frame)(frame_layer, mainw->currticks - mainw->stream_ticks, return_layer))
          || (!player_v2 && !(*mainw->vpp->render_frame)(lwidth, weed_layer_get_height(frame_layer),
							 mainw->currticks - mainw->stream_ticks, pd_array, retdata, mainw->vpp->play_params))) {
        //vid_playback_plugin_exit();
        if (return_layer) {
          weed_layer_free(return_layer);
          lives_free(retdata);
          return_layer = NULL;
        }
      } else success = TRUE;
      if (!player_v2) lives_free(pd_array);
      if (prefs->dev_show_timing)
        g_printerr("rend fr done @ %f\n\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if (frame_layer != mainw->frame_layer) {
        check_layer_ready(frame_layer);
        weed_layer_free(frame_layer);
      }

      if (return_layer) {
        save_to_scrap_file(return_layer);
        weed_layer_free(return_layer);
        lives_free(retdata);
        return_layer = NULL;
      }

      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
    }

    get_player_size(&mainw->pwidth, &mainw->pheight);

    if (prefs->dev_show_timing)
      g_printerr("ext start  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    if (mainw->ext_playback && (!(mainw->vpp->capabilities & VPP_CAN_RESIZE))) {
      // here we are playing through an external video playback plugin which cannot resize
      // - we must resize to whatever width and height we set when we called init_screen() in the plugin
      // i.e. mainw->vpp->fwidth, mainw->vpp fheight
      // both dimensions are in pixels,
      weed_plant_t *frame_layer = NULL;
      weed_plant_t *return_layer = NULL;
      int ovpppalette = mainw->vpp->palette;
      boolean needs_lb = FALSE;

      /// check if function exists - it accepts rowstrides
      if (mainw->vpp->play_frame) player_v2 = TRUE;

      check_layer_ready(mainw->frame_layer);
      if (prefs->dev_show_timing)
        g_printerr("clr2  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      layer_palette = weed_layer_get_palette(mainw->frame_layer);
      if (!weed_palette_is_valid(layer_palette)) goto lfi_done;

      // some plugins allow changing the palette on the fly
      if ((mainw->vpp->capabilities & VPP_CAN_CHANGE_PALETTE)
          && mainw->vpp->palette != layer_palette) vpp_try_match_palette(mainw->vpp, mainw->frame_layer);

      interp = get_interp_value(prefs->pb_quality, TRUE);

      if (mainw->fs && (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)) {
        mainw->vpp->fwidth = mainw->pwidth;
        mainw->vpp->fheight = mainw->pheight;
      }

      if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) && !(mainw->vpp->capabilities & VPP_CAN_RESIZE) &&
          ((mainw->vpp->fwidth  < mainw->pwidth || mainw->vpp->fheight < mainw->pheight))) {
        // mainw->frame_layer will be downsized for the plugin but upsized for screen
        // so copy layer and convert, retaining original
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      } else frame_layer = mainw->frame_layer;

      if (frame_layer == mainw->frame_layer && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
          ((weed_palette_is_rgb(layer_palette) &&
            !(weed_palette_is_rgb(mainw->vpp->palette))) ||
           (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
        // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
        // so copy layer and convert, retaining original
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      }
      if (prefs->dev_show_timing)
        g_printerr("copied  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      pwidth = mainw->vpp->fwidth;
      pheight = mainw->vpp->fheight;

      if ((mainw->multitrack && prefs->letterbox_mt) || (!mainw->multitrack && prefs->letterbox)) {
        /// letterbox external
        lb_width = weed_layer_get_width_pixels(mainw->frame_layer);
        lb_height = weed_layer_get_height(mainw->frame_layer);
        get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, (mainw->vpp->capabilities & VPP_CAN_RESIZE) != 0);
        if (pwidth != lb_width || pheight != lb_height) {
          needs_lb = TRUE;
          if (!(mainw->vpp->capabilities & VPP_CAN_LETTERBOX)) {
            if (frame_layer == mainw->frame_layer) {
              if (layer_palette != mainw->vpp->palette && (pwidth > lb_width || pheight > lb_height)) {
                frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
                if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
                if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                                mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgamma)) {
                  goto lfi_done;
                }
              } else {
                frame_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
                weed_layer_copy(frame_layer, mainw->frame_layer);
              }
            }
            //g_print("LB to %d X %d and from %d X %d\n", pwidth, pheight, lb_width, lb_height);
            if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
            if (!letterbox_layer(frame_layer, pwidth, pheight, lb_width, lb_height, interp,
                                 mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
            was_letterboxed = TRUE;
          } else {
            weed_set_double_value(frame_layer, "x_range", (double)lb_width / (double)pwidth);
            weed_set_double_value(frame_layer, "y_range", (double)lb_height / (double)pheight);
	    // *INDENT-OFF*
	  }}}
      // *INDENT-ON*

      if (prefs->dev_show_timing)
        g_printerr("lbb  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      layer_palette = weed_layer_get_palette(frame_layer);

      if (((weed_layer_get_width_pixels(frame_layer) ^ pwidth) >> 2) ||
          ((weed_layer_get_height(frame_layer) ^ pheight) >> 1)) {
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        if (!needs_lb || was_letterboxed) {
          lb_width = pwidth;
          lb_height = pheight;
        }
        if (!resize_layer(frame_layer, lb_width, lb_height, interp,
                          mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
      }
      if (prefs->dev_show_timing)
        g_printerr("resize done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      // resize_layer can change palette

      if (frame_layer == mainw->frame_layer && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
          ((weed_palette_is_rgb(layer_palette) &&
            !(weed_palette_is_rgb(mainw->vpp->palette))) ||
           (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
        // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
        // so copy layer and convert, retaining original
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      }

      layer_palette = weed_layer_get_palette(frame_layer);

      pwidth = weed_layer_get_width(frame_layer) * weed_palette_get_pixels_per_macropixel(layer_palette);
      pheight = weed_layer_get_height(frame_layer);

      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
        if (!check_for_overlay_text(frame_layer)) {
          if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
            frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);
          }
        }
      }

      if (prefs->apply_gamma) {
        // gamma correction
        if (weed_palette_is_rgb(mainw->vpp->palette)) {
          if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
            tgamma = WEED_GAMMA_LINEAR;
          else {
            tgamma = WEED_GAMMA_SRGB;
          }
        }
      }
      //g_print("clp start %d %d   %d %d @\n", weed_layer_get_palette(frame_layer),
      //mainw->vpp->palette, weed_layer_get_gamma(frame_layer), tgamma);
      if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
      if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                      mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgamma)) {
        goto lfi_done;
      }

      if (prefs->dev_show_timing)
        g_printerr("clp done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (mainw->stream_ticks == -1) mainw->stream_ticks = mainw->currticks;

      if (!player_v2) {
        // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
        if (!compact_rowstrides(frame_layer)) goto lfi_done;
        if (prefs->dev_show_timing)
          g_printerr("c rows done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      }
      if (rec_after_pb) {
        // record output from playback plugin
        int retwidth = mainw->vpp->fwidth;
        int retheight = mainw->vpp->fheight;

        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        return_layer = weed_layer_create(retwidth, retheight, NULL, ovpppalette);

        if (weed_palette_is_yuv(mainw->vpp->palette)) {
          weed_layer_set_yuv_clamping(return_layer, mainw->vpp->YUV_clamping);
          weed_layer_set_yuv_sampling(return_layer, mainw->vpp->YUV_sampling);
          weed_layer_set_yuv_subspace(return_layer, mainw->vpp->YUV_subspace);
        }

        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1 ; /// special value to compact the rowstrides
        if (create_empty_pixel_data(return_layer, FALSE, TRUE)) {
          retdata = weed_layer_get_pixel_data_planar(return_layer, NULL);
        } else return_layer = NULL;
      }

      // chain any data to the playback plugin
      if (!(mainw->preview || mainw->is_rendering)) {
        // chain any data pipelines
        if (mainw->pconx) {
          pconx_chain_data(-2, 0, FALSE);
        }
        if (mainw->cconx) cconx_chain_data(-2, 0);
      }

      if (tgamma != WEED_GAMMA_UNKNOWN) {
        if (!was_letterboxed) {
          gamma_convert_layer(tgamma, frame_layer);
        } else {
          gamma_convert_sub_layer(tgamma, 1.0, frame_layer, (pwidth - lb_width) / 2, (pheight - lb_height) / 2,
                                  lb_width, lb_height, TRUE);
        }
      }

      if (return_layer) weed_layer_set_gamma(return_layer, weed_layer_get_gamma(frame_layer));

      if (player_v2) {
        weed_set_double_value(frame_layer, "x_range", 1.0);
        weed_set_double_value(frame_layer, "y_range", 1.0);
      }

      if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
        // TODO - do conversion before letterboxing
        gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
      }

      if (!player_v2) pd_array = weed_layer_get_pixel_data_planar(frame_layer, NULL);
      if ((player_v2 && !(*mainw->vpp->play_frame)(frame_layer,
						   mainw->currticks - mainw->stream_ticks, return_layer))
          || (!player_v2 && !(*mainw->vpp->render_frame)(weed_layer_get_width(frame_layer),
							 weed_layer_get_height(frame_layer),
							 mainw->currticks - mainw->stream_ticks, pd_array, retdata,
							 mainw->vpp->play_params))) {
        //vid_playback_plugin_exit();
        if (return_layer) {
          weed_layer_free(return_layer);
          lives_free(retdata);
          return_layer = NULL;
        }
        goto lfi_done;
      } else success = TRUE;
      if (!player_v2) lives_free(pd_array);
      if (prefs->dev_show_timing)
        g_printerr("rend done  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (return_layer) {
        int width = MIN(weed_layer_get_width_pixels(mainw->frame_layer),
                        weed_layer_get_width_pixels(return_layer));
        int height = MIN(weed_layer_get_height(mainw->frame_layer),
                         weed_layer_get_height(return_layer));
        if (resize_layer(return_layer, width, height, LIVES_INTERP_FAST, WEED_PALETTE_END, 0)) {
          if (tgamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
            // TODO - save w. screen_gamma
            gamma_convert_layer(WEED_GAMMA_SRGB, frame_layer);
          }
          save_to_scrap_file(return_layer);
          lives_freep((void **)&framecount);
        }
        lives_free(retdata);
        weed_layer_free(return_layer);
        return_layer = NULL;
      }

      if (frame_layer != mainw->frame_layer) {
        check_layer_ready(frame_layer);
        weed_layer_free(frame_layer);
      }

      // frame display was handled by a playback plugin, skip the rest
      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
    }

    ////////////////////////////////////////////////////////
    // local display - either we are playing with no playback plugin, or else the playback plugin has no
    // local display of its own
    if (prefs->dev_show_timing)
      g_printerr("clr @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    check_layer_ready(mainw->frame_layer); // wait for all threads to complete
    if (prefs->dev_show_timing)
      g_printerr("clr end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    if (weed_layer_get_width(mainw->frame_layer) == 0) {
      goto lfi_done;
    }
    if ((mainw->sep_win && !prefs->show_playwin) || (!mainw->sep_win && !prefs->show_gui)) {
      // no display to output, skip the rest
      success = TRUE;
      goto lfi_done;
    }

    if (mainw->ext_playback && !player_v2) THREADVAR(rowstride_alignment_hint) = -1 ; /// special value to compact the rowstrides
    layer_palette = weed_layer_get_palette(mainw->frame_layer);
    if (!weed_palette_is_valid(layer_palette) || !CURRENT_CLIP_IS_VALID) goto lfi_done;

    if (cfile->img_type == IMG_TYPE_JPEG || !weed_palette_has_alpha(layer_palette))
      cpal = WEED_PALETTE_RGB24;
    else {
      cpal = WEED_PALETTE_RGBA32;
    }
    if (mainw->fs && (!mainw->multitrack || mainw->sep_win)) {
      // set again, in case vpp was turned off because of preview conditions
      get_player_size(&mainw->pwidth, &mainw->pheight);
    }

    interp = get_interp_value(prefs->pb_quality, TRUE);

    pwidth = opwidth;
    pheight = opheight;

    pwidth = (int)(pwidth >> 3) << 3;

    if (prefs->dev_show_timing)
      g_printerr("res start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    lb_width = weed_layer_get_width_pixels(mainw->frame_layer);
    lb_height = weed_layer_get_height(mainw->frame_layer);

    if ((lb_width != pwidth || lb_height != pheight)
        || weed_get_boolean_value(mainw->frame_layer, "letterboxed", NULL) == WEED_FALSE) {
      if ((!mainw->multitrack && prefs->letterbox) || (mainw->multitrack && prefs->letterbox_mt)) {
        /// letterbox internal
        get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, FALSE);
        if (!letterbox_layer(mainw->frame_layer, pwidth, pheight, lb_width,
                             lb_height, interp, cpal, 0)) goto lfi_done;
        was_letterboxed = TRUE;
        weed_set_boolean_value(mainw->frame_layer, "letterboxed", WEED_TRUE);
        lb_width = pwidth;
        lb_height = pheight;
      }
    }

    if (lb_width != pwidth || lb_height != pheight ||
        weed_get_boolean_value(mainw->frame_layer, "letterboxed", NULL) == WEED_FALSE) {
      if (weed_layer_get_width_pixels(mainw->frame_layer) != pwidth ||
          weed_layer_get_height(mainw->frame_layer) != pheight) {
        if (!resize_layer(mainw->frame_layer, pwidth, pheight, interp, cpal, 0)) goto lfi_done;
      }
    }
    if (prefs->dev_show_timing)
      g_printerr("res end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    if (!convert_layer_palette_full(mainw->frame_layer, cpal, 0, 0, 0, WEED_GAMMA_SRGB)) goto lfi_done;

    if (prefs->dev_show_timing)
      g_printerr("clp end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    if (LIVES_IS_PLAYING) {
      if (!check_for_overlay_text(mainw->frame_layer)) {
        if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
          mainw->frame_layer = render_text_overlay(mainw->frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);
        }
      }
    }

    /* if (1 || !was_letterboxed) { */
    /*   if (prefs->use_screen_gamma) */
    /*     gamma_convert_layer(WEED_GAMMA_MONITOR, mainw->frame_layer); */
    /*   else */
    /*     gamma_convert_layer(WEED_GAMMA_SRGB, mainw->frame_layer); */
    /* } else { */
    /*   if (prefs->use_screen_gamma) */
    /*     gamma_convert_sub_layer(WEED_GAMMA_MONITOR, 1.0, mainw->frame_layer, (pwidth - lb_width) / 2, (pheight - lb_height / 2), */
    /*                             lb_width, lb_height, TRUE); */
    /*   else */
    /*     gamma_convert_sub_layer(WEED_GAMMA_SRGB, 1.0, mainw->frame_layer, (pwidth - lb_width) / 2, (pheight - lb_height / 2), */
    /*                             lb_width, lb_height, TRUE); */
    /* } */

    if (prefs->dev_show_timing)
      g_printerr("l2p start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    pixbuf = layer_to_pixbuf(mainw->frame_layer, TRUE, TRUE);

    if (prefs->dev_show_timing)
      g_printerr("l2p @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    // internal player, double size or fullscreen, or multitrack

    if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
      set_drawing_area_from_pixbuf(mainw->preview_image, pixbuf, mainw->pi_surface);
      lives_widget_queue_draw(mainw->preview_image);
    } else {
      pwidth = lives_widget_get_allocation_width(mainw->play_image);
      pheight = lives_widget_get_allocation_height(mainw->play_image);
      if (pwidth < old_pwidth || pheight < old_pheight)
        clear_widget_bg(mainw->play_image, mainw->play_surface);
      old_pwidth = pwidth;
      old_pheight = pheight;
      set_drawing_area_from_pixbuf(mainw->play_image, pixbuf, mainw->play_surface);
      lives_widget_queue_draw(mainw->play_image);
    }

    if (pixbuf) lives_widget_object_unref(pixbuf);
    success = TRUE;
    if (prefs->dev_show_timing)
      g_print("paint @ %f\n\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    goto lfi_done;
  }

  // record external window
  if (mainw->record_foreign) {
    char fname[PATH_MAX];
    int xwidth, xheight;
    LiVESError *gerror = NULL;
    lives_painter_t *cr = lives_painter_create_from_surface(mainw->play_surface);

    if (!cr) return TRUE;

    if (mainw->rec_vid_frames == -1) {
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), (tmp = lives_strdup_printf("%9d", frame)));
    } else {
      if (frame > mainw->rec_vid_frames) {
        mainw->cancelled = CANCEL_KEEP;
        if (CURRENT_CLIP_HAS_VIDEO) cfile->frames = mainw->rec_vid_frames;
        return TRUE;
      }

      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), (tmp = lives_strdup_printf("%9d / %9d",
											frame, mainw->rec_vid_frames)));
      lives_free(tmp);
    }

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
    xwidth = gdk_window_get_width(mainw->foreign_window);
    xheight = gdk_window_get_height(mainw->foreign_window);
    if ((pixbuf = gdk_pixbuf_get_from_window(mainw->foreign_window, 0, 0, xwidth, xheight))) {
#else
      gdk_window_get_size(mainw->foreign_window, &xwidth, &xheight);
      if ((pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(mainw->foreign_window),
						 mainw->foreign_cmap, 0, 0, 0, 0, xwidth, xheight))) {
#endif
#endif
#if 0
      }
#endif
      tmp = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
      lives_snprintf(fname, PATH_MAX, "%s", tmp);
      lives_free(tmp);

      do {
	// TODO ***: add a timeout here
	if (gerror) lives_error_free(gerror);
	lives_pixbuf_save(pixbuf, fname, cfile->img_type, 100, cfile->hsize, cfile->vsize, &gerror);
      } while (gerror);

      lives_painter_set_source_pixbuf(cr, pixbuf, 0, 0);
      lives_painter_paint(cr);
      lives_painter_destroy(cr);

      if (pixbuf) lives_widget_object_unref(pixbuf);
      cfile->frames = frame;
    } else {
      widget_opts.non_modal = TRUE;
      do_error_dialog(_("LiVES was unable to capture this image\n\n"));
      widget_opts.non_modal = FALSE;
      mainw->cancelled = CANCEL_CAPTURE_ERROR;
    }
    if (frame > mainw->rec_vid_frames && mainw->rec_vid_frames > -1)
      mainw->cancelled = CANCEL_KEEP;
    lives_freep((void **)&framecount);
    return TRUE;
  }
 lfi_done:
  // here we unref / free the mainw->frame_layer (the output video "frame" we just worked with)
  // if we return FALSE, then mainw->frame_layer still has a ref
  // we also animate the timeline and frame counters
  // if success is TRUE we may send an OSC FRAME_SYNCH notification

  /* if (cleaner) { */
  /* 	lives_nanosleep_until_nonzero(weed_get_boolean_value(cleaner, WEED_LEAF_DONE, NULL)); */
  /* 	weed_plant_free(cleaner); */
  /* 	cleaner = NULL; */
  /* } */

  if (!mainw->video_seek_ready) avsync_check();

  if (framecount) {
    if ((!mainw->fs || (prefs->play_monitor != widget_opts.monitor && capable->nmonitors > 1) ||
	 (mainw->ext_playback && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)))
	&& !prefs->hide_framebar) {
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
    }
  }

  unreffed = do_cleanup(mainw->frame_layer, success);
  if (unreffed < 0) mainw->frame_layer = NULL;

  THREADVAR(rowstride_alignment_hint) = 0;
  lives_freep((void **)&framecount);
  if (success) {
    if (!mainw->multitrack &&
	!mainw->faded && (!mainw->fs || (prefs->gui_monitor != prefs->play_monitor
					 && prefs->play_monitor != 0 && capable->nmonitors > 1))
	&& mainw->current_file != mainw->scrap_file) {
      double ptrtime = ((double)mainw->actual_frame - 1.) / cfile->fps;
      mainw->ptrtime = ptrtime;
      lives_widget_queue_draw(mainw->eventbox2);
    }
    if (LIVES_IS_PLAYING && mainw->multitrack && !cfile->opening) animate_multitrack(mainw->multitrack);
  }
  return unreffed;
}


static lives_time_source_t lastt = LIVES_TIME_SOURCE_NONE;
static ticks_t delta = 0;

void reset_playback_clock(void) {
  mainw->sc_timing_ratio = -1.;
  mainw->cadjticks = mainw->adjticks = mainw->syncticks = 0;
  mainw->currticks = mainw->startticks = mainw->deltaticks = 0;
  lastt = LIVES_TIME_SOURCE_NONE;
  delta = 0;
}


ticks_t lives_get_current_playback_ticks(int64_t origsecs, int64_t orignsecs, lives_time_source_t *time_source) {
  // get the time using a variety of methods
  // time_source may be NULL or LIVES_TIME_SOURCE_NONE to set auto
  // or another value to force it (EXTERNAL cannot be forced)
  lives_time_source_t *tsource, xtsource = LIVES_TIME_SOURCE_NONE;
  ticks_t clock_ticks, current = -1;
  static ticks_t lclock_ticks, interticks, last_sync_ticks;
  static ticks_t sc_start, sys_start;

  if (time_source) tsource = time_source;
  else tsource = &xtsource;

  if (lastt == LIVES_TIME_SOURCE_NONE) last_sync_ticks = mainw->syncticks;

  mainw->clock_ticks = clock_ticks = lives_get_relative_ticks(origsecs, orignsecs);

  if (*tsource == LIVES_TIME_SOURCE_EXTERNAL) *tsource = LIVES_TIME_SOURCE_NONE;

  if (mainw->foreign || prefs->force_system_clock || (prefs->vj_mode && AUD_SRC_EXTERNAL)) {
    *tsource = LIVES_TIME_SOURCE_SYSTEM;
    current = clock_ticks;
  }

#ifdef ENABLE_JACK_TRANSPORT
  if (*tsource == LIVES_TIME_SOURCE_NONE) {
    if (mainw->jack_can_stop && mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) {
      // calculate the time from jack transport
      *tsource = LIVES_TIME_SOURCE_EXTERNAL;
      current = jack_transport_get_current_ticks(mainw->jackd_trans);
    }
  }
#endif

  // generally tsource is set to NONE, - here we check first for soundcard time
  if (is_realtime_aplayer(prefs->audio_player) && (*tsource == LIVES_TIME_SOURCE_NONE ||
						   *tsource == LIVES_TIME_SOURCE_SOUNDCARD)) {
    if ((!mainw->is_rendering || (mainw->multitrack && !cfile->opening && !mainw->multitrack->is_rendering)) &&
        (!(mainw->fixed_fpsd > 0. || (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)))) {
      // get time from soundcard
      // this is done so as to synch video stream with the audio
      // we do this in two cases:
      // - for internal audio, playing back a clip with audio (writing)
      // - or when audio source is set to external (reading), no internal audio generator is running

      // we ignore this if we are running with a playback plugin which requires a fixed framerate (e.g a streaming plugin)
      // in that case we will adjust the audio rate to fit the system clock
      // or if we are rendering

      // if the timecard cannot return current time we get a value of -1 back, and then fall back to system clock

#ifdef ENABLE_JACK
      if (prefs->audio_player == AUD_PLAYER_JACK &&
          ((prefs->audio_src == AUDIO_SRC_INT && mainw->jackd && mainw->jackd->in_use &&
            IS_VALID_CLIP(mainw->jackd->playing_file) && mainw->files[mainw->jackd->playing_file]->achans > 0) ||
           (prefs->audio_src == AUDIO_SRC_EXT && mainw->jackd_read && mainw->jackd_read->in_use))) {
        *tsource = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_jack_get_time(mainw->jackd_read);
        else
          current = lives_jack_get_time(mainw->jackd);
      }
#endif

#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_player == AUD_PLAYER_PULSE &&
          ((prefs->audio_src == AUDIO_SRC_INT && mainw->pulsed && mainw->pulsed->in_use &&
            ((mainw->multitrack && cfile->achans > 0)
             || (!mainw->multitrack && IS_VALID_CLIP(mainw->pulsed->playing_file)
                 && mainw->files[mainw->pulsed->playing_file]->achans > 0)))
           || (prefs->audio_src == AUDIO_SRC_EXT && mainw->pulsed_read && mainw->pulsed_read->in_use))) {
        *tsource = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_pulse_get_time(mainw->pulsed_read);
        else
	  current = lives_pulse_get_time(mainw->pulsed);
      }
#endif
    }

    if (current >= 0 && LIVES_IS_PLAYING && time_source && lastt == LIVES_TIME_SOURCE_SOUNDCARD
        && *tsource == LIVES_TIME_SOURCE_SOUNDCARD) {
      if (current - mainw->adjticks + mainw->syncticks < mainw->startticks) {
        mainw->syncticks = mainw->startticks - current + mainw->adjticks;
      }
      if (current - mainw->adjticks + mainw->syncticks < mainw->currticks) {
        mainw->syncticks = mainw->currticks - current + mainw->adjticks;
      }
    }
  }


  if (*tsource == LIVES_TIME_SOURCE_NONE || current == -1) {
    *tsource = LIVES_TIME_SOURCE_SYSTEM;
    current = clock_ticks;
  }

  //if (lastt != *tsource) {
  /* g_print("t1 = %ld, t2 = %ld cadj =%ld, adj = %ld del =%ld %ld %ld\n", clock_ticks, current, mainw->cadjticks, mainw->adjticks, */
  /*         delta, clock_ticks + mainw->cadjticks, current + mainw->adjticks); */
  //}

  /// synchronised timing
  /// it can be helpful to imagine a virtual clock which is at some "current time" (interticks):
  /// clock time - cadjticks = virtual time = other time - adjticks
  /// cadjticks and adjticks are only set when we switch from one source to another, after this the clock time runs at a rate
  /// depending on the source. This is fine as it enables sync with the clock source, provided the time doesn't jump when moving
  /// from one source to another.
  /// when the source changes we therefor alter either cadjticks or adjticks so that the initial timing matches

  /// occasionally when switching sources e.g from system -> soundcard, the new source may take a small time
  /// to start running - in this case we can allow for this by increasing mainw->syncticks via system clock
  /// until the audio source begins updating correctly. Alternately the source may be reste causing a jump in the timing.
  /// here we can also compensate by making an adjust to mainw->syncticks so the rest becomes transparent.

  // tl;dr - mainw->synticks is an external global adjustment factor

  if (time_source) {
    if (*tsource == LIVES_TIME_SOURCE_SYSTEM)  {
      if (lastt != LIVES_TIME_SOURCE_SYSTEM && lastt != LIVES_TIME_SOURCE_NONE) {
	// current + adjt == clock_ticks - cadj /// interticks == lcurrent + adj
	// current - ds + adjt == clock_ticks - dc - cadj /// interticks == lcurrent + adj

	// cadj = clock_ticks - interticks + (current - lcurrent) - since we may not have current
	// we have to approximate with clock_ticks - lclock_ticks

	// this is the difference between clock ticks and last returned value
	// since interticks was from the last cycle, we subtract the delt to compare times at that cycle
	// - i.e, adding this value to interticks would give clock ticks at last cycle
	// thus if we subtract this value from current clock ticks we get interticks for current cycle
	mainw->cadjticks = (clock_ticks + mainw->syncticks) - (clock_ticks - lclock_ticks) - (interticks + last_sync_ticks);
      }
      interticks = clock_ticks - mainw->cadjticks;
      //if (interticks + mainw->syncticks < mainw->startticks) break_me("oops 2");
      mainw->sc_timing_ratio = -1.;
    } else {
      if (lastt == LIVES_TIME_SOURCE_SYSTEM || lastt == LIVES_TIME_SOURCE_NONE) {
	// current - ds + adjt == clock_ticks - dc - cadj /// iinterticks == lclock_ticks - cadj ///

	// here we calculate the difference between interticks and current. interticks was from the last cycle
	// thus we subtract the clock time differenc between this cycle and the last to give an adjusted current
	// (we have to use clock time for the adjustment as we don't have the s.card value from previous)
	// subtracting from current would give interticks for this cycle
	if (lastt == LIVES_TIME_SOURCE_SYSTEM)
	  mainw->adjticks = current + mainw->syncticks - (clock_ticks - lclock_ticks) - (interticks + last_sync_ticks);
	if (*tsource == LIVES_TIME_SOURCE_SOUNDCARD) {
	  sc_start = current + mainw->syncticks;
	  sys_start = clock_ticks;
	}
      }
      if (*tsource == LIVES_TIME_SOURCE_SOUNDCARD && clock_ticks > sys_start) {
	// here we can calculate the ratio sc_rate : sys_rate (< 1. means sc is playing slower)
	mainw->sc_timing_ratio = (double)(current + mainw->syncticks - sc_start) / (double)(clock_ticks - sys_start);
	//g_print("timing ratio is %.2f\n", mainw->sc_timing_ratio * 100.);
      } else mainw->sc_timing_ratio = -1.;
      // on reset, mainw->adjticks is 0, and lastt is NONE, so interticks is set to current ticks
      interticks = current - mainw->adjticks;
    }

    /* if (lastt != *tsource) { */
    /*   g_print("aft t1 = %ld, t2 = %ld cadj =%ld, adj = %ld del =%ld %ld %ld\n", clock_ticks, current, mainw->cadjticks, */
    /*           mainw->adjticks, delta, clock_ticks + mainw->cadjticks, current + mainw->adjticks); */
    /* } */
    lclock_ticks = clock_ticks;
    lastt = *tsource;
    last_sync_ticks = mainw->syncticks;
  }
  else {
    if (*tsource == LIVES_TIME_SOURCE_SYSTEM) interticks = clock_ticks - mainw->cadjticks;
    else  interticks = current - mainw->adjticks;
  }

  // onreset mainw->syncticks is set to 0, so we just return interticks
  return interticks + mainw->syncticks;
}


static boolean check_for_audio_stop(int fileno, frames_t first_frame, frames_t last_frame) {
  // this is only used for older versions with non-realtime players
  // return FALSE if audio stops playback
  lives_clip_t *sfile = mainw->files[fileno];
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd && mainw->jackd->playing_file == fileno) {
    if (!mainw->loop || mainw->playing_sel) {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno - 0.0001 < (double)first_frame + 0.0001)
            || (sfile->adirection == LIVES_DIRECTION_FORWARD && mainw->aframeno + 0.0001 >= (double)last_frame - 0.0001)) {
          return FALSE;
        }
      }
    } else {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno < 0.9999) ||
            (sfile->adirection == LIVES_DIRECTION_FORWARD && calc_time_from_frame(mainw->current_file, mainw->aframeno + 1.0001)
             >= cfile->laudio_time - 0.0001)) {
          return FALSE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed && mainw->pulsed->playing_file == fileno) {
    if (!mainw->loop || mainw->playing_sel) {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno - 0.0001 < (double)first_frame + 0.0001)
            || (sfile->adirection == LIVES_DIRECTION_FORWARD && mainw->aframeno + 1.0001 >= (double)last_frame - 0.0001)) {
          return FALSE;
        }
      }
    } else {
      if (!mainw->loop_cont) {
        if ((sfile->adirection == LIVES_DIRECTION_REVERSE && mainw->aframeno < 0.9999) ||
            (sfile->adirection == LIVES_DIRECTION_FORWARD && calc_time_from_frame(mainw->current_file, mainw->aframeno + 1.0001)
             >= cfile->laudio_time - 0.0001)) {
          return FALSE;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

#endif
  return TRUE;
}


LIVES_GLOBAL_INLINE void calc_aframeno(int fileno) {
  if (CLIP_HAS_AUDIO(fileno)) {
    lives_clip_t *sfile = mainw->files[fileno];
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && ((mainw->jackd && mainw->jackd->playing_file == fileno) ||
						   (mainw->jackd_read && mainw->jackd_read->playing_file == fileno))) {
      // get seek_pos from jack
      if (mainw->jackd_read && mainw->jackd_read->playing_file == fileno)
        mainw->aframeno = lives_jack_get_pos(mainw->jackd_read) * sfile->fps + 1.;
      else
        mainw->aframeno = lives_jack_get_pos(mainw->jackd) * sfile->fps + 1.;
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && ((mainw->pulsed && mainw->pulsed->playing_file == fileno) ||
						    (mainw->pulsed_read && mainw->pulsed_read->playing_file == fileno))) {
      // get seek_pos from pulse
      if (mainw->pulsed_read && mainw->pulsed_read->playing_file == fileno)
        mainw->aframeno = lives_pulse_get_pos(mainw->pulsed_read) * sfile->fps + 1.;
      else
        mainw->aframeno = lives_pulse_get_pos(mainw->pulsed) * sfile->fps + 1.;
    }
#endif
  }
}


static pthread_mutex_t recursion_mutex = PTHREAD_MUTEX_INITIALIZER;

static frames_t clamp_frame(int clipno, boolean is_fg, frames_t nframe) {
  lives_clip_t *sfile = RETURN_NORMAL_CLIP(clipno);
  if (!sfile) return 0;
  else {
    frames_t first_frame = 1, last_frame = sfile->frames;
    if (LIVES_IS_PLAYING && is_fg) {
      if (!mainw->clip_switched && (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
	last_frame = mainw->playing_sel ? sfile->end : mainw->play_end;
	if (last_frame > sfile->frames) last_frame = sfile->frames;
	first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
	if (first_frame > sfile->frames) first_frame = sfile->frames;
      }
    }
    if (nframe >= first_frame && nframe <= last_frame) return nframe;
    else {
      // get our frame back to within bounds:
      // define even parity (0) as backwards, odd parity (1) as forwards
      // we subtract the lower bound from the new frame, and divide the result by the selection length,
      // rounding towards zero. (nloops)
      // in ping mode this is then added to the original direction, and the resulting parity gives the new direction
      // the remainder after doing the division is then either added to the selection start (forwards)
      /// or subtracted from the selection end (backwards) [if we started backwards then the boundary crossing will be with the
      // lower bound and nloops and the remainder will be negative, so we subtract and add the negatvie value instead]
      // we must also set    lives_direction_t dir;

      double fps = sfile->pb_fps;
      frames_t selrange = (1 + last_frame - first_frame);
      lives_direction_t dir;
      int nloops;

      if (!LIVES_IS_PLAYING || (fabs(fps) < 0.001 && mainw->scratch != SCRATCH_NONE))
	fps = sfile->fps;

      if (is_fg) {
	// check if video stopped playback
	if (mainw->whentostop == STOP_ON_VID_END && !mainw->loop_cont) {
	  mainw->cancelled = CANCEL_VID_END;
	  mainw->scratch = SCRATCH_NONE;
	  return 0;
	}
	mainw->scratch = SCRATCH_JUMP;
      }

      if (first_frame == last_frame) {
	return first_frame;
      }

      // get the starting parity, odd or even, then we add (int)transits
      // and the final parity gives the direction
      dir = LIVES_DIRECTION_PAR(fps >= 0.);

      if (dir == LIVES_DIRECTION_FORWARD && nframe < first_frame) {
	// if FWD and before lower bound, just jump to lower bound
	return first_frame;
      }

      if (dir == LIVES_DIRECTION_BACKWARD && nframe  > last_frame) {
	// if BACK and after upper bound, just jump to upper bound
	return last_frame;
      }

      nframe -= first_frame;
      if (nframe < 0) nframe = -nframe;
      nloops = nframe / selrange;
      if (mainw->ping_pong && (dir == LIVES_DIRECTION_BACKWARD || (clip_can_reverse(clipno)))) {
	dir += nloops + dir + 1;
	dir = LIVES_DIRECTION_PAR(dir);
	if (dir == LIVES_DIRECTION_BACKWARD && !clip_can_reverse(clipno))
	  dir = LIVES_DIRECTION_FORWARD;
      }

      nframe -= nloops * selrange;

      if (dir == LIVES_DIRECTION_FORWARD) {
	nframe += first_frame;
	if (fps < 0.) {
	  // backwards -> forwards
	  if (is_fg) {
	    /// must set norecurse, otherwise we can end up in an infinite loop since dirchange_callback calls
	    // calc_new_playback_position() which in turn calls this function
	    pthread_mutex_lock(&recursion_mutex);
	    dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
			       LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
	    pthread_mutex_unlock(&recursion_mutex);
	  } else sfile->pb_fps = -sfile->pb_fps;
	}
      }
      else {
	nframe = last_frame - nframe;
	if (fps > 0.) {
	  // forwards -> backwards
	  if (is_fg) {
	    pthread_mutex_lock(&recursion_mutex);
	    dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
			       LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
	    pthread_mutex_unlock(&recursion_mutex);
	  } else sfile->pb_fps = -sfile->pb_fps;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  return nframe;
}


static boolean check_audio_limits(int clipno, frames_t nframe) {
  // check if audio stopped playback. The audio player will also be checking this, BUT: we have to check here too
  // before doing any resync, otherwise the video can loop and if the audio is then resynced it may never reach the end !

  // in the case that this happens, mainw->cancelled is set to CANCEL_AUD_END
  
  // after checking this, if mainw->scratch is set to SCRATCH_JUMP, we do a a/v resync if appropriate,
  // then reset mainw->scratch to SCRATCH_JUMP_NORESYNC, to avoid the chance of multiple resyncs

  // see also: AUDIO_OPTS_NO_RESYNC_VPOS and AUDIO_OPTS_IS_LOCKED

  // NOTE: audio resync may result in timing changes, so we return TRUE in case
  boolean retval = FALSE;

  if (AUD_SRC_INTERNAL && AV_CLIPS_EQUAL) {
    if (mainw->whentostop == STOP_ON_AUD_END && !mainw->loop_cont) {
      lives_clip_t *sfile = mainw->files[clipno];
      frames_t first_frame = 1, last_frame = sfile->frames;
      if (LIVES_IS_PLAYING) {
	if (!mainw->clip_switched && (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
	  last_frame = mainw->playing_sel ? sfile->end : mainw->play_end;
	  if (last_frame > sfile->frames) last_frame = sfile->frames;
	  first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
	  if (first_frame > sfile->frames) first_frame = sfile->frames;
	}
      }

      calc_aframeno(clipno);
      if (!check_for_audio_stop(clipno, first_frame + 1, last_frame - 1)) {
	mainw->cancelled = CANCEL_AUD_END;
	mainw->scratch = SCRATCH_NONE;
	return FALSE;
      }
    }

    if (mainw->scratch == SCRATCH_JUMP && !(prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS)
	&& !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
      resync_audio(clipno, (double)nframe);
      retval = TRUE;
    }
  }

  if (mainw->scratch == SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;

  return retval;
}


frames_t calc_new_playback_position(int clipno, boolean is_fg, ticks_t otc, ticks_t *ntc) {
  // returns a frame number (floor) using sfile->last_frameno and ntc - otc
  // ntc is adjusted backwards to timecode of the new frame

  // th returned frame should be passed to clamp_frame() and for the player clip,
  // check audio limits

  ticks_t dtc = *ntc - otc, ddtc;
  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
  double fps;
  frames_t cframe, nframe;

  if (!sfile) return 0;

  cframe = sfile->last_frameno;

  if (pthread_mutex_trylock(&recursion_mutex)) return cframe;
  pthread_mutex_unlock(&recursion_mutex);

  if (sfile->frames == 0 && !mainw->foreign) {
    if (is_fg) mainw->scratch = SCRATCH_NONE;
    return 0;
  }

  fps = sfile->pb_fps;
  if (!LIVES_IS_PLAYING || (fps < 0.001 && fps > -0.001 && mainw->scratch != SCRATCH_NONE)) fps = sfile->fps;

  if (fps < 0.001 && fps > -0.001) {
    *ntc = otc;
    if (is_fg) {
      mainw->scratch = SCRATCH_NONE;
      if (AUD_SRC_INTERNAL) calc_aframeno(clipno);
    }
    return cframe;
  }

  if (dtc < 0) dtc = 0;

  // dtc is delta ticks (last frame time - current time), quantise this to the frame rate and round down
  // - multiplied by the playback rate, this how many frames we will add or subtract
  dtc = q_gint64_floor(dtc, fabs(fps));
  ddtc = dtc;

  if (is_fg) {
    /// if we are scratching we do the following:
    /// the time since the last call is considered to have happened at an increased fps (fwd or back)
    /// thus we multiply dtc by a factor 
    if (mainw->scratch == SCRATCH_FWD || mainw->scratch == SCRATCH_BACK
        || mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) {
      if (mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) dtc *= 4;

      if (mainw->scratch == SCRATCH_BACK || mainw->scratch == SCRATCH_BACK_EXTRA) {
        dtc *= KEY_RPT_INTERVAL * prefs->scratchback_amount
	  * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
	fps = -fps;
      }
      else {
	dtc *= KEY_RPT_INTERVAL * prefs->scratchback_amount
	  * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
      }
    }
  }

  // nframe is our new frame; convert dtc to seconds, and multiply by the frame rate, then add or subtract from current frame number
  // the small constant is just to account for rounding errors
  if (fps >= 0.)
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps + .001);
  else
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps - .001);
  
  if (ddtc != dtc) {
    if (nframe != cframe) mainw->scratch = SCRATCH_REALIGN;
    else mainw->scratch = SCRATCH_NONE;
    dtc = ddtc;
  }

  // ntc is the time when the current frame should have been played
  *ntc = otc + dtc;

  if (*ntc > mainw->currticks && is_fg) {
    break_me("uh oh\n");
    g_print("ERR %ld and %ld and %ld %ld %ld %ld\n", otc, dtc, *ntc, mainw->currticks, mainw->startticks, mainw->syncticks);
  }

  //g_print("FRAMES %d and %d\n", cframe, nframe);

  if (nframe != cframe) {
    if (!IS_NORMAL_CLIP(clipno)) return 1;
    if (mainw->foreign) return sfile->frameno + 1;
  }

  return nframe;
}


//#define SHOW_CACHE_PREDICTIONS

#define ENABLE_PRECACHE
static short scratch = SCRATCH_NONE;

#define ANIM_LIM 500000

// processing
static ticks_t last_anim_ticks;
static uint64_t spare_cycles, last_spare_cycles;
static ticks_t last_kbd_ticks;
static frames_t getahead = -1, test_getahead = -0, bungle_frames;

static boolean recalc_bungle_frames = 0;
static boolean cleanup_preload;
static boolean init_timers = TRUE;
static boolean drop_off = FALSE;
static frames_t lagged, dropped, skipped;

static lives_time_source_t last_time_source;

static frames_t cache_hits = 0, cache_misses = 0;
static double jitter = 0.;

static weed_timecode_t event_start = 0;

void ready_player_one(weed_timecode_t estart) {
  event_start = 0;
  cleanup_preload = FALSE;
  cache_hits = cache_misses = 0;
  lagged = dropped = skipped = 0;
  event_start = estart;
  /// INIT here
  init_timers = TRUE;
  last_kbd_ticks = 0;
  last_time_source = LIVES_TIME_SOURCE_NONE;
  getahead = -1;
  drop_off = FALSE;
  bungle_frames = -1;
  recalc_bungle_frames = FALSE;
  spare_cycles = 0ul;
}


const char *get_cache_stats(void) {
  static char buff[1024];
  lives_snprintf(buff, 1024, "preload caches = %d, hits = %d "
                 "misses = %d,\nframe jitter = %.03f milliseconds.",
                 cache_hits + cache_misses, cache_hits, cache_misses, jitter * 1000.);
  return buff;
}

#ifdef RT_AUDIO
//#define ADJUST_AUDIO_RATE
#endif

int process_one(boolean visible) {
  // INTERNAL PLAYER
  // here we handle playback, as well as the "processing dialog"
  // visible == FALSE                         visible == TRUE
  // for the player, the important variables are:
  // mainw->actual_frame - the last frame number shown
  // sfile->frameno == requested_frame - calculated from sfile->last_frameno and mainw->currticks - mainw->startticks
  // this is the "ideal" frame according to the clock time
  // --
  // we increment (or decrement) mainw->actual_frame each time
  // and compare with requested_frame - if we fall too far behind, we try to jump ahead and "land" on requested_frame
  // (predictive caching)
  // the prediction is set in mainw->pred_frame (the value will be negative, until played, then set to positive)
  // if pred_frame is early we just show and then maybe jump again (bad)
  // if it is too far ahead, we keep the frame around and reshow it until requested_frame or pred_frame is ahead and loaded
  // if there is no jumping, we just set pred_frame to actual_frame + 1 (or -1)
  static frames_t last_req_frame = 0;
  /* #if defined HAVE_PULSE_AUDIO || defined ENABLE_JACK */
  /*   static off_t last_aplay_offset = 0; */
  /* #endif */
  volatile float *cpuload;
  //double cpu_pressure;
  lives_clip_t *sfile = cfile;
  _vid_playback_plugin *old_vpp;
  ticks_t new_ticks;
  lives_time_source_t time_source = LIVES_TIME_SOURCE_NONE;
  static frames_t requested_frame = 0;
  boolean fixed_frame = FALSE;
  boolean show_frame = FALSE, showed_frame;
  boolean skip_lfi = FALSE;
  boolean did_switch = FALSE;
  boolean can_rec;
  static boolean reset_on_tsource_change = FALSE;
  int old_current_file, old_playing_file;
  lives_cancel_t cancelled = CANCEL_NONE;
#ifdef ENABLE_PRECACHE
  int delta = 0;
#endif
  int proc_file = -1;
  int aplay_file = -1;
#ifdef ADJUST_AUDIO_RATE
  double audio_stretch = 1.0;
  ticks_t audio_ticks = -2;
#endif

  // current video playback direction
  lives_direction_t dir = LIVES_DIRECTION_NONE;

  old_current_file = mainw->current_file;

  if (visible) goto proc_dialog;

  //check_mem_status();

  sfile = mainw->files[mainw->playing_file];

  old_playing_file = mainw->playing_file;
  old_vpp = mainw->vpp;

#ifdef ENABLE_JACK
  if (init_timers) {
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) {
      if (mainw->jackd_read && AUD_SRC_EXTERNAL) jack_conx_exclude(mainw->jackd_read, mainw->jackd, TRUE);
#ifdef ENABLE_JACK_TRANSPORT
      if (mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
          && (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER)) {
        if (!mainw->preview && !mainw->foreign) {
          if (!mainw->multitrack)
            jack_pb_start(mainw->jackd_trans, sfile->achans > 0 ? sfile->real_pointer_time : sfile->pointer_time);
          else
            jack_pb_start(mainw->jackd_trans, mainw->multitrack->pb_start_time);
        }
      }
    }
#endif
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) {
      if (prefs->audio_opts & AUDIO_OPTS_AUX_PLAY)
        register_aux_audio_channels(1);
      if (AUD_SRC_EXTERNAL) {
        if (prefs->audio_opts & AUDIO_OPTS_EXT_FX)
          register_audio_client(FALSE);
        mainw->jackd->in_use = TRUE;
      }
    }
    if (mainw->lock_audio_checkbutton)
      aud_lock_act(NULL, LIVES_INT_TO_POINTER(lives_toggle_tool_button_get_active
                                              (LIVES_TOGGLE_TOOL_BUTTON(mainw->lock_audio_checkbutton))));
  }
#endif

  // time is obtained as follows:
  // -  if there is an external transport or clock active, we take our time from that
  // -  else if we have a fixed output framerate (e.g. we are streaming) we take our time from
  //         the system clock
  //  in these cases we adjust our audio rate slightly to keep in synch with video
  // - otherwise, we take the time from soundcard by counting samples played (the normal case),
  //   and we synch video with that; however, the soundcard time only updates when samples are played -
  //   so, between updates we interpolate with the system clock and then adjust when we get a new value
  //   from the card
  //g_print("process_one @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

  lives_nanosleep(100);
  
  last_time_source = time_source;
  time_source = LIVES_TIME_SOURCE_NONE;

  mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
  if (mainw->currticks < mainw->startticks) {
    break_me("cur start");
  }
  if (mainw->currticks == -1) {
    if (time_source == LIVES_TIME_SOURCE_SOUNDCARD) handle_audio_timeout();
    mainw->cancelled = CANCEL_ERROR;
    return mainw->cancelled;
  }

  if (time_source != last_time_source && last_time_source != LIVES_TIME_SOURCE_NONE
      && reset_on_tsource_change) {
    reset_on_tsource_change = FALSE;
    mainw->last_startticks = mainw->startticks = last_anim_ticks
      = mainw->fps_mini_ticks = mainw->currticks;
  }

  if (init_timers) {
    init_timers = FALSE;
    mainw->last_startticks = mainw->startticks = last_anim_ticks
      = mainw->fps_mini_ticks = mainw->currticks;
    mainw->last_startticks--;
    mainw->fps_mini_measure = 0;
    last_req_frame = sfile->frameno - 1;
    getahead = test_getahead = -1;
    mainw->actual_frame = sfile->frameno;
    mainw->offsetticks -= mainw->currticks;
    get_proc_loads(FALSE);
    //reset_on_tsource_change = TRUE;
  }

  mainw->audio_stretch = 1.;

  if (mainw->record_starting) {
#ifdef ENABLE_JACK
    if (mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
      jack_get_rec_avals(mainw->jackd);
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE) {
      pulse_get_rec_avals(mainw->pulsed);
    }
#endif
    mainw->record_starting = FALSE;
  }

#ifdef ADJUST_AUDIO_RATE
  // adjust audio rate slightly if we are behind or ahead
  // shouldn't need this since normally we sync video to soundcard
  // - unless we are controlled externally (e.g. jack transport) or system clock is forced
  if (time_source != LIVES_TIME_SOURCE_SOUNDCARD) {
#ifdef ENABLE_JACK
    if (prefs->audio_src == AUDIO_SRC_INT && prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd &&
        IS_VALID_CLIP(mainw->playing_file)  &&
        sfile->achans > 0 && (!mainw->is_rendering || (mainw->multitrack && !mainw->multitrack->is_rendering)) &&
        (mainw->currticks - mainw->offsetticks) > TICKS_PER_SECOND * 10 && ((audio_ticks = lives_jack_get_time(mainw->jackd)) >
									    mainw->offsetticks || audio_ticks == -1)) {
      if (audio_ticks == -2) audio_ticks = lives_jack_get_time(mainw->jackd);
      if (audio_ticks == -1) {
        if (mainw->cancelled == CANCEL_NONE) {
          if (IS_VALID_CLIP(mainw->playing_file)  && !sfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
          else mainw->cancelled = CANCEL_AUDIO_ERROR;
          return mainw->cancelled;
        }
      }
      if ((audio_stretch = (double)(audio_ticks - mainw->offsetticks) /
	   (double)(mainw->currticks - mainw->offsetticks)) < 2. &&
          audio_stretch > 0.5) {
        // if audio_stretch is > 1. it means that audio is playing too fast
        // < 1. it is playing too slow

        // if too fast we increase the apparent sample rate so that it gets downsampled more
        // if too slow we decrease the apparent sample rate so that it gets upsampled more
        mainw->audio_stretch = audio_stretch;
      }
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_src == AUDIO_SRC_INT && prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed &&
        CURRENT_CLIP_IS_VALID &&
        sfile->achans > 0 && (!mainw->is_rendering || (mainw->multitrack && !mainw->multitrack->is_rendering)) &&
        (mainw->currticks - mainw->offsetticks) > TICKS_PER_SECOND * 10 &&
        ((audio_ticks = lives_pulse_get_time(mainw->pulsed)) >
         mainw->offsetticks || audio_ticks == -1)) {
      if (audio_ticks == -2) audio_ticks = lives_pulse_get_time(mainw->pulsed);
      if (audio_ticks == -1) {
        if (mainw->cancelled == CANCEL_NONE) {
          if (sfile && !sfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
          else mainw->cancelled = CANCEL_AUDIO_ERROR;
          return mainw->cancelled;
        }
      }
      // fps is synched to external source, so we adjust the audio rate to fit
      if ((audio_stretch = (double)(audio_ticks - mainw->offsetticks) / (double)(mainw->clock_ticks
										 + mainw->offsetticks)) < 2. &&  audio_stretch > 0.5) {
        // if audio_stretch is > 1. it means that audio is playing too fast
        // < 1. it is playing too slow

        // if too fast we increase the apparent sample rate so that it gets downsampled more
        // if too slow we decrease the apparent sample rate so that it gets upsampled more
        mainw->audio_stretch = audio_stretch;
      }
    }
#endif
  }
#endif

  /// SWITCH POINT

  /// during playback this is the only place to update certain variables,
  /// e.g. current / playing file, playback plugin. Anywhere else it should be deferred by setting the appropriate
  /// update value (e.g. mainw->new_clip, mainw->new_vpp)
  /// the code will enforce this so that setting the values directly will cause playback to end

  // we allow an exception only when starting or stopping a generator

  if (mainw->current_file != old_current_file || mainw->playing_file != old_playing_file || mainw->vpp != old_vpp) {
    if (!mainw->ignore_clipswitch) {
      mainw->cancelled = CANCEL_INTERNAL_ERROR;
      return FALSE;
    }
    old_current_file = mainw->current_file;
    old_playing_file = mainw->playing_file;
    mainw->ignore_clipswitch = FALSE;
  }

  if (mainw->new_vpp) {
    mainw->vpp = open_vid_playback_plugin(mainw->new_vpp, TRUE);
    mainw->new_vpp = NULL;
    old_vpp = mainw->vpp;
  }

 switch_point:

  if (IS_VALID_CLIP(mainw->new_clip)) {
    if (AV_CLIPS_EQUAL && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
      avsync_force();
    }

    mainw->noswitch = FALSE;

    if (IS_VALID_CLIP(mainw->playing_file)) {
      if (mainw->frame_layer_preload) {
        check_layer_ready(mainw->frame_layer_preload);
        weed_layer_free(mainw->frame_layer_preload);
        mainw->frame_layer_preload = NULL;
        mainw->pred_frame = 0;
        mainw->pred_clip = 0;
        cleanup_preload = FALSE;
      }
    }

#ifdef ENABLE_JACK
    if (mainw->xrun_active) {
      mainw->xrun_active = FALSE;
    }
#endif
    
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      /* if (sfile->arate) { */
      /*   g_print("HIB %d %d %d %d %ld %f %f %ld %ld %d %f\n", sfile->frameno, last_req_frame, mainw->playing_file, */
      /*           aplay_file, aseek_pos, */
      /*           sfile->fps * lives_jack_get_pos(mainw->jackd) + 1., (double)aseek_pos */
      /*           / (double)sfile->arps / 4. * sfile->fps + 1., */
      /*           mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
      /* } */
    }
#endif
    //}
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE) {
      if (sfile->arate) {
        /* aseek_pos = (lives_pulse_get_pos(mainw->pulsed) */
        /*                     - (mainw->startticks - mainw->currticks) / TICKS_PER_SECOND_DBL) */
        /*                    * (double)(sfile->achans * sfile->asampsize / 8. * sfile->arate); */

        g_print("HIB %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", sfile->frameno, sfile->last_frameno,
                last_req_frame, mainw->playing_file,
                aplay_file, sfile->aseek_pos,
                sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., (double)sfile->aseek_pos /
                (double)sfile->arps / 4. * sfile->fps + 1.,
                mainw->currticks, mainw->startticks, sfile->arps, sfile->fps);
      }
    }
#endif

    mainw->switch_during_pb = TRUE;
    sfile->last_play_sequence = mainw->play_sequence;
    do_quick_switch(mainw->new_clip);
    did_switch = TRUE;
    mainw->switch_during_pb = FALSE;

    if (!IS_VALID_CLIP(mainw->playing_file)) {
      if (IS_VALID_CLIP(mainw->new_clip)) goto switch_point;
      mainw->cancelled = CANCEL_INTERNAL_ERROR;
      cancel_process(FALSE);
      return ONE_MILLION + mainw->cancelled;
    }

    sfile = mainw->files[mainw->playing_file];

    mainw->actual_frame = sfile->frameno;

    lagged = dropped = skipped = 0;
    bungle_frames = -1;
    recalc_bungle_frames = FALSE;
    mainw->deltaticks = 0;

    if (sfile->last_play_sequence != mainw->play_sequence) {
      sfile->last_play_sequence = mainw->play_sequence;
      sfile->last_frameno = mainw->actual_frame = sfile->frameno;
    }

#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      /* if (sfile->arate) */
      /*   g_print("HIB2 %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", mainw->actual_frame, sfile->frameno, last_req_frame, */
      /*           mainw->playing_file, aplay_file, aseek_pos, */
      /*           sfile->fps * lives_jack_get_pos(mainw->jackd) + 1., */
      /*           (double)aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
      /*           mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE) {
      /* if (sfile->arate) */
      /*   g_print("HIB2 %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", mainw->actual_frame, sfile->frameno, last_req_frame, */
      /* 	  mainw->playing_file, aplay_file, aseek_pos, */
      /* 	  sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., */
      /* 	  (double)aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
      /* 	  mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
    }
#endif

    cache_hits = cache_misses = 0;

    mainw->new_clip = -1;
    getahead = test_getahead = -1;
    if (prefs->pbq_adaptive) reset_effort();
    // TODO: add a few to bungle_frames in case of decoder unchilling

    if (mainw->record && !mainw->record_paused) mainw->rec_aclip = mainw->current_file;

    if (mainw->scratch != SCRATCH_JUMP) scratch = SCRATCH_JUMP_NORESYNC;
    drop_off = TRUE;
    requested_frame = last_req_frame = sfile->frameno;

    /// playing file should == current_file, but just in case store separate values.
    old_current_file = mainw->current_file;
    old_playing_file = mainw->playing_file;

    cleanup_preload = TRUE;

    if (sfile->arate)
      g_print("seek vals: vid %d %d %ld = %f %d %f\n", sfile->last_frameno, sfile->frameno, sfile->aseek_pos,
              (double)sfile->aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1.,
              sfile->arate, sfile->fps);

    last_time_source = time_source;
    time_source = LIVES_TIME_SOURCE_NONE;
    mainw->last_startticks = mainw->startticks = mainw->currticks
      = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
    g_print("SWITCH %d %d %d %d\n", sfile->frameno, requested_frame, sfile->last_frameno, mainw->actual_frame);
  }

  mainw->noswitch = TRUE;

  /// end SWITCH POINT

  // playing back an event_list
  // here we need to add mainw->offsetticks, to get the correct position when playing back in multitrack
  if (!mainw->proc_ptr && cfile->next_event) {
    // playing an event_list
    if (0) {
      // TODO -retest
#ifdef ENABLE_JACK
      if (mainw->scratch != SCRATCH_NONE && mainw->multitrack && mainw->jackd_trans
          && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
          && (prefs->jack_opts & JACK_OPTS_TRANSPORT_SLAVE)
          && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) {
        // handle transport jump in multitrack : end current playback and restart it from the new position
        ticks_t transtc = q_gint64(jack_transport_get_current_ticks(mainw->jackd_trans), cfile->fps);
        mainw->multitrack->pb_start_event = get_frame_event_at(mainw->multitrack->event_list, transtc, NULL, TRUE);
        if (mainw->cancelled == CANCEL_NONE) mainw->cancelled = CANCEL_EVENT_LIST_END;
      }
#endif
    } else {
      ticks_t currticks = mainw->currticks;
      if (mainw->multitrack) currticks += mainw->offsetticks; // add the offset of playback start time
      if (currticks >= event_start) {
        // see if we are playing a selection and reached the end
        if (mainw->multitrack && mainw->multitrack->playing_sel &&
            get_event_timecode(cfile->next_event) / TICKS_PER_SECOND_DBL >=
            mainw->multitrack->region_end) mainw->cancelled = CANCEL_EVENT_LIST_END;
        else {
          mainw->noswitch = FALSE;
          cfile->next_event = process_events(cfile->next_event, FALSE, currticks);
          mainw->noswitch = TRUE;
          if (!cfile->next_event) mainw->cancelled = CANCEL_EVENT_LIST_END;
        }
      }
    }

    lives_widget_context_update();

    if (mainw->cancelled == CANCEL_NONE) return 0;
    cancel_process(FALSE);
    return mainw->cancelled;
  }

  // free playback

#define CORE_LOAD_THRESH 75.

  //#define SHOW_CACHE_PREDICTIONS
#define TEST_TRIGGER 9999

  /// Values may need tuning for each clip - possible future targets for the autotuner
#define LAGFRAME_TRIGGER 4
#define DROPFRAME_TRIGGER 8
#define JUMPFRAME_TRIGGER 99999999 // we should retain cdata->jump_limit from the initial file open

  if (mainw->currticks - last_kbd_ticks > KEY_RPT_INTERVAL * 100000) {
    // if we have a cached key (ctrl-up, ctrl-down, ctrl-left, crtl-right) trigger it here
    // this is to avoid the keyboard repeat delay (dammit !) so we get smooth trickplay
    // BUT we need a timer sufficiently large so it isn't triggered on every loop, to produce a constant repeat rate
    // but not so large so it doesn't get triggered enough
    if (last_kbd_ticks > 0) handle_cached_keys();
    last_kbd_ticks = mainw->currticks;
  }

  if (mainw->scratch != SCRATCH_NONE && time_source == LIVES_TIME_SOURCE_EXTERNAL) {
    sfile->frameno = sfile->last_frameno = calc_frame_from_time(mainw->playing_file,
								mainw->currticks / TICKS_PER_SECOND_DBL);
    mainw->startticks = mainw->currticks;
    mainw->force_show = TRUE;
    fixed_frame = TRUE;
  }

  new_ticks = mainw->currticks;

  if (new_ticks < mainw->startticks) {
    if (mainw->scratch == SCRATCH_NONE)
      new_ticks = mainw->startticks;
  }

  show_frame = FALSE;
  scratch = SCRATCH_NONE;

  if (sfile->pb_fps != 0.) {
    dir = LIVES_DIRECTION_SIG(sfile->pb_fps);
    if (sfile->delivery == LIVES_DELIVERY_PUSH) {
      if (mainw->force_show) goto update_effort;
    } else {
      // calc_new_playback_postion returns a frame request based on the player mode and the time delta
      //
      // mainw->startticks is the timecode of the last frame shown
      // new_ticks is the (adjusted) current time
      // sfile->last_frameno, sfile->pb_fps (if playing) or sfile->fps (if not) are also used in the calculation
      // as well as selection bounds and loop mode settings (if appropriate)
      //
      // on return, new_ticks is set to either mainw->startticks or the timecode of the next frame to show
      // which will be <= the current time
      // and requested_frame is set to the frame to show. By default this IS the frame we show, but we may vary
      // this depending on the cached frame available. We will always show the cached frame if it is available
      // then either jump ahead again or wait for the timecode to catch up
      // the exception to this is if the requested frame is out of range - then we may adjust it to within bounds,

      // clips can set a target_fps, which is the
      if (sfile->delivery == LIVES_DELIVERY_PUSH_PULL) {
        if (mainw->force_show) goto update_effort;
        if (sfile->target_framerate) {
          if (mainw->inst_fps > sfile->target_framerate) {
            sfile->pb_fps *= .99;
          } else if (mainw->inst_fps < sfile->target_framerate) {
            sfile->pb_fps *= 1.01;
          }
        }
      }

      /* if (mainw->scratch == SCRATCH_NONE && scratch == SCRATCH_NONE */
      /*     && requested_frame > 0) sfile->last_frameno = rrequested_frame; */

      if (!mainw->video_seek_ready) {
	requested_frame = sfile->last_frameno = sfile->frameno;
        new_ticks = mainw->startticks = mainw->currticks;
        mainw->force_show = TRUE;
	fixed_frame = TRUE;
	mainw->scratch = SCRATCH_NONE;
      } else {
        /* g_print("PRE: %ld %ld  %d %f\n", mainw->startticks, new_ticks, sfile->last_frameno, */
        /*         (new_ticks - mainw->startticks) / TICKS_PER_SECOND_DBL * sfile->pb_fps); */
	/* sfile->frameno = 0; */

        requested_frame
          = calc_new_playback_position(mainw->playing_file, FALSE, mainw->startticks, &new_ticks);
        /* g_print("POST: %ld %ld %d (%ld %d)\n", mainw->startticks, new_ticks, requested_frame, mainw->pred_frame, getahead); */
	// by default we play the requested_frame, unless it is invalid
	// if we have a pre-cached frame ready, we may play that instead
	//g_print("ZZ 1 %d\n", skip_lfi);

#ifdef ENABLE_PRECACHE
	if (skip_lfi) {
	  if (is_layer_ready(mainw->frame_layer_preload)) {
	    sfile->frameno = labs(mainw->pred_frame);
	  }
	  else sfile->frameno = mainw->actual_frame;
	  skip_lfi = FALSE;
	  g_print("PASS 1 %d\n", skip_lfi);
	  show_frame = TRUE;
	}
#endif
      }

      //g_print("VALS %ld %ld %ld and %d %d\n", new_ticks, mainw->startticks, mainw->last_startticks, requested_frame, last_req_frame);
      if (new_ticks != mainw->startticks && new_ticks != mainw->last_startticks
	  && (requested_frame != last_req_frame || sfile->frames == 1
	      || (mainw->playing_sel && sfile->start == sfile->end))) {
	if (mainw->fixed_fpsd <= 0. && (!mainw->vpp || mainw->vpp->fixed_fpsd <= 0. || !mainw->ext_playback)) {
	g_print("PASS 1222\n");
	  show_frame = TRUE;
	}
	if (prefs->show_dev_opts) jitter = (double)(mainw->currticks - new_ticks) / TICKS_PER_SECOND_DBL;
      }
    }

#ifdef USE_GDK_FRAME_CLOCK
    if (display_ready) {
      show_frame = TRUE;
      /// not used
      display_ready = FALSE;
    }
#endif
  }
  //g_print("GAHHHHlll is %d\n", getahead);

  // play next frame
  if (LIVES_LIKELY(mainw->cancelled == CANCEL_NONE)) {
    // calculate the audio 'frame' for non-realtime audio players
    // for realtime players, we did this in calc_new_playback_position()
    if (!is_realtime_aplayer(prefs->audio_player)) {
      if (LIVES_UNLIKELY(mainw->loop_cont && (mainw->aframeno > (mainw->audio_end ? mainw->audio_end :
								 sfile->laudio_time * sfile->fps)))) {
	mainw->firstticks = mainw->clock_ticks;
      }
    }

    if (mainw->force_show) {
      new_ticks = mainw->startticks = mainw->currticks;
      g_print("PASS 3331\n");
      show_frame = TRUE;
    } else {
      //g_print("%ld %ld %ld %d %d %d\n", mainw->currticks, mainw->startticks, new_ticks,
      //sfile->last_frameno, requested_frame, last_req_frame);
      if (mainw->fixed_fpsd > 0. || (mainw->vpp  && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)) {
	ticks_t dticks;
	dticks = (mainw->clock_ticks - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL;
	if ((mainw->fixed_fpsd > 0. && (dticks >= 1. / mainw->fixed_fpsd)) ||
	    (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback &&
	     dticks >= 1. / mainw->vpp->fixed_fpsd)) {
	  show_frame = TRUE;
	g_print("PASS 1ss\n");
	}
      }
    }
	
    if (show_frame && sfile->delivery != LIVES_DELIVERY_PUSH) {
      g_print("CSW 1\n");
      // time to show a new frame
      get_proc_loads(FALSE);
      last_spare_cycles = spare_cycles;
      dropped = 0;

      if (mainw->frame_layer_preload && labs(mainw->pred_frame) == sfile->frameno) {
	lagged = (requested_frame - labs(mainw->pred_frame)) * dir;
      }
      else lagged = (requested_frame - mainw->actual_frame) * dir;
      lagged--;
      if (lagged < 0) lagged = 0;
      dropped = dir * (requested_frame - last_req_frame) - 1;
      if (dropped < 0) dropped = 0;

      if (mainw->scratch != SCRATCH_NONE) {
	scratch  = mainw->scratch;
	mainw->scratch = SCRATCH_NONE;
      }

      if (scratch == SCRATCH_REALIGN) {
	sfile->frameno = requested_frame;
      }

      sfile->last_frameno = requested_frame;
      if (new_ticks > mainw->startticks) {
	mainw->last_startticks = mainw->startticks;
	mainw->startticks = new_ticks;
      }

      if (mainw->foreign) {
	if (requested_frame >= sfile->frameno) {
	  load_frame_image(sfile->frameno);
	}
	lives_widget_context_update();
	if (mainw->cancelled != CANCEL_NONE) return mainw->cancelled;
	return 0;
      }

      if (sfile->frames > 1 && prefs->noframedrop
	  && (scratch == SCRATCH_NONE || scratch == SCRATCH_REV)) {
	// if noframedrop is set, we may not skip any frames
	// - the usual situation is that we are allowed to drop late frames
	// in this mode we may be forced to play at a reduced framerate
	sfile->frameno = mainw->actual_frame + dir;
      }

#define SHOW_CACHE_PREDICTIONS
#ifdef ENABLE_PRECACHE
      if (scratch != SCRATCH_NONE && scratch != SCRATCH_JUMP_NORESYNC) {
	getahead = test_getahead = -1;
	cleanup_preload = TRUE;
	mainw->pred_frame = 0;
	//if (!did_switch) sfile->last_play_sequence = mainw->play_sequence;
      }
#endif
      if (mainw->frame_layer_preload) {
	if ((mainw->pred_frame > 0 && is_layer_ready(mainw->frame_layer_preload)) 
	    || (mainw->pred_frame < 0 && (-mainw->pred_frame - requested_frame) * dir >= 0)) {
	  sfile->frameno = mainw->pred_frame;
	}
	else {
	  if (mainw->pred_frame > 0) {
	    g_print("PF %ld not loaded\n", mainw->pred_frame);
	    skip_lfi = TRUE;
	  }
	  else cleanup_preload = TRUE;
	  // *INDENT-OFF*
	}}
      // *INDENT-ON*
	g_print("ZZ 12 %d\n", skip_lfi);

#ifdef ENABLE_PRECACHE
      if (mainw->pred_clip == -1) {
	/// failed load, just reset
	mainw->frame_layer_preload = NULL;
	cleanup_preload = FALSE;
      }
      else if (cleanup_preload) {
	if (mainw->frame_layer_preload && is_layer_ready(mainw->frame_layer_preload)) {
	  check_layer_ready(mainw->frame_layer_preload);
	  weed_layer_free(mainw->frame_layer_preload);
	  mainw->frame_layer_preload = NULL;
	  cleanup_preload = FALSE;
	  mainw->pred_clip = -1;
	  mainw->pred_frame = 0;
	}
      }

      // if we have a cached frame ready, and it's ahead of us, play it, even if it's lagging
      if (mainw->frame_layer_preload && !cleanup_preload) {
	if (mainw->pred_clip == mainw->current_file) {
	  frames_t pframe = labs(mainw->pred_frame);
	  if (dir * (pframe - mainw->actual_frame) > 0 && (getahead < 0 || pframe == getahead
							   || is_layer_ready(mainw->frame_layer_preload))) {
	    sfile->frameno = pframe;
	    //mainw->force_show = TRUE;
	    //g_print("AA 2setsasds to %d\n", sfile->frameno);
	  }
	  if (pframe == requested_frame) cache_hits++;
	  else {
	    if (prefs->dev_show_caching) {
	      g_print("WASTED cache frame %d !!!! range was %d to %d or was not ready\n",
		      pframe, requested_frame, sfile->frameno);
	      cache_misses++;
	    }
	    if ((mainw->pred_frame > 0 && dir * (pframe - mainw->actual_frame) < 0)
		|| (mainw->pred_frame < 0 && dir * (pframe - mainw->actual_frame) == 0)) {
	      cleanup_preload = TRUE;
	      if (!fixed_frame) sfile->frameno = 0;
	    }
	    if (sfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->playing_file, sfile->frameno))
	      sfile->last_vframe_played = sfile->frameno;
#endif
	  }
	}
      }

    update_effort:
      if (prefs->pbq_adaptive && scratch == SCRATCH_NONE) {
	if (requested_frame != last_req_frame || sfile->frames == 1) {
	  if (sfile->frames == 1) {
	    if (!spare_cycles) {
	      //if (sfile->ext_src_type == LIVES_EXT_SRC_FILTER) {
	      //weed_plant_t *inst = (weed_plant_t *)sfile->ext_src;
	      double target_fps = fabs(sfile->pb_fps);//weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);
	      if (target_fps) {
		mainw->inst_fps = get_inst_fps(FALSE);
		if (scratch == SCRATCH_NONE && mainw->inst_fps < target_fps) {
		  update_effort(1 + target_fps / mainw->inst_fps, TRUE);
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*
	  else {
	    /// update the effort calculation with dropped frames and spare_cycles
	    if (dropped > 0) update_effort(dropped, TRUE);
	  }
	  update_effort(spare_cycles + 1, FALSE);
	}
      }

      if (sfile->delivery == LIVES_DELIVERY_PUSH
	  || (sfile->delivery == LIVES_DELIVERY_PUSH_PULL && mainw->force_show)) {
	goto play_frame;
      }
	g_print("ZZ 144 %d\n", skip_lfi);

#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("dropped = %d, %d scyc = %ld %d %d\n", dropped, mainw->effort, spare_cycles, requested_frame, sfile->frameno);
#endif
      drop_off = FALSE;

      g_print("xxxxGAHHHH is %d\n", getahead);

      if (mainw->force_show || ((requested_frame != last_req_frame ||
				 (show_frame && sfile->frames == 1 && sfile->frameno == 1)
				 || (mainw->playing_sel && sfile->start == sfile->end))
#ifdef ENABLE_PRECACHE
				&& getahead < 0)
	  || (getahead > 0 && labs(mainw->pred_frame) == getahead && mainw->frame_layer_preload && !cleanup_preload
#endif
	      )) {
	spare_cycles = 0;
	mainw->record_frame = requested_frame;

	/// note the audio seek position at the current frame. We will use this when switching clips
	aplay_file = get_aplay_clipno();
	if (IS_VALID_CLIP(aplay_file)) {
#if 0
	  if (prefs->audio_player == AUD_PLAYER_NONE) {
	    aplay_file = mainw->nullaudio->playing_file;
	    if (IS_VALID_CLIP(aplay_file))
	      mainw->files[aplay_file]->aseek_pos = nullaudio_get_seek_pos();
	  }
#endif
	}

      play_frame:
	//g_print("DISK PR is %f\n", mainw->disk_pressure);
	showed_frame = FALSE;

	if (!sfile->frameno || (sfile->frameno - mainw->actual_frame) * dir < 0
	    || (!mainw->frame_layer_preload && sfile->frameno == mainw->actual_frame)) {
	  sfile->frameno = mainw->actual_frame + (dropped + 1) * dir;
	}

#ifdef ENABLE_PRECACHE
	if (mainw->frame_layer_preload && !cleanup_preload) {
	  if (is_layer_ready(mainw->frame_layer_preload)) {
	    sfile->frameno = labs(mainw->pred_frame);
	  }
	  else sfile->frameno = mainw->actual_frame;
	}
#endif
	if (mainw->force_show || mainw->pred_clip != -1
	    || (mainw->pred_clip == -1 && *(cpuload = get_core_loadvar(0)) < CORE_LOAD_THRESH)) {
	  boolean can_realign = FALSE;

	  mainw->force_show = FALSE;
	  skipped = (sfile->frameno - mainw->actual_frame) * dir - 1;
	  if (skipped < 0) skipped = 0;

	  // will reset mainw->scratch
	  if (skipped > 0) update_effort(skipped / sfile->pb_fps, TRUE);
	  else update_effort(sfile->pb_fps, FALSE);

	  // temp value
	  dropped = dir * (sfile->frameno - sfile->last_frameno);

	  sfile->frameno = clamp_frame(mainw->playing_file, TRUE, sfile->frameno);
	  if (mainw->cancelled != CANCEL_NONE) {
	    cancel_process(FALSE);
	    return ONE_MILLION + mainw->cancelled;
	  }
	  if (mainw->scratch == SCRATCH_JUMP) {
	    can_realign = TRUE;
	  }
	g_print("ZZ 1kkk %d\n", skip_lfi);

	  // can change in clamp_frame()
	  dir = LIVES_DIRECTION_SIG(sfile->pb_fps);

	  if (!check_audio_limits(mainw->playing_file, sfile->frameno)) {
	    if (mainw->cancelled != CANCEL_NONE) {
	      cancel_process(FALSE);
	      return ONE_MILLION + mainw->cancelled;
	    }
	    if (can_realign) {
	      sfile->last_frameno = sfile->frameno - dropped * dir;
	      mainw->record_frame = requested_frame = sfile->frameno + lagged * dir;
	    }
	  }
	  else {
	    sfile->last_frameno = sfile->frameno;
	  }

	  dropped = dir * (requested_frame - last_req_frame) - 1;
	  if (dropped < 0) dropped = 0;

	  mainw->actual_frame = sfile->frameno;

	  g_print("PLAY %d %d %d %d %ld %ld %d %d\n", sfile->frameno, requested_frame, sfile->last_frameno,
		  mainw->actual_frame,
		  mainw->currticks, mainw->startticks, mainw->video_seek_ready, mainw->audio_seek_ready);

	  load_frame_image(sfile->frameno);
	  showed_frame = TRUE;
	}

	can_rec = FALSE;

#define REC_IDEAL
#ifdef REC_IDEAL
	if (mainw->scratch == SCRATCH_JUMP || mainw->scratch == SCRATCH_JUMP_NORESYNC
	    || mainw->record_frame != requested_frame || new_ticks != mainw->last_startticks)
	  can_rec = TRUE;
#else
	if (showed_frame) can_rec = TRUE;
#endif
	if (can_rec) {
	  if (mainw->record && !mainw->record_paused) {
	    void **eevents;
	    weed_event_list_t *event_list;
	    int64_t *frames;
	    int *clips;
	    ticks_t actual_ticks;
	    boolean rec_after_pb = FALSE, rec_to_scrap = FALSE;
	    int numframes, nev;
	    int fg_file = mainw->playing_file;
	    frames_t fg_frame;
	    int bg_file = (IS_VALID_CLIP(mainw->blend_file)
			   && (prefs->tr_self || (mainw->blend_file != mainw->playing_file)))
	      ? mainw->blend_file : -1;
	    frames_t bg_frame = (bg_file > 0 && (prefs->tr_self || (bg_file != mainw->playing_file)))
	      ? mainw->files[bg_file]->frameno : 0;

	    // should we record the output from the playback plugin ?
	    if ((prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
		(mainw->vpp->capabilities & VPP_CAN_RETURN))
	      rec_after_pb = TRUE;

	    if (rec_after_pb || !CURRENT_CLIP_IS_NORMAL ||
		(prefs->rec_opts & REC_EFFECTS && bg_file != -1 && !IS_NORMAL_CLIP(bg_file)))
	      rec_to_scrap = TRUE;

#ifdef REC_IDEAL
	    if (showed_frame && !rec_to_scrap && mainw->scratch != SCRATCH_JUMP && mainw->scratch != SCRATCH_JUMP_NORESYNC
		&& mainw->record_frame == requested_frame && new_ticks == mainw->last_startticks)
	      can_rec = FALSE;
	    else can_rec = TRUE;
	    if (new_ticks != mainw->last_startticks) actual_ticks = new_ticks;
	    else actual_ticks = mainw->currticks;
	    fg_frame = mainw->record_frame;
#else
	    can_rec = showed_frame;
	    actual_ticks = mainw->currticks;
	    fg_frame = mainw->actual_frame;
#endif

	    if (can_rec) {
	      if (rec_to_scrap) {
		fg_file = mainw->scrap_file;
		fg_frame = mainw->files[mainw->scrap_file]->frames;
		bg_file = -1;
		bg_frame = 0;
	      }
 
	      numframes = (bg_file == -1) ? 1 : 2;
	      clips = (int *)lives_malloc(numframes * sizint);
	      frames = (int64_t *)lives_malloc(numframes * 8);

	      clips[0] = fg_file;
	      frames[0] = (int64_t)fg_frame;
	      if (numframes == 2) {
		clips[1] = bg_file;
		frames[1] = (int64_t)bg_frame;
	      }

	      // MUST do this before locking event_list_mutex, else we can fall into a deadlock
	      eevents = get_easing_events(&nev);
	      pthread_mutex_lock(&mainw->event_list_mutex);

	      /// usual function to record a frame event
	      if ((event_list = append_frame_event(mainw->event_list, actual_ticks,
						   numframes, clips, frames)) != NULL) {
		if (!mainw->event_list) mainw->event_list = event_list;
		mainw->elist_eom = FALSE;
		if (eevents || scratch == SCRATCH_JUMP || scratch == SCRATCH_JUMP_NORESYNC
		    || mainw->scrap_file_size != -1 ||
		    (mainw->rec_aclip != -1 && (prefs->rec_opts & REC_AUDIO))) {
		  weed_plant_t *event = get_last_frame_event(mainw->event_list);
		  if (eevents) {
		    weed_set_voidptr_array(event, LIVES_LEAF_EASING_EVENTS, nev, eevents);
		    lives_free(eevents);
		  }

		  if (scratch == SCRATCH_JUMP || scratch == SCRATCH_JUMP_NORESYNC) {
		    weed_set_int_value(event, LIVES_LEAF_SCRATCH, scratch);
		  }

		  if (mainw->scrap_file_size != -1)
		    weed_set_int64_value(event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET, mainw->scrap_file_size);
		  if (!mainw->mute) {
		    if (mainw->rec_aclip != -1) {
		      if (AUD_SRC_INTERNAL || (mainw->rec_aclip == mainw->ascrap_file)) {
			weed_event_t *xevent = get_prev_frame_event(event);
			if (!xevent) xevent = event;

			if (mainw->rec_aclip == mainw->ascrap_file) {
			  mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
			    (double)(mainw->files[mainw->ascrap_file]->arps
				     * mainw->files[mainw->ascrap_file]->achans *
				     mainw->files[mainw->ascrap_file]->asampsize >> 3);
			  mainw->rec_avel = 1.;
			} else {
			  if (1 || !did_switch) {
#ifdef ENABLE_JACK
			    if (mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
			      jack_get_rec_avals(mainw->jackd);
			    }
#endif
#ifdef HAVE_PULSE_AUDIO
			    if (mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE) {
			      pulse_get_rec_avals(mainw->pulsed);
			    }
#endif
			  }
			}
			insert_audio_event_at(xevent, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel);
			// *INDENT-OFF*
		      }}}}}
	      // *INDENT-ON*
	      else mainw->elist_eom = TRUE;
	      pthread_mutex_unlock(&mainw->event_list_mutex);
	      mainw->record_starting = FALSE;
	      mainw->rec_aclip = -1;
	    }
	  }
	}

	scratch = SCRATCH_NONE;

	if (showed_frame) {
	  mainw->inst_fps = get_inst_fps(FALSE);
	  if (prefs->show_player_stats) {
	    mainw->fps_measure++;
	  }
	  mainw->fps_mini_measure++;
	}

	mainw->last_startticks = mainw->startticks;
	// end load_frame_image()
	//}
      } else spare_cycles++;
	g_print("ZZ 1lll %d\n", skip_lfi);

#ifdef ENABLE_PRECACHE
      if (mainw->frame_layer_preload) {
	if (mainw->pred_clip == mainw->playing_file) {
	  if (mainw->pred_clip != mainw->current_file
	      || (mainw->pred_frame < 0 && (-dir * (mainw->pred_frame - requested_frame) < 0))
	      || (getahead > 0 && mainw->pred_frame != -getahead)) {
	    cleanup_preload = TRUE;
	    drop_off = FALSE;
	  }
	} else {
	  cleanup_preload = TRUE;
	}
      }
#endif

      if (!prefs->vj_mode) {
	//g_print("lfi done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
	if (mainw->last_display_ticks == 0) mainw->last_display_ticks = mainw->clock_ticks;
	else {
	  if (mainw->vpp && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.)
	    mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->vpp->fixed_fpsd;
	  else if (mainw->fixed_fpsd > 0.)
	    mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->fixed_fpsd;
	  else mainw->last_display_ticks = mainw->clock_ticks;
	}
      } // end show_frame
      else spare_cycles++;

#ifdef ENABLE_PRECACHE
      if (cleanup_preload) {
	if (mainw->frame_layer_preload) {
	  if (getahead > 0 || is_layer_ready(mainw->frame_layer_preload)) {
	    //wait_for_cleaner();
	    if (mainw->pred_clip > 0) {
	      check_layer_ready(mainw->frame_layer_preload);
	      weed_layer_free(mainw->frame_layer_preload);
	    }
	    mainw->frame_layer_preload = NULL;
	    mainw->pred_frame = 0;
	    mainw->pred_clip = 0;
	    cleanup_preload = FALSE;
	    // *INDENT-OFF*
	  }
	  // *INDENT-ON*
#endif
	}}

#ifdef ENABLE_PRECACHE
      if (recalc_bungle_frames) {
	/// we want to avoid the condition where we are constantly seeking ahead and because the seek may take a while
	/// to happen, we immediately need to seek again. This will cause the video stream to stutter.
	/// So to try to avoid this
	/// we will do an an EXTRA jump forwards which ideally will give the player a chance to catch up
	/// - in this condition, instead of showing the reqiested frame we will do the following:
	/// - if we have a cached frame, we will show that; otherwise we will advance the frame by 1 from the last frame.
	///   and show that, since we can decode it quickly.
	/// - following this we will cache the "getahead" frame. The player will then render the getahead frame
	//     and keep reshowing it until the time catches up.
	/// (A future update will implement a more flexible caching system which will enable the possibility
	/// of caching further frames while we waut)
	/// - if we did not advance enough, we show the getahead frame and then do a larger jump.
	// ..'bungle frames' is a rough estimate of how far ahead we need to jump so that we land exactly
	/// on the player's frame. 'getahead' is the target frame.
	/// after a jump, we adjust bungle_frames to try to jump more acurately the next tine
	/// however, it is impossible to get it right 100% of the time, as the actual value can vary unpredictably
	/// 'test_getahead' is used so that we can sometimes recalibrate without actually jumping the frame
	/// in future, we could also get a more accurate estimate by integrating statistics from the decoder.
	/// - useful values would be the frame decode time, keyframe positions, seek time to keyframe, keyframe decode time.
	delta = (test_getahead - requested_frame) * dir;
	if (prefs->dev_show_caching) {
	  g_print("gah (%d) %d, act %d %d %d, bungle %d, shouldabeen %d %s", mainw->effort, test_getahead,
		  mainw->actual_frame, sfile->frameno, requested_frame,
		  bungle_frames, bungle_frames - delta, getahead == -1 ? "(calibrating)" : "");
	  if (delta < 0) g_print(" !!!!!\n");
	  if (delta == 0) g_print(" EXACT\n");
	  if (delta > 0) g_print(" >>>>\n");
	}
	if (delta == 0)  bungle_frames++;
	if (delta > 0 && delta < 3 && bungle_frames > 1) bungle_frames--;
	else bungle_frames += (requested_frame - test_getahead) * dir;
	if (bungle_frames <= -dir) bungle_frames = 0;
	if (delta >= 0 && getahead > -1) drop_off = TRUE;
	recalc_bungle_frames = FALSE;
      }
    }
#endif

	    
#ifdef ENABLE_PRECACHE
    if (!mainw->frame_layer_preload) {
      if (sfile->clip_type == CLIP_TYPE_FILE && scratch == SCRATCH_NONE
	  && is_virtual_frame(mainw->playing_file, requested_frame)
	  && lagged > 0 && ((sfile->pb_fps < 0. && !clip_can_reverse(mainw->playing_file))
			    || abs(sfile->frameno - sfile->last_vframe_played) >= JUMPFRAME_TRIGGER
			    || lagged >= MIN(TEST_TRIGGER, LAGFRAME_TRIGGER))) {
	if (prefs->dev_show_caching) {
	  if (abs(sfile->frameno - sfile->last_vframe_played) >= JUMPFRAME_TRIGGER) {
	    lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
	    if (cdata) {
	      g_print("decoder: seek flags = %d, jump_limit = %ld, max_fps = %.4f\n",
		      cdata->seek_flag,
		      cdata->jump_limit, cdata->max_decode_fps);
	      g_print("vframe jump will be %d\n", requested_frame - sfile->last_vframe_played);
	    }
	  }
	}
	if (bungle_frames <= -dir || bungle_frames == 0) bungle_frames = dir;
	//bungle_frames += requested_frame - mainw->actual_frame - dir;

	// TRY TO PREDICT the next frame from the target using statistical method
	test_getahead = requested_frame + bungle_frames * dir;

	if (test_getahead < 1 || test_getahead > sfile->frames) test_getahead = -1;
	else {
	  if ((lagged > 0 && sfile->pb_fps < 0. && !clip_can_reverse(mainw->current_file))
	      || lagged >= LAGFRAME_TRIGGER || dropped >= DROPFRAME_TRIGGER) {
	    lives_decoder_t *dplug = NULL;
	    lives_decoder_sys_t *dpsys = NULL;
	    int64_t pred_frame;

	    if (prefs->dev_show_caching) {
	      g_print("getahead jumping to %d\n", test_getahead);
	    }

	    getahead = test_getahead;
	    recalc_bungle_frames = TRUE;

	    if (!mainw->pred_frame) {
	      pred_frame = getahead;
	      //g_print("CACHE 1122 %ld and %d\n", mainw->pred_frame, mainw->actual_frame);

	      if (pred_frame >= 1 && pred_frame < sfile->frames) {
		//
		if (sfile->clip_type == CLIP_TYPE_FILE) {
		  dplug = (lives_decoder_t *)sfile->ext_src;
		  if (dplug) dpsys = (lives_decoder_sys_t *)dplug->dpsys;
		}

		if (!mainw->xrun_active && !did_switch) {
		  for (int zz = 0; ; zz++) {
		    int64_t new_pred_frame = requested_frame + zz * dir;
		    double est_time;

		    if ((new_pred_frame - pred_frame) * dir > 1 || new_pred_frame > sfile->frames
			|| new_pred_frame < 1) break;

		    if (is_virtual_frame(mainw->playing_file, new_pred_frame)) {
		      if (!dpsys || !dpsys->estimate_delay) break;
		      est_time = (*dpsys->estimate_delay)(dplug->cdata,
							  get_indexed_frame(mainw->playing_file, new_pred_frame));
		      if (fpclassify(est_time) != FP_NORMAL) break;
		    } else {
		      // img timings
		      est_time = sfile->img_decode_time;
		    }

		    if (est_time <= 0.) break;

		    if (est_time <= fabs((double)(zz + .5) / sfile->pb_fps)) {
		      pred_frame = new_pred_frame;
		      //g_print("CACHE 112233 %ld and %d\n", mainw->pred_frame, mainw->actual_frame);
		      break;
		    }
		  }
		}
	      }

	      g_print("estimate adjusted to frame %ld\n", pred_frame);

	      getahead = pred_frame;
	      if (getahead < 1) getahead = 1;
	      if (getahead > sfile->frames) getahead = sfile->frames;
	    }
	  }
	  else getahead = -1;
	}
      }
    }
  }
#endif

  // paused
  if (LIVES_UNLIKELY(sfile->play_paused)) {
    mainw->startticks = mainw->currticks;
    mainw->video_seek_ready = TRUE;
  } else {
    cpuload = get_core_loadvar(0);
    if (!mainw->multitrack && scratch == SCRATCH_NONE && IS_NORMAL_CLIP(mainw->playing_file)
	&& mainw->playing_file > 0
	&& (*cpuload > CORE_LOAD_THRESH
	    || (spare_cycles > 0ul && last_spare_cycles > 0ul)
	    || (getahead > -1 && !mainw->frame_layer_preload))) {
#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("PRELOADING (%d %d %lu %p):", sfile->frameno, dropped,
      //spare_cycles, mainw->frame_layer_preload);
#endif
#ifdef ENABLE_PRECACHE
      if (!mainw->frame_layer_preload) {
	if (!mainw->preview) {
	  mainw->pred_frame = 0;
	  mainw->pred_clip = mainw->playing_file;
	  if (getahead > 0) {
	    if (dir * (getahead - mainw->actual_frame) > 0
		&& dir * (getahead - requested_frame) > 0) {
	      mainw->pred_frame = getahead;
	      g_print("CACHE xx1122 %ld and %d\n", mainw->pred_frame, mainw->actual_frame);
	    }
	    else getahead = -1;
	    g_print("GAHHHH is %d\n", getahead);
	  }
	  else {
	    lives_decoder_t *dplug = NULL;
	    lives_decoder_sys_t *dpsys = NULL;
	    int64_t pred_frame = mainw->pred_frame;;
	    double last_est_time = -1.;
	    if (pred_frame == 0) {
	      frames_t diff = dropped / 2.;
	      if (diff < 1) diff = 1;
	      pred_frame = mainw->actual_frame + diff * dir;
	      g_print("CACHE sdsdhuh1122 %ld and %d %d j %d\n", pred_frame, mainw->actual_frame, dropped, skipped);
	    }
	    if (sfile->clip_type == CLIP_TYPE_FILE) {
	      dplug = (lives_decoder_t *)sfile->ext_src;
	      if (dplug) dpsys = (lives_decoder_sys_t *)dplug->dpsys;
	    }

	    if (!mainw->xrun_active && !did_switch) {
	      frames_t min = pred_frame - requested_frame;
	      for (int zz = 0; zz < 6; zz++) {
		int64_t new_pred_frame = pred_frame;
		double est_time;
		if (is_virtual_frame(mainw->playing_file, new_pred_frame)) {
		  if (!dpsys || !dpsys->estimate_delay) break;
		  est_time = (*dpsys->estimate_delay)(dplug->cdata, get_indexed_frame(mainw->playing_file,
										      new_pred_frame));
		  if (fpclassify(est_time) != FP_NORMAL) {
		    est_time = -1.;
		    break;
		  }
		} else {
		  // png timings
		  est_time = sfile->img_decode_time;
		}

		if (est_time <= 0.) break;
		if (last_est_time > -1. && est_time > last_est_time) break;

		if (new_pred_frame == requested_frame + dir) {
		  new_pred_frame = requested_frame + (frames_t)(est_time * sfile->pb_fps + .5 * dir);
		  if (new_pred_frame == requested_frame) {
		    pred_frame = requested_frame + dir;
		    break;
		  }
		  break;
		}
		new_pred_frame = requested_frame + (frames_t)(est_time * sfile->pb_fps + .5 * dir);
		if (new_pred_frame == requested_frame) {
		  new_pred_frame += dir;
		  continue;
		}
		      
		if (dir * (new_pred_frame - mainw->actual_frame) < 1) {
		  new_pred_frame += dir;
		  continue;
		}
		      
		if (dir * (new_pred_frame - mainw->actual_frame) == 1) {
		  pred_frame = new_pred_frame;
		  break;
		}

		if (dir * (new_pred_frame - requested_frame) < min) {
		  min = dir * (new_pred_frame - requested_frame);
		  pred_frame = new_pred_frame;
		}

		if (new_pred_frame == pred_frame) break;
		g_print("CACHE 11dsadasd22 %ld and %d\n", pred_frame, mainw->actual_frame);
		//if (est_time < last_est_time) break;
		last_est_time = est_time;
	      }
	    }
	    mainw->pred_frame = pred_frame;
	    g_print("CACHE 11 %ld and %d %d\n", mainw->pred_frame, mainw->actual_frame, requested_frame);
	  }

	  if (mainw->pred_frame > 0 && mainw->pred_frame < sfile->frames
	      && dir * (mainw->pred_frame - mainw->actual_frame) > 0) {
	    const char *img_ext = get_image_ext_for_type(sfile->img_type);
	    if (!is_virtual_frame(mainw->pred_clip, mainw->pred_frame)) {
	      mainw->frame_layer_preload = lives_layer_new_for_frame(mainw->pred_clip, mainw->pred_frame);
	      pull_frame_threaded(mainw->frame_layer_preload, img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
	    } else {
	      // if the target is a clip-frame we have to decode it now, since we cannot simply decode 2 frames
	      // simultaneously
	      // (although it could be possible in the future to have 2 clone decoders and have them leapfrog...)
	      // NOTE: change to labs when frames_t updated
	      mainw->frame_layer_preload =
		lives_layer_new_for_frame(mainw->pred_clip, mainw->pred_frame);
	      if (!pull_frame_at_size(mainw->frame_layer_preload, img_ext,
				      (weed_timecode_t)mainw->currticks,
				      sfile->hsize, sfile->vsize, WEED_PALETTE_END)) {
		if (mainw->frame_layer_preload) {
		  check_layer_ready(mainw->frame_layer_preload);
		  weed_layer_free(mainw->frame_layer_preload);
		  mainw->frame_layer_preload = NULL;
		  mainw->pred_clip = -1;
		}
		if (prefs->dev_show_caching) {
		  g_print("failed to load frame %ld\n", mainw->pred_frame);
		}
	      }
	    }
	    if (mainw->pred_clip != -1) {
	      if (prefs->dev_show_caching) {
		double av = (double)get_aplay_offset()
		  / (double)sfile->arate / (double)(sfile->achans * (sfile->asampsize >> 3));
		g_print("cached frame %ld for %d %f\n", mainw->pred_frame, requested_frame, av);
	      }
	    }
	    // *INDENT-OFF*
	  }}
	else mainw->pred_frame = 0;
      }}
    
    if (mainw->video_seek_ready) {
      last_req_frame = requested_frame;
      sfile->last_frameno = requested_frame;

      if (new_ticks > mainw->startticks) {
	mainw->last_startticks = mainw->startticks;
	mainw->startticks = new_ticks;
      }
    }
    //g_print("GAHHHHrrr is %d\n", getahead);
#ifdef SHOW_CACHE_PREDICTIONS
    //g_print("frame %ld already in cache\n", mainw->pred_frame);
#endif
#endif
  }

  // *INDENT-ON*

  if (!spare_cycles) get_proc_loads(FALSE);

  cancelled = THREADVAR(cancelled) = mainw->cancelled;

 proc_dialog:
  if (visible) {
    proc_file = THREADVAR(proc_file) = mainw->current_file;
    sfile = mainw->files[proc_file];
    if (!mainw->proc_ptr) {
      // fixes a problem with opening preview with bg generator
      if (cancelled == CANCEL_NONE) {
	cancelled = THREADVAR(cancelled) = mainw->cancelled = CANCEL_NO_PROPOGATE;
      }
    } else {
      if (LIVES_IS_SPIN_BUTTON(mainw->framedraw_spinbutton))
	lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), 1,
				    mainw->proc_ptr->frames_done);
      // set the progress bar %
      update_progress(visible, proc_file);
    }
  } else proc_file = THREADVAR(proc_file) = mainw->playing_file;

  cancelled = THREADVAR(cancelled) = mainw->cancelled;
  if (LIVES_LIKELY(cancelled == CANCEL_NONE)) {
    if (proc_file != mainw->current_file) return 0;
    else {
      lives_rfx_t *xrfx;

      if ((xrfx = (lives_rfx_t *)mainw->vrfx_update) != NULL && fx_dialog[1]) {
	// the audio thread wants to update the parameter window
	mainw->vrfx_update = NULL;
	update_visual_params(xrfx, FALSE);
      }

      // the audio thread wants to update the parameter scroll(s)
      if (mainw->ce_thumbs) ce_thumbs_apply_rfx_changes();

      /// we are permitted to switch clips here under very restricitive circumstances, e.g when opening a clip
      if (mainw->cs_permitted) {
	mainw->cs_is_permitted = TRUE;
	old_current_file = mainw->current_file;
      }

      if (mainw->currticks - last_anim_ticks > ANIM_LIM || mainw->currticks < last_anim_ticks) {
	// a segfault here can indicate memory corruption in an FX plugin
	last_anim_ticks = mainw->currticks;
	lives_widget_context_update();
      }

      //#define LOAD_ANALYSE_TICKS MILLIONS(10)

      /* if (mainw->currticks - last_ana_ticks > LOAD_ANALYSE_TICKS) { */
      /*   cpu_pressure = analyse_cpu_stats(); */
      /*   last_ana_ticks = mainw->currticks; */
      /* } */

      /// if we did switch clips then cancel the dialog without cancelling the background process
      if (mainw->cs_is_permitted) {
	mainw->cs_is_permitted = FALSE;
	if (mainw->current_file != old_current_file) mainw->cancelled = CANCEL_NO_PROPOGATE;
      }

      if (!CURRENT_CLIP_IS_VALID) {
	if (IS_VALID_CLIP(mainw->new_clip)) goto switch_point;
	mainw->cancelled = CANCEL_INTERNAL_ERROR;
      }
    }
    if (mainw->cancelled != CANCEL_NONE) {
      cancel_process(FALSE);
      return ONE_MILLION + mainw->cancelled;
    }
    return 0;
  }
 
  if (LIVES_IS_PLAYING) {
    mainw->jack_can_stop = FALSE;
  }

  cancel_process(visible);
  //skipit:

  return 2000000 + mainw->cancelled;
}



boolean clip_can_reverse(int clipno) {
  if (!LIVES_IS_PLAYING || mainw->internal_messaging || mainw->is_rendering || mainw->is_processing
      || !IS_VALID_CLIP(clipno) || mainw->preview) return FALSE;
  else {
    lives_clip_t *sfile = mainw->files[clipno];
    if (sfile->clip_type == CLIP_TYPE_DISK) return TRUE;
    if (sfile->next_event) return FALSE;
    if (sfile->clip_type == CLIP_TYPE_FILE) {
      lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
      if (!cdata || !(cdata->seek_flag & LIVES_SEEK_FAST_REV)) return FALSE;
    }
  }
  return TRUE;
}


// objects / intents

// TODO - will be part of LIVES_INTENTION_CREATE_INSTANCE (req == subtype)
lives_object_instance_t *lives_player_inst_create(uint64_t subtype) {
  char *choices[2];
  weed_plant_t *gui;
  lives_obj_attr_t *attr;
  lives_object_instance_t *inst = lives_object_instance_create(OBJECT_TYPE_PLAYER, subtype);
  inst->state = OBJECT_STATE_NORMAL;
  attr = lives_object_declare_attribute(inst, AUDIO_ATTR_SOURCE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_SOURCE, _("Source"), WEED_PARAM_INTEGER);
  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  choices[0] = lives_strdup("Internal");
  choices[1] = lives_strdup("External");
  weed_set_string_array(gui, WEED_LEAF_CHOICES, 2, choices);
  lives_free(choices[0]); lives_free(choices[1]);
  lives_object_declare_attribute(inst, AUDIO_ATTR_RATE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_RATE, _("Rate Hz"), WEED_PARAM_INTEGER);
  lives_object_declare_attribute(inst, AUDIO_ATTR_CHANNELS, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_CHANNELS, _("Channels"), WEED_PARAM_INTEGER);
  lives_object_declare_attribute(inst, AUDIO_ATTR_SAMPSIZE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_SAMPSIZE, _("Sample size (bits)"), WEED_PARAM_INTEGER);
  lives_object_declare_attribute(inst, AUDIO_ATTR_STATUS, WEED_SEED_INT64);
  lives_object_declare_attribute(inst, AUDIO_ATTR_SIGNED, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_SIGNED, _("Signed"), WEED_PARAM_SWITCH);
  attr = lives_object_declare_attribute(inst, AUDIO_ATTR_ENDIAN, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_ENDIAN, _("Endian"), WEED_PARAM_INTEGER);
  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  choices[0] = lives_strdup("Little endian");
  choices[1] = lives_strdup("Big endian");
  weed_set_string_array(gui, WEED_LEAF_CHOICES, 2, choices);
  lives_free(choices[0]); lives_free(choices[1]);
  lives_object_declare_attribute(inst, AUDIO_ATTR_FLOAT, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_FLOAT, _("Is float"), WEED_PARAM_SWITCH);
  lives_object_declare_attribute(inst, AUDIO_ATTR_INTERLEAVED, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, AUDIO_ATTR_INTERLEAVED, _("Interleaved"), WEED_PARAM_SWITCH);
  lives_object_declare_attribute(inst, AUDIO_ATTR_DATA_LENGTH, WEED_SEED_INT64);
  lives_object_declare_attribute(inst, AUDIO_ATTR_DATA, WEED_SEED_VOIDPTR);
  return inst;
}
