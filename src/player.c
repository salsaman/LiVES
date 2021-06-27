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
    *opwidth = (*opwidth >> 1) << 1;
    *opheight = (*opheight >> 1) << 1;
    mainw->pwidth = *opwidth;
    mainw->pheight = *opheight;
    return;
  }

  if (!mainw->fs) {
    // embedded player
    *opwidth = rwidth = lives_widget_get_allocation_width(mainw->play_image);// - H_RESIZE_ADJUST;
    *opheight = rheight = lives_widget_get_allocation_height(mainw->play_image);// - V_RESIZE_ADJUST;
  } else {
    // try to get exact inner size of the main window
    lives_window_get_inner_size(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), opwidth, opheight);
    if (prefs->show_tool) *opheight -= lives_widget_get_allocation_height(mainw->btoolbar);
  }

align:
  *opwidth = (*opwidth >> 2) << 2;
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


LIVES_GLOBAL_INLINE void init_track_decoders(void) {
  int i;
  for (i = 0; i < MAX_TRACKS; i++) {
    mainw->track_decoders[i] = NULL;
    mainw->old_active_track_list[i] = mainw->active_track_list[i] = 0;
  }
  for (i = 0; i < MAX_FILES; i++) mainw->ext_src_used[i] = FALSE;
}

LIVES_GLOBAL_INLINE void free_track_decoders(void) {
  for (int i = 0; i < MAX_TRACKS; i++) {
    if (mainw->track_decoders[i] &&
        (mainw->active_track_list[i] <= 0 || mainw->track_decoders[i] != mainw->files[mainw->active_track_list[i]]->ext_src))
      clip_decoder_free(mainw->track_decoders[i]);
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


#define USEC_WAIT_FOR_SYNC 5000000000

static boolean avsync_check(void) {
  int count = USEC_WAIT_FOR_SYNC / 10, rc = 0;
  struct timespec ts;

#ifdef VALGRIND_ON
  count *= 10;
#else
  if (mainw->debug) count *= 10;
#endif

  if (mainw->foreign || !LIVES_IS_PLAYING || prefs->audio_src == AUDIO_SRC_EXT || prefs->force_system_clock
      || (mainw->event_list && !(mainw->record || mainw->record_paused)) || prefs->audio_player == AUD_PLAYER_NONE
      || !is_realtime_aplayer(prefs->audio_player) || cfile->play_paused) {
    mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    return TRUE;
  }

  if (!mainw->video_seek_ready) {
#ifdef ENABLE_JACK
    if (!mainw->foreign && mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK) {
      /// try to improve sync by delaying audio pb start
      if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
                         && !mainw->record_paused && mainw->jackd->is_paused)) {
        mainw->video_seek_ready = TRUE;
        if (!mainw->switch_during_pb) mainw->force_show = TRUE;
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
        if (!mainw->switch_during_pb) mainw->force_show = TRUE;
        return TRUE;
      }
    }
#endif
  }

  clock_gettime(CLOCK_REALTIME, &ts);
  pthread_mutex_lock(&mainw->avseek_mutex);
  mainw->video_seek_ready = TRUE;
  while (!mainw->audio_seek_ready && --count) {
    ts.tv_nsec += 10000;   // def. 10 usec (* 50000 = 0.5 sec)
    if (ts.tv_nsec >= ONE_BILLION) {
      ts.tv_sec++;
      ts.tv_nsec -= ONE_BILLION;
    }
    sched_yield();
    rc = pthread_cond_timedwait(&mainw->avseek_cond, &mainw->avseek_mutex, &ts);
    mainw->video_seek_ready = TRUE;
  }
  if (!mainw->audio_seek_ready && rc == ETIMEDOUT) {
    pthread_mutex_unlock(&mainw->avseek_mutex);
    mainw->cancelled = handle_audio_timeout();
    return FALSE;
  }
  pthread_mutex_unlock(&mainw->avseek_mutex);
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
  double scrap_file_size = -1;

  ticks_t audio_timed_out = 1;

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
  int fg_file = mainw->current_file;
  int tgamma = WEED_GAMMA_UNKNOWN;
  boolean was_letterboxed = FALSE;

#if defined ENABLE_JACK || defined HAVE_PULSE_AUDIO
  lives_alarm_t alarm_handle;
#endif

#define BFC_LIMIT 1000
  if (LIVES_UNLIKELY(cfile->frames == 0 && !mainw->foreign && !mainw->is_rendering)) {
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
    mainw->actual_frame = frame;
    if (!mainw->preview_rendering && (!((was_preview = mainw->preview) || mainw->is_rendering))) {
      /////////////////////////////////////////////////////////

      // normal play

      if (LIVES_UNLIKELY(mainw->nervous) && clip_can_reverse(mainw->playing_file)) {
        // nervous mode
        if ((mainw->actual_frame += (-10 + (int)(21.*rand() / (RAND_MAX + 1.0)))) > cfile->frames ||
            mainw->actual_frame < 1) mainw->actual_frame = frame;
        else {
          if (!(prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS))
            resync_audio(mainw->playing_file, (double)mainw->actual_frame);
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
      if ((mainw->record && !mainw->record_paused) || mainw->record_starting) {
        ticks_t actual_ticks;
        int fg_frame = mainw->record_frame;
        int bg_file = IS_VALID_CLIP(mainw->blend_file) && mainw->blend_file != mainw->current_file
                      ? mainw->blend_file : -1;
        int bg_frame = bg_file > 0 && bg_file != mainw->current_file ? mainw->files[bg_file]->frameno : 0;
        int numframes;
        int *clips;
        int64_t *frames;
        weed_plant_t *event_list;

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
          fg_frame = mainw->files[mainw->scrap_file]->frames + 1;
          scrap_file_size = mainw->files[mainw->scrap_file]->f_size;
          bg_file = -1;
          bg_frame = 0;
        }

        //actual_ticks = mainw->clock_ticks;//mainw->currticks;
        actual_ticks = mainw->startticks; ///< use the "thoretical" time

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

#ifdef ENABLE_JACK
          if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd &&
              (prefs->rec_opts & REC_AUDIO) && prefs->audio_src == AUDIO_SRC_INT
              && mainw->rec_aclip != mainw->ascrap_file) {
            // get current seek position
            alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);

            while ((audio_timed_out = lives_alarm_check(alarm_handle)) > 0
                   && jack_get_msgq(mainw->jackd)) {
              // wait for audio player message queue clearing
              sched_yield();
              lives_usleep(prefs->sleep_time);
            }
            lives_alarm_clear(alarm_handle);
            if (audio_timed_out == 0) {
              mainw->cancelled = handle_audio_timeout();
              goto lfi_done;
            }
            jack_get_rec_avals(mainw->jackd);
            g_print("GOT SEEK %f\n", mainw->rec_aseek);
          }
#endif
#ifdef HAVE_PULSE_AUDIO
          if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed &&
              (prefs->rec_opts & REC_AUDIO) && prefs->audio_src
              == AUDIO_SRC_INT && mainw->rec_aclip != mainw->ascrap_file) {
            // get current seek position
            alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
            while ((audio_timed_out = lives_alarm_check(alarm_handle)) > 0
                   && pulse_get_msgq(mainw->pulsed)) {
              // wait for audio player message queue clearing
              sched_yield();
              lives_usleep(prefs->sleep_time);
            }
            lives_alarm_clear(alarm_handle);
            pulse_get_rec_avals(mainw->pulsed);
          }
#endif
          mainw->record_starting = FALSE;
          if (audio_timed_out == 0) {
            mainw->cancelled = handle_audio_timeout();
            goto lfi_done;
          }
          mainw->record = TRUE;
          mainw->record_paused = FALSE;
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
        if (framecount) lives_free(framecount);
        pthread_mutex_lock(&mainw->event_list_mutex);

        /// usual function to record a frame event
        if ((event_list = append_frame_event(mainw->event_list, actual_ticks,
                                             numframes, clips, frames)) != NULL) {
          if (!mainw->event_list) mainw->event_list = event_list;

          // TODO ***: do we need to perform more checks here ???
          if (mainw->scratch != SCRATCH_NONE || scrap_file_size != -1 || (mainw->rec_aclip != -1 && (prefs->rec_opts & REC_AUDIO))) {
            weed_plant_t *event = get_last_frame_event(mainw->event_list);

            if (mainw->scratch != SCRATCH_NONE)
              weed_set_int_value(event, LIVES_LEAF_SCRATCH, (int)mainw->scratch);

            if (scrap_file_size != -1)
              weed_set_int64_value(event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET, scrap_file_size);

            if (mainw->rec_aclip != -1) {
              if (mainw->rec_aclip == mainw->ascrap_file) {
                mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
                                   (double)(mainw->files[mainw->ascrap_file]->arps
                                            * mainw->files[mainw->ascrap_file]->achans *
                                            mainw->files[mainw->ascrap_file]->asampsize >> 3);
                mainw->rec_avel = 1.;

              }
              if (!mainw->mute) {
                weed_event_t *xevent = get_prev_frame_event(event);
                if (!xevent) xevent = event;
                insert_audio_event_at(xevent, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel);
              }
              mainw->rec_aclip = -1;
            }
          }
          pthread_mutex_unlock(&mainw->event_list_mutex);

          /* TRANSLATORS: rec(ord) */
          framecount = lives_strdup_printf((tmp = _("rec %9d / %d")), mainw->actual_frame,
                                           cfile->frames > mainw->actual_frame
                                           ? cfile->frames : mainw->actual_frame);
        } else {
          pthread_mutex_unlock(&mainw->event_list_mutex);
          /* TRANSLATORS: out of memory (rec(ord)) */
          framecount = lives_strdup_printf((tmp = _("!rec %9d / %d")),
                                           mainw->actual_frame, cfile->frames);
        }
        lives_free(tmp);
        lives_free(clips);
        lives_free(frames);
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
      if (mainw->frame_layer) {
        // free the old mainw->frame_layer
        check_layer_ready(mainw->frame_layer); // ensure all threads are complete
        weed_layer_free(mainw->frame_layer);
        mainw->frame_layer = NULL;
      }

      if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
        // here if we are rendering from multitrack, previewing a recording, or applying realtime effects to a selection
        weed_timecode_t tc = mainw->cevent_tc;
        if (mainw->scrap_file != -1 && mainw->clip_index[0]
            == mainw->scrap_file && mainw->num_tracks == 1) {
          // do not apply fx, just pull frame
          mainw->frame_layer = lives_layer_new_for_frame(mainw->clip_index[0], mainw->frame_index[0]);
          weed_layer_ref(mainw->frame_layer);
          pull_frame_threaded(mainw->frame_layer, NULL, (weed_timecode_t)mainw->currticks, 0, 0);
        } else {
          int oclip, nclip, i;
          weed_plant_t **layers =
            (weed_plant_t **)lives_calloc((mainw->num_tracks + 1), sizeof(weed_plant_t *));
          // get list of active tracks from mainw->filter map
          get_active_track_list(mainw->clip_index, mainw->num_tracks, mainw->filter_map);
          for (i = 0; i < mainw->num_tracks; i++) {
            oclip = mainw->old_active_track_list[i];
            mainw->ext_src_used[oclip] = FALSE;
            if (oclip > 0 && oclip == (nclip = mainw->active_track_list[i])) {
              // check if ext_src survives old->new
              if (mainw->track_decoders[i] == mainw->files[oclip]->ext_src)
                mainw->ext_src_used[oclip] = TRUE;
            }
          }

          for (i = 0; i < mainw->num_tracks; i++) {
            layers[i] = lives_layer_new_for_frame(mainw->clip_index[i], mainw->frame_index[i]);
            weed_layer_ref(layers[i]);
            weed_set_int_value(layers[i], WEED_LEAF_CURRENT_PALETTE, (mainw->clip_index[i] == -1 ||
                               mainw->files[mainw->clip_index[i]]->img_type ==
                               IMG_TYPE_JPEG) ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32);
            if ((oclip = mainw->old_active_track_list[i]) != (nclip = mainw->active_track_list[i])) {
              // now using threading, we want to start pulling all pixel_data for all active layers here
              // however, we may have more than one copy of the same clip -
              // in this case we want to create clones of the decoder plugin
              // this is to prevent constant seeking between different frames in the clip
              if (oclip > 0) {
                if (mainw->files[oclip]->clip_type == CLIP_TYPE_FILE) {
                  if (mainw->track_decoders[i] != (lives_decoder_t *)mainw->files[oclip]->ext_src) {
                    // remove the clone for oclip
                    clip_decoder_free(mainw->track_decoders[i]);
                  }
                  mainw->track_decoders[i] = NULL;
                }
              }

              if (nclip > 0) {
                if (mainw->files[nclip]->clip_type == CLIP_TYPE_FILE) {
                  if (!mainw->ext_src_used[nclip]) {
                    mainw->track_decoders[i] = (lives_decoder_t *)mainw->files[nclip]->ext_src;
                    mainw->ext_src_used[nclip] = TRUE;
                  } else {
                    // add new clone for nclip
                    mainw->track_decoders[i] = clone_decoder(nclip);
		    // *INDENT-OFF*
		  }}}}
	    // *INDENT-ON*

            mainw->old_active_track_list[i] = mainw->active_track_list[i];

            if (nclip > 0) {
              img_ext = get_image_ext_for_type(mainw->files[nclip]->img_type);
              // set alt src in layer
              weed_set_voidptr_value(layers[i], WEED_LEAF_HOST_DECODER,
                                     (void *)mainw->track_decoders[i]);
              //pull_frame_threaded(layers[i], img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
            } else {
              weed_layer_pixel_data_free(layers[i]);
            }
          }
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
      } else {
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
                if (delta <= 0 || (mainw->pred_frame < 0 && delta > 0)) {
                  check_layer_ready(mainw->frame_layer_preload);
                  if (weed_layer_get_pixel_data(mainw->frame_layer_preload)) {
                    // depending on frame value we either make a deep or shallow copy of the cache frame
                    //g_print("YAH !\n");
                    got_preload = TRUE;
                    if (mainw->pred_frame < 0) {
                      // -ve value...make a deep copy, e.g we got the frame too early
                      // and we may need to reshow it several times
                      if (mainw->frame_layer) {
                        check_layer_ready(mainw->frame_layer);
                        weed_layer_free(mainw->frame_layer);
                      }
                      mainw->frame_layer = weed_layer_copy(NULL, mainw->frame_layer_preload);
                      weed_layer_ref(mainw->frame_layer);
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
                if (delta > 0) g_print("    waiting...\n");
              }
              if (!got_preload) {
                // if we didn't have a preloaded frame, we kick off a thread here to load it
                pull_frame_threaded(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
                //pull_frame(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks);
              }
              if ((mainw->rte || (mainw->is_rendering && !mainw->event_list))
                  && (mainw->current_file != mainw->scrap_file || mainw->multitrack)) {
                /// will set mainw->blend_layer
                get_blend_layer((weed_timecode_t)mainw->currticks);
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*

        if (prefs->dev_show_timing)
          g_printerr("pull_frame done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        if ((!cfile->next_event && mainw->is_rendering && !mainw->switch_during_pb &&
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
      // when playing backend previews, keep going until we run out of frames or the user cancels
    } while (!mainw->frame_layer && mainw->cancelled == CANCEL_NONE
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
      mainw->video_seek_ready = TRUE;
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

      if (!avsync_check()) goto lfi_done;

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
        g_printerr("rend fr done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
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
      mainw->video_seek_ready = TRUE;
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
        if (!needs_lb  || was_letterboxed) {
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

      if (!avsync_check()) goto lfi_done;

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
    mainw->video_seek_ready = TRUE;
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

    if (!avsync_check()) goto lfi_done;

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
#if 0
}
#endif
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
  mainw->cadjticks = mainw->adjticks = mainw->syncticks = 0;
  lastt = LIVES_TIME_SOURCE_NONE;
  delta = 0;
}


ticks_t lives_get_current_playback_ticks(int64_t origsecs, int64_t orignsecs, lives_time_source_t *time_source) {
  // get the time using a variety of methods
  // time_source may be NULL or LIVES_TIME_SOURCE_NONE to set auto
  // or another value to force it (EXTERNAL cannot be forced)
  lives_time_source_t *tsource, xtsource = LIVES_TIME_SOURCE_NONE;
  ticks_t clock_ticks, current = -1;
  static ticks_t lclock_ticks, interticks;

  if (time_source) tsource = time_source;
  else tsource = &xtsource;

  clock_ticks = lives_get_relative_ticks(origsecs, orignsecs);
  mainw->clock_ticks = clock_ticks;

  if (*tsource == LIVES_TIME_SOURCE_EXTERNAL) *tsource = LIVES_TIME_SOURCE_NONE;

  if (mainw->foreign || prefs->force_system_clock || (prefs->vj_mode && (prefs->audio_src == AUDIO_SRC_EXT))) {
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

  if (is_realtime_aplayer(prefs->audio_player) && (*tsource == LIVES_TIME_SOURCE_NONE ||
      *tsource == LIVES_TIME_SOURCE_SOUNDCARD)) {
    if ((!mainw->is_rendering || (mainw->multitrack && !cfile->opening && !mainw->multitrack->is_rendering)) &&
        (!(mainw->fixed_fpsd > 0. || (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)))) {
      // get time from soundcard
      // this is done so as to synch video stream with the audio
      // we do this in two cases:
      // - for internal audio, playing back a clip with audio (writing)
      // - or when audio source is set to external (reading) and we are recording, no internal audio generator is running

      // we ignore this if we are running with a playback plugin which requires a fixed framerate (e.g a streaming plugin)
      // in that case we will adjust the audio rate to fit the system clock

      // or if we are rendering

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
            IS_VALID_CLIP(mainw->pulsed->playing_file) && mainw->files[mainw->pulsed->playing_file]->achans > 0) ||
           (prefs->audio_src == AUDIO_SRC_EXT && mainw->pulsed_read && mainw->pulsed_read->in_use))) {
        *tsource = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          current = lives_pulse_get_time(mainw->pulsed_read);
        else current = lives_pulse_get_time(mainw->pulsed);
      }
#endif
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
  /// it can be helpful to imagine a virtual clock which is at current time:
  /// clock time - cadjticks = virtual time = other time + adjticks
  /// cadjticks and adjticks are only set when we switch from one source to another, i.e the virtual clock will run @ different rates
  /// depending on the source. This is fine as it enables sync with the clock source, provided the time doesn't jump when moving
  /// from one source to another.
  /// when the source changes we then alter either cadjticks or adjticks so that the initial timing matches
  /// e.g when switching to clock source, cadjticks and adjticks will have diverged. So we want to set new cadjtick s.t:
  /// clock ticks - cadjticks == source ticks + adjticks. i.e cadjticks = clock ticks - (source ticks + adjticks).
  /// we use the delta calculated the last time, since the other source may longer be available.
  /// this should not be a concern since this function is called very frequently
  /// recalling cadjticks_new = clock_ticks - (source_ticks + adjticks), and substituting for delta we get:
  // cadjticks_new = clock_ticks - (source_ticks + adjticks) = delta + cadjticks_old
  /// conversely, when switching from clock to source, adjticks_new = clock_ticks - cadjticks - source_ticks
  /// again, this just delta + adjticks; in this case we can use current delta since it is assumed that the system clock is always available

  /// this scheme does, however introduce a small problem, which is that when the sources are switched, we assume that the
  /// time on both clocks is equivalent. This can lead to a problem when switching clips, since temporarily we switch to system
  /// time and then back to soundcard. However, this can cause some updates to the timer to be missed, i.e the audio is playing but the
  /// samples are not counted, however we cannot simply add these to the soundcard timer, as they will be lost due to the resync.
  /// hence we need mainw->syncticks --> a global adjustment which is independent of the clock source. This is similar to
  /// mainw->deltaticks for the player, however, deltaticks is a temporary impulse, whereas syncticks is a permanent adjustment.

  if (*tsource == LIVES_TIME_SOURCE_SYSTEM)  {
    if (lastt != LIVES_TIME_SOURCE_SYSTEM && lastt != LIVES_TIME_SOURCE_NONE) {
      // current + adjt == clock_ticks - cadj /// interticks == lcurrent + adj
      // current - ds + adjt == clock_ticks - dc - cadj /// interticks == lcurrent + adj

      // cadj = clock_ticks - interticks + (current - lcurrent) - since we may not have current
      // we have to approximate with clock_ticks - lclock_ticks
      mainw->cadjticks = clock_ticks - interticks - (clock_ticks - lclock_ticks);
    }
    interticks = clock_ticks - mainw->cadjticks;
  } else {
    if (lastt == LIVES_TIME_SOURCE_SYSTEM) {
      // current - ds + adjt == clock_ticks - dc - cadj /// iinterticks == lclock_ticks - cadj ///
      mainw->adjticks = interticks - current + (clock_ticks - lclock_ticks);
    }
    interticks = current + mainw->adjticks;
  }

  /* if (lastt != *tsource) { */
  /*   g_print("aft t1 = %ld, t2 = %ld cadj =%ld, adj = %ld del =%ld %ld %ld\n", clock_ticks, current, mainw->cadjticks, */
  /*           mainw->adjticks, delta, clock_ticks + mainw->cadjticks, current + mainw->adjticks); */
  /* } */
  lclock_ticks = clock_ticks;
  lastt = *tsource;
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


frames_t calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc) {
  // returns a frame number (floor) using sfile->last_frameno and ntc-otc
  // takes into account looping modes

  // the range is first_frame -> last_frame

  // which is generally 1 -> sfile->frames, unless we are playing a selection

  // in case the frame is out of range and playing, returns 0 and sets mainw->cancelled

  // ntc is adjusted backwards to timecode of the new frame

  // the basic operation is quite simple, given the time difference between the last frame and
  // now, we calculate the new frame from the current fps and then ensure it is in the range
  // first_frame -> last_frame

  // Complications arise because we have ping-pong loop mode where the play direction
  // alternates - here we need to determine how many times we have reached the start or end
  // play point. This is similar to the winding number in topological calculations.

  // caller should check return value of ntc, and if it differs from otc, show the frame

  // note we also calculate the audio "frame" and position for realtime audio players
  // this is done so we can check here if audio limits stopped playback

  static boolean norecurse = FALSE;
  static ticks_t last_ntc = 0;

  ticks_t ddtc = *ntc - last_ntc;
  ticks_t dtc = *ntc - otc;
  int64_t first_frame, last_frame, selrange;
  lives_clip_t *sfile = mainw->files[fileno];
  double fps;
  lives_direction_t dir;
  frames_t cframe, nframe, fdelta;
  int nloops;

  if (!sfile) return 0;

  cframe = sfile->last_frameno;
  if (norecurse) return cframe;

  if (sfile->frames == 0 && !mainw->foreign) {
    if (fileno == mainw->playing_file) mainw->scratch = SCRATCH_NONE;
    return 0;
  }

  fps = sfile->pb_fps;
  if (!LIVES_IS_PLAYING || (fps < 0.001 && fps > -0.001 && mainw->scratch != SCRATCH_NONE)) fps = sfile->fps;

  if (fps < 0.001 && fps > -0.001) {
    *ntc = otc;
    if (fileno == mainw->playing_file) {
      mainw->scratch = SCRATCH_NONE;
      if (prefs->audio_src == AUDIO_SRC_INT) calc_aframeno(fileno);
    }
    return cframe;
  }

  // dtc is delta ticks (last frame time - current time), quantise this to the frame rate and round down
  dtc = q_gint64_floor(dtc, fps);

  // ntc is the time when the next frame should be / have been played, or if dtc is zero we just set it to otc - the last frame time
  *ntc = otc + dtc;

  // nframe is our new frame; convert dtc to seconds, and multiply by the frame rate, then add or subtract from current frame number
  // the small constant is just to account for rounding errors
  if (fps >= 0.)
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps + .5);
  else
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps - .5);

  if (nframe != cframe) {
    if (!IS_NORMAL_CLIP(fileno)) {
      return 1;
    }
    if (mainw->foreign) return sfile->frameno + 1;
  }

  if (fileno == mainw->playing_file) {
    /// if we are scratching we do the following:
    /// the time since the last call is considered to have happened at an increased fps (fwd or back)
    /// we recalculate the frame at ntc as if we were at the faster framerate.

    if (mainw->scratch == SCRATCH_FWD || mainw->scratch == SCRATCH_BACK
        || mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) {
      if (mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) ddtc *= 4;
      if (mainw->scratch == SCRATCH_BACK || mainw->scratch == SCRATCH_BACK_EXTRA) {
        mainw->deltaticks -= ddtc * KEY_RPT_INTERVAL * prefs->scratchback_amount
                             * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
      } else mainw->deltaticks += ddtc * KEY_RPT_INTERVAL * prefs->scratchback_amount
                                    * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
      // dtc is delta ticks, quantise this to the frame rate and round down
      mainw->deltaticks = q_gint64_floor(mainw->deltaticks, fps * 4);
    }

    if (nframe != cframe) {
      int delval = (ticks_t)((double)mainw->deltaticks / TICKS_PER_SECOND_DBL * fps + .5);
      if (delval <= -1 || delval >= 1) {
        /// the frame number changed, but we will recalculate the value using mainw->deltaticks
        frames64_t xnframe = cframe + (int64_t)delval;
        frames64_t dframes = xnframe - nframe;

        if (xnframe != nframe) {
          nframe = xnframe;
          /// retain the fractional part for next time
          mainw->deltaticks -= (ticks_t)((double)delval / fps * TICKS_PER_SECOND_DBL);
          if (nframe != cframe) {
            sfile->last_frameno += dframes;
            if (fps < 0. && mainw->scratch == SCRATCH_FWD) sfile->last_frameno--;
            if (fps > 0. &&  mainw->scratch == SCRATCH_BACK) sfile->last_frameno++;
            mainw->scratch = SCRATCH_JUMP_NORESYNC;
          } else mainw->scratch = SCRATCH_NONE;
        }
      }
    }
    last_ntc = *ntc;
  }

  last_frame = sfile->frames;
  first_frame = 1;

  if (fileno == mainw->playing_file) {
    // calculate audio "frame" from the number of samples played
    if (prefs->audio_src == AUDIO_SRC_INT) calc_aframeno(fileno);

    if (nframe == cframe || mainw->foreign) {
      if (mainw->scratch == SCRATCH_JUMP) {
        if (!mainw->foreign && fileno == mainw->playing_file &&
            (prefs->audio_src == AUDIO_SRC_INT) &&
            !(prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS)) {
          // check if audio stopped playback. The audio player will also be checking this, BUT: we have to check here too
          // before doing any resync, otherwise the video can loop and if the audio is then resynced it may never reach the end
          if (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
            if (!check_for_audio_stop(fileno, first_frame + 1, last_frame - 1)) {
              mainw->cancelled = CANCEL_AUD_END;
              mainw->scratch = SCRATCH_NONE;
              return 0;
            }
          }
          if (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED))
            resync_audio(mainw->playing_file, (double)nframe);
        }
        mainw->scratch = SCRATCH_JUMP_NORESYNC;
      }
      return nframe;
    }

    if (!mainw->clip_switched && (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
      last_frame = mainw->playing_sel ? sfile->end : mainw->play_end;
      if (last_frame > sfile->frames) last_frame = sfile->frames;
      first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
      if (first_frame > sfile->frames) first_frame = sfile->frames;
    }

    if (sfile->frames > 1 && prefs->noframedrop && (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
      // if noframedrop is set, we may not skip any frames
      // - the usual situation is that we are allowed to drop late frames
      // in this mode we may be forced to play at a reduced framerate
      if (nframe > cframe + 1) {
        // update this so the player can calculate 'dropped' frames correctly
        cfile->last_frameno -= (nframe - cframe - 1);
        nframe = cframe + 1;
      } else if (nframe < cframe - 1) {
        cfile->last_frameno += (cframe - 1 - nframe);
        nframe = cframe - 1;
      }
    }
  }

  while (IS_NORMAL_CLIP(fileno) && (nframe < first_frame || nframe > last_frame)) {
    // get our frame back to within bounds:
    // define even parity (0) as backwards, odd parity (1) as forwards
    // we subtract the lower bound from the new frame, and divide the result by the selection length,
    // rounding towards zero. (nloops)
    // in ping mode this is then added to the original direction, and the resulting parity gives the new direction
    // the remainder after doing the division is then either added to the selection start (forwards)
    /// or subtracted from the selection end (backwards) [if we started backwards then the boundary crossing will be with the
    // lower bound and nloops and the remainder will be negative, so we subtract and add the negatvie value instead]
    // we must also set

    if (fileno == mainw->playing_file) {
      // check if video stopped playback
      if (mainw->whentostop == STOP_ON_VID_END && !mainw->loop_cont) {
        mainw->cancelled = CANCEL_VID_END;
        mainw->scratch = SCRATCH_NONE;
        return 0;
      }
      // we need to set this for later in the function
      mainw->scratch = SCRATCH_JUMP;
    }

    if (first_frame == last_frame) {
      nframe = first_frame;
      break;
    }

    if (fps < 0.) dir = LIVES_DIRECTION_BACKWARD; // 0, and even parity
    else dir = LIVES_DIRECTION_FORWARD; // 1, and odd parity

    if (dir == LIVES_DIRECTION_FORWARD && nframe < first_frame) {
      // if FWD and before lower bound, just jump to lower bound
      nframe = first_frame;
      sfile->last_frameno = first_frame;
      break;
    }

    if (dir == LIVES_DIRECTION_BACKWARD && nframe  > last_frame) {
      // if BACK and after upper bound, just jump to upper bound
      nframe = last_frame;
      sfile->last_frameno = last_frame;
      break;
    }

    fdelta = ABS(sfile->frameno - sfile->last_frameno);
    nframe -= first_frame;
    selrange = (1 + last_frame - first_frame);
    if (nframe < 0) nframe = -nframe;
    nloops = nframe / selrange;
    if (mainw->ping_pong && (dir == LIVES_DIRECTION_BACKWARD || (clip_can_reverse(fileno)))) {
      dir += nloops + dir + 1;
      dir = LIVES_DIRECTION_PAR(dir);
      if (dir == LIVES_DIRECTION_BACKWARD && !clip_can_reverse(fileno))
        dir = LIVES_DIRECTION_FORWARD;
    }

    nframe -= nloops * selrange;

    if (dir == LIVES_DIRECTION_FORWARD) {
      nframe += first_frame;
      sfile->last_frameno = nframe - fdelta;
      if (fps < 0.) {
        // backwards -> forwards
        if (fileno == mainw->playing_file) {
          /// must set norecurse, otherwise we can end up in an infinite loop since dirchange_callback calls this function
          norecurse = TRUE;
          dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
                             LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          norecurse = FALSE;
        } else sfile->pb_fps = -sfile->pb_fps;
      }
    }

    else {
      nframe = last_frame - nframe;
      sfile->last_frameno = nframe + fdelta;
      if (fps > 0.) {
        // forwards -> backwards
        if (fileno == mainw->playing_file) {
          norecurse = TRUE;
          dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
                             LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          norecurse = FALSE;
        } else sfile->pb_fps = -sfile->pb_fps;
      }
    }
    break;
  }

  if (mainw->scratch == SCRATCH_JUMP) {
    if (!mainw->foreign && fileno == mainw->playing_file &&
        (prefs->audio_src == AUDIO_SRC_INT) &&
        !(prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS)) {
      if (mainw->whentostop == STOP_ON_AUD_END && !mainw->loop_cont) {
        // check if audio stopped playback. The audio player will also be checking this, BUT: we have to check here too
        // before doing any resync, otherwise the video can loop and if the audio is then resynced it may never reach the end
        calc_aframeno(fileno);
        if (!check_for_audio_stop(fileno, first_frame + 1, last_frame - 1)) {
          mainw->cancelled = CANCEL_AUD_END;
          mainw->scratch = SCRATCH_NONE;
          return 0;
        }
      }
      resync_audio(mainw->playing_file, (double)nframe);
    } else sfile->last_frameno = nframe;
  }

  if (fileno == mainw->playing_file) {
    if (mainw->scratch != SCRATCH_NONE) {
      mainw->scratch = SCRATCH_JUMP_NORESYNC;
    }
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
static frames_t getahead = -1, test_getahead = -1, bungle_frames;

static boolean recalc_bungle_frames = 0;
static boolean cleanup_preload;
static boolean init_timers = TRUE;
static boolean drop_off = FALSE;
static int dropped;

static lives_time_source_t last_time_source;

static frames_t cache_hits = 0, cache_misses = 0;
static double jitter = 0.;

static weed_timecode_t event_start = 0;

void ready_player_one(weed_timecode_t estart) {
  event_start = 0;
  cleanup_preload = FALSE;
  cache_hits = cache_misses = 0;
  dropped = 0;
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
#define ADJUST_AUDIO_RATE
#endif

int process_one(boolean visible) {
  // INTERNAL PLAYER
  static frames_t last_req_frame = 0;
  static frames_t real_requested_frame = 0;
#ifdef HAVE_PULSE_AUDIO
  static int64_t last_seek_pos = 0;
#endif
  //static ticks_t last_ana_ticks;
  volatile float *cpuload;
  //double cpu_pressure;
  lives_clip_t *sfile = cfile;
  _vid_playback_plugin *old_vpp;
  ticks_t new_ticks;
  lives_time_source_t time_source = LIVES_TIME_SOURCE_NONE;
  frames_t requested_frame = 0;
  boolean show_frame = FALSE;
  boolean did_switch = FALSE;
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
  ticks_t audio_ticks = 0;
#endif
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

  last_time_source = time_source;
  time_source = LIVES_TIME_SOURCE_NONE;

  mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
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
    real_requested_frame = 0;
    get_proc_loads(FALSE);
    reset_on_tsource_change = TRUE;
  }

  mainw->audio_stretch = 1.0;

  if (AUD_SRC_REALTIME && (!mainw->video_seek_ready || !mainw->audio_seek_ready)) {
    mainw->video_seek_ready = TRUE;
    avsync_check();
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
      if (audio_ticks == -1) {
        if (mainw->cancelled == CANCEL_NONE) {
          if (IS_VALID_CLIP(mainw->playing_file)  && !sfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
          else mainw->cancelled = CANCEL_AUDIO_ERROR;
          return mainw->cancelled;
        }
      }
      if ((audio_stretch = (double)(audio_ticks - mainw->offsetticks) / (double)(mainw->currticks - mainw->offsetticks)) < 2. &&
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

  if (mainw->new_clip != -1) {
    mainw->noswitch = FALSE;
    mainw->deltaticks = 0;

    if (IS_VALID_CLIP(mainw->playing_file)) {
      /* if (sfile->arate) { */
      /*   g_print("HIB %d %d %d %d %ld %f %f %ld %ld %d %f\n", sfile->frameno, last_req_frame, mainw->playing_file, */
      /*   	aplay_file, sfile->aseek_pos, */
      /*   	sfile->fps * lives_jack_get_pos(mainw->jackd) + 1., (double)sfile->aseek_pos */
      /*   	/ (double)sfile->arps / 4. * sfile->fps + 1., */
      /*   	mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
      /*   sfile->frameno = sfile->last_frameno = last_req_frame + sig(sfile->pb_fps); */
      /* } */
      /* if (sfile->arate) { */
      /*   g_print("HIB %d %d %d %d %ld %f %f %ld %ld %d %f\n", sfile->frameno, last_req_frame, mainw->playing_file, */
      /*   	aplay_file, sfile->aseek_pos, */
      /*   	sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., (double)sfile->aseek_pos */
      /*   	/ (double)sfile->arps / 4. * sfile->fps + 1., */
      /*   	mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
      /*   sfile->frameno = sfile->last_frameno = last_req_frame + sig(sfile->pb_fps); */
      /* } */
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
      if (mainw->jackd) update_aud_pos(mainw->jackd, 0);
      mainw->xrun_active = FALSE;
    }
#endif

    if (AV_CLIPS_EQUAL) {
      aplay_file = get_aplay_clipno();
      if (IS_VALID_CLIP(aplay_file)) {
        lives_clip_t *sfile = mainw->files[aplay_file];
        int qnt = sfile->achans * (sfile->asampsize >> 3);
        sfile->aseek_pos -= (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL
                            * sfile->arps * qnt;
      }
    }

    do_quick_switch(mainw->new_clip);
    did_switch = TRUE;

    if (!IS_VALID_CLIP(mainw->playing_file)) {
      if (IS_VALID_CLIP(mainw->new_clip)) goto switch_point;
      mainw->cancelled = CANCEL_INTERNAL_ERROR;
      cancel_process(FALSE);
      return ONE_MILLION + mainw->cancelled;
    }

    sfile = mainw->files[mainw->playing_file];
    sfile->last_frameno = sfile->frameno;

    real_requested_frame = 0;
    dropped = 0;

    /* if (sfile->arate) */
    /*   g_print("HIB2 %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", mainw->actual_frame, sfile->frameno, last_req_frame, */
    /*   	      mainw->playing_file, aplay_file, sfile->aseek_pos, */
    /*   	      sfile->fps * lives_jack_get_pos(mainw->jackd) + 1., */
    /*   	      (double)sfile->aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
    /*   	      mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */

    /* if (sfile->arate) */
    /*   g_print("HIB2 %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", mainw->actual_frame, sfile->frameno, last_req_frame, */
    /*   	      mainw->playing_file, aplay_file, sfile->aseek_pos, */
    /*   	      sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., */
    /*   	      (double)sfile->aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
    /*   	      mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */

    cache_hits = cache_misses = 0;
    mainw->force_show = TRUE;
    mainw->actual_frame = mainw->files[mainw->new_clip]->frameno;
    mainw->new_clip = -1;
    getahead = test_getahead = -1;
    if (prefs->pbq_adaptive) reset_effort();
    // TODO: add a few to bungle_frames in case of decoder unchilling

    /// switch compensation allows us to give a brief impulse to the audio when switching
    // this may be adjusted for accuracy | a value > 1.0 will slow audio down on switch
#define SWITCH_COMPENSATION 1.0

    mainw->audio_stretch = SWITCH_COMPENSATION;
    scratch = SCRATCH_JUMP_NORESYNC;
    drop_off = TRUE;
    last_req_frame = sfile->frameno - 1;
    /// playing file should == current_file, but just in case store separate values.
    old_current_file = mainw->current_file;
    old_playing_file = mainw->playing_file;

    if (sfile->arate)
      g_print("seek vals: vid %d %ld = %f %d %f\n", sfile->frameno, sfile->aseek_pos,
              (double)sfile->aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1.,
              sfile->arate, sfile->fps);
  }

  mainw->noswitch = TRUE;

  /// end SWITCH POINT

  // playing back an event_list
  // here we need to add mainw->offsetticks, to get the correct position when playing back in multitrack
  if (!mainw->proc_ptr && cfile->next_event) {
    // playing an event_list
    if (0 && mainw->scratch != SCRATCH_NONE && mainw->multitrack && mainw->jackd_trans
        && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
        && (prefs->jack_opts & JACK_OPTS_TRANSPORT_SLAVE)
        && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) {
      // handle transport jump in multitrack : end current playback and restart it from the new position
      ticks_t transtc = q_gint64(jack_transport_get_current_ticks(mainw->jackd_trans), cfile->fps);
      mainw->multitrack->pb_start_event = get_frame_event_at(mainw->multitrack->event_list, transtc, NULL, TRUE);
      if (mainw->cancelled == CANCEL_NONE) mainw->cancelled = CANCEL_EVENT_LIST_END;
    } else {
      if (mainw->multitrack) mainw->currticks += mainw->offsetticks; // add the offset of playback start time
      if (mainw->currticks >= event_start) {
        // see if we are playing a selection and reached the end
        if (mainw->multitrack && mainw->multitrack->playing_sel &&
            get_event_timecode(cfile->next_event) / TICKS_PER_SECOND_DBL >=
            mainw->multitrack->region_end) mainw->cancelled = CANCEL_EVENT_LIST_END;
        else {
          mainw->noswitch = FALSE;
          cfile->next_event = process_events(cfile->next_event, FALSE, mainw->currticks);
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
#define DROPFRAME_TRIGGER 4
#define JUMPFRAME_TRIGGER 99999999 // we should retain cdata->jump_limit from the initial file open

  if (mainw->currticks - last_kbd_ticks > KEY_RPT_INTERVAL * 100000) {
    // if we have a cached key (ctrl-up, ctrl-down, ctrl-left, crtl-right) trigger it here
    // this is to avoid the keyboard repeat delay (dammit !) so we get smooth trickplay
    // BUT we need a timer sufficiently large so it isn't triggered on every loop, to produce a constant repeat rate
    // but not so large so it doesn't get triggered enough
    if (last_kbd_ticks > 0) handle_cached_keys();
    last_kbd_ticks = mainw->currticks;
  }

  if (mainw->scratch == SCRATCH_JUMP && time_source == LIVES_TIME_SOURCE_EXTERNAL) {
    sfile->frameno = sfile->last_frameno = calc_frame_from_time(mainw->playing_file,
                                           mainw->currticks / TICKS_PER_SECOND_DBL);
    mainw->startticks = mainw->currticks;
  }

  new_ticks = mainw->currticks;

  if (mainw->scratch != SCRATCH_JUMP)
    if (new_ticks < mainw->startticks) new_ticks = mainw->startticks;

  show_frame = FALSE;
  requested_frame = sfile->frameno;

  if (sfile->pb_fps != 0.) {
    // calc_new_playback_postion returns a frame request based on the player mode and the time delta
    //
    // mainw->startticks is the timecode of the last frame shown
    // new_ticks is the (adjusted) current time
    // sfile->last_frameno, sfile->pb_fps (if playing) or sfile->fps (if not) are also used in the calculation
    // as well as selection bounds and loop mode settings (if appropriate)
    //
    // on return, new_ticks is set to either mainw->starticks or the timecode of the next frame to show
    // which will be <= the current time
    // and requested_frame is set to the frame to show. By default this is the frame we show, but we may vary
    // this depending on the cached frame
    frames_t oframe = sfile->last_frameno;

    if (mainw->scratch == SCRATCH_NONE && scratch == SCRATCH_NONE
        && real_requested_frame > 0) sfile->last_frameno = real_requested_frame;
    //g_print("PRE: %ld %ld  %d %f\n", mainw->startticks, new_ticks, sfile->last_frameno, (new_ticks - mainw->startticks) / TICKS_PER_SECOND_DBL * sfile->pb_fps);
    real_requested_frame = requested_frame
                           = calc_new_playback_position(mainw->playing_file, mainw->startticks, &new_ticks);
    //g_print("POST: %ld %ld %d\n", mainw->startticks, new_ticks, requested_frame);
    //mainw->startticks = new_ticks;
    sfile->last_frameno = oframe;
    if (mainw->foreign) {
      if (requested_frame > sfile->frameno) {
        load_frame_image(sfile->frameno++);
      }
      lives_widget_context_update();
      if (mainw->cancelled != CANCEL_NONE) return mainw->cancelled;
      return 0;
    }
    if (requested_frame < 1 || requested_frame > sfile->frames) {
      requested_frame = sfile->frameno;
    } else sfile->frameno = requested_frame;

    if (mainw->scratch != SCRATCH_NONE) scratch  = mainw->scratch;
    mainw->scratch = SCRATCH_NONE;
#define SHOW_CACHE_PREDICTIONS
#ifdef ENABLE_PRECACHE
    if (scratch != SCRATCH_NONE) {
      getahead = test_getahead = -1;
      cleanup_preload = TRUE;
      mainw->pred_frame = -1;
      // fix for a/v sync
      //if (!did_switch) sfile->last_play_sequence = mainw->play_sequence;
    }
#endif

    if (new_ticks != mainw->startticks && new_ticks != mainw->last_startticks
        && (requested_frame != last_req_frame || sfile->frames == 1
            || (mainw->playing_sel && sfile->start == sfile->end))) {
      //g_print("%ld %ld %ld %d %d %d\n", mainw->currticks, mainw->startticks, new_ticks,
      //sfile->last_frameno, requested_frame, last_req_frame);
      if (mainw->fixed_fpsd <= 0. && (!mainw->vpp || mainw->vpp->fixed_fpsd <= 0. || !mainw->ext_playback)) {
        show_frame = TRUE;
      }
      if (prefs->show_dev_opts) jitter = (double)(mainw->currticks - new_ticks) / TICKS_PER_SECOND_DBL;
#ifdef ENABLE_PRECACHE
      if (test_getahead > 0) {
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
          int dir = sig(sfile->pb_fps);
          delta = (test_getahead - mainw->actual_frame) * dir;
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
        }
        recalc_bungle_frames = FALSE;
        test_getahead = -1;
      }
#endif

#ifdef USE_GDK_FRAME_CLOCK
      if (display_ready) {
        show_frame = TRUE;
        /// not used
        display_ready = FALSE;
      }
#endif
    }
  }

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
      show_frame = TRUE;
    } else {
      if (mainw->fixed_fpsd > 0. || (mainw->vpp  && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)) {
        ticks_t dticks;
        dticks = (mainw->clock_ticks - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL;
        if ((mainw->fixed_fpsd > 0. && (dticks >= 1. / mainw->fixed_fpsd)) ||
            (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback &&
             dticks >= 1. / mainw->vpp->fixed_fpsd)) {
          show_frame = TRUE;
        }
      }
    }

    if (show_frame) {
      // time to show a new frame
      get_proc_loads(FALSE);
      last_spare_cycles = spare_cycles;
      dropped = 0;
      if (LIVES_IS_PLAYING && (!mainw->event_list  || mainw->record || mainw->preview_rendering)) {
        /// calculate dropped frames, this is ABS(frame - last_frame) - 1
        if (scratch != SCRATCH_NONE || getahead > -1 || drop_off) dropped = 0;
        else {
          if (mainw->frame_layer_preload && !cleanup_preload && mainw->pred_clip == mainw->playing_file
              && mainw->pred_frame != 0 && (labs(mainw->pred_frame) - mainw->actual_frame)
              * sig(sfile->pb_fps) > 0
              && is_layer_ready(mainw->frame_layer_preload))
            dropped = ABS(requested_frame - mainw->pred_frame) - 1;
          else
            dropped = ABS(requested_frame - mainw->actual_frame) - 1;
          if (dropped < 0) dropped = 0;
        }
#ifdef ENABLE_PRECACHE
        if (getahead > -1) {
          if (mainw->pred_frame == -getahead) {
            if (is_layer_ready(mainw->frame_layer_preload))
              sfile->frameno = mainw->pred_frame = -mainw->pred_frame;
          } else {
            if (mainw->pred_frame == getahead) {
              if ((sfile->pb_fps > 0. && sfile->frameno >= getahead)
                  || (sfile->pb_fps < 0. && sfile->frameno <= getahead)) {
                if (sfile->frameno != getahead) {
                  getahead = -1;
                  mainw->pred_frame = 0;
                  cleanup_preload = TRUE;
                  if (sfile->pb_fps > 0.)  /// not sure why yet but this doesn't work for rev. pb
                    sfile->last_frameno = requested_frame;
                  drop_off = FALSE;
                }
              } else {
                sfile->frameno = getahead;
		// *INDENT-OFF*
              }}}}
	// *INDENT-ON*
        else {
          lives_direction_t dir;
          if (sfile->clip_type == CLIP_TYPE_FILE && scratch == SCRATCH_NONE
              && is_virtual_frame(mainw->playing_file, sfile->frameno)
              && dropped > 0 && ((sfile->pb_fps < 0. && !clip_can_reverse(mainw->playing_file))
                                 || abs(sfile->frameno - sfile->last_vframe_played) >= JUMPFRAME_TRIGGER
                                 || dropped >= MIN(TEST_TRIGGER, DROPFRAME_TRIGGER))) {
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
            dir = LIVES_DIRECTION_SIG(sfile->pb_fps);
            if (bungle_frames <= -dir || bungle_frames == 0) bungle_frames = dir;
            //bungle_frames += requested_frame - mainw->actual_frame - dir;
            test_getahead = requested_frame + bungle_frames * dir;
            if (test_getahead < 1 || test_getahead > sfile->frames) test_getahead = -1;
            else {
              if (prefs->dev_show_caching) {
                g_print("getahead jumping to %d\n", test_getahead);
              }
              recalc_bungle_frames = TRUE;
              if (dropped > 0 && delta <= 0 && ((sfile->pb_fps < 0. && (!clip_can_reverse(mainw->current_file)))
                                                || (abs(sfile->frameno - sfile->last_vframe_played) >= JUMPFRAME_TRIGGER)
                                                || dropped >= DROPFRAME_TRIGGER)) {
                lives_decoder_t *dplug = NULL;
                lives_decoder_sys_t *dpsys = NULL;
                int64_t pred_frame;

                getahead = test_getahead;
                //bungle_frames -= getahead;
                pred_frame = getahead;
                //pred_frame = requested_frame;

                if (pred_frame >= 1 && pred_frame < sfile->frames) {
                  if (sfile->clip_type == CLIP_TYPE_FILE) {
                    dplug = (lives_decoder_t *)sfile->ext_src;
                    if (dplug) dpsys = (lives_decoder_sys_t *)dplug->dpsys;
                  }

                  if (!mainw->xrun_active && !did_switch) {
                    for (int zz = 0; zz < 6; zz++) {
                      int64_t new_pred_frame;
                      double est_time;

                      if (is_virtual_frame(mainw->playing_file, pred_frame)) {
                        if (!dpsys || !dpsys->estimate_delay) break;
                        est_time = (*dpsys->estimate_delay)(dplug->cdata, get_indexed_frame(mainw->playing_file, pred_frame));
                      } else {
                        // img timings
                        est_time = sfile->img_decode_time;
                      }

                      if (est_time <= 0.) break;

                      if (sfile->pb_fps > 0.) {
                        new_pred_frame = sfile->frameno + (frames_t)(est_time * sfile->pb_fps + .9999);
                        if (new_pred_frame > sfile->frames) new_pred_frame = pred_frame;
                      } else {
                        // this does not work for reverse playback - we compensate for delay by jumping further
                        // but for reverse play, the further back we go, the faster it may be
                        // so test if we can reach the new target quicker
                        new_pred_frame = sfile->frameno + (frames_t)(est_time * sfile->pb_fps - .9999);
                        if (new_pred_frame < 1 || new_pred_frame >= sfile->frames) new_pred_frame = pred_frame;
                        else {
                          while (1) {
                            double est_time2;
                            if (is_virtual_frame(mainw->playing_file, pred_frame)) {
                              est_time2 = (*dpsys->estimate_delay)(dplug->cdata, get_indexed_frame(mainw->playing_file, new_pred_frame));
                            } else {
                              est_time2 = sfile->img_decode_time;
                            }
                            if (est_time2 >= est_time) {
                              new_pred_frame = sfile->frameno + (frames_t)(est_time * sfile->pb_fps - .9999);
                              break;
                            }
                            new_pred_frame = sfile->frameno + (frames_t)(est_time2 * sfile->pb_fps - .9999);
                            if (new_pred_frame < 1 || new_pred_frame >= sfile->frames) {
                              new_pred_frame = sfile->frameno + (frames_t)(est_time * sfile->pb_fps - .9999);
                              break;
                            }
                            est_time = est_time2;
                          }
                        }
                      }
                      if (sfile->pb_fps > 0.) {
                        if (new_pred_frame < requested_frame) break;
                      } else {
                        if (new_pred_frame > requested_frame) break;
                      }
                      //g_print("EST %ld -> %ld\n", pred_frame, new_pred_frame);
                      if (new_pred_frame == pred_frame) break;
                      pred_frame = new_pred_frame;
                    }
                  }
                }
                getahead = pred_frame;
                if (getahead < 1) getahead = 1;
                if (getahead > sfile->frames) getahead = sfile->frames;

                //bungle_frames += getahead;
                //if (bungle_frames < 1) bungle_frames = 1;

                //g_print("xxxEST %d\n", getahead);

                if (mainw->pred_frame > 0 && (mainw->pred_frame - mainw->actual_frame) * dir > 0
                    && mainw->frame_layer_preload && is_layer_ready(mainw->frame_layer_preload))
                  sfile->frameno = mainw->pred_frame;
                else sfile->frameno = getahead;
		// *INDENT-OFF*
	      }}}
	  // *INDENT-ON*
#else
        if (1) {
#endif
        }

#ifdef HAVE_PULSE_AUDIO
        if (prefs->audio_player == AUD_PLAYER_PULSE) {
          if (getahead < 0 && new_ticks != mainw->startticks
              && (!mainw->pulsed || (mainw->pulsed->seek_pos == last_seek_pos
                                     && CLIP_HAS_AUDIO(mainw->pulsed->playing_file)))) {
            mainw->startticks = new_ticks;
            sfile->frameno = mainw->actual_frame;
          }
        }
#endif
#ifdef ENABLE_JACK
        if (prefs->audio_player == AUD_PLAYER_JACK) {
          if (getahead < 0 && new_ticks != mainw->startticks
              && (!mainw->jackd || (mainw->jackd->seek_pos == last_seek_pos
                                    && CLIP_HAS_AUDIO(mainw->jackd->playing_file)))) {
            mainw->startticks = new_ticks;
            sfile->frameno = mainw->actual_frame;
          }
        }
#endif

#ifdef ENABLE_PRECACHE
        if (mainw->pred_clip == -1) {
          /// failed load, just reset
          mainw->frame_layer_preload = NULL;
          cleanup_preload = FALSE;
        } else if (cleanup_preload) {
          if (mainw->frame_layer_preload && is_layer_ready(mainw->frame_layer_preload)) {
            check_layer_ready(mainw->frame_layer_preload);
            weed_layer_free(mainw->frame_layer_preload);
            mainw->frame_layer_preload = NULL;
            cleanup_preload = FALSE;
          }
          /* if (mainw->frame_layer_preload) { */
          /*   cleanup_preload = FALSE; */
          /* } */
        }

        if (mainw->frame_layer_preload && !cleanup_preload) {
          if (mainw->pred_clip == mainw->current_file) {
            frames_t pframe = labs(mainw->pred_frame);
            if (((sfile->pb_fps > 0. && pframe >= mainw->actual_frame &&
                  (pframe <= requested_frame || is_virtual_frame(mainw->playing_file, sfile->frameno))) ||
                 (sfile->pb_fps < 0. && pframe <= mainw->actual_frame &&
                  (pframe >= requested_frame || is_virtual_frame(mainw->playing_file, sfile->frameno))))
                && ((getahead > -1 || pframe == requested_frame
                     || is_layer_ready(mainw->frame_layer_preload)))) {
              sfile->frameno = pframe;
            }
            if (pframe == sfile->frameno) cache_hits++;
            else if (getahead == -1) {
              if ((sfile->pb_fps > 0. && pframe <= mainw->actual_frame)
                  || (sfile->pb_fps < 0. && pframe >= mainw->actual_frame)) {
                cleanup_preload = TRUE;
                if (pframe != mainw->actual_frame) {
                  if (prefs->dev_show_caching) {
                    g_print("WASTED cache frame %ld !!!! range was %d to %d or was not ready\n",
                            mainw->pred_frame, mainw->actual_frame,
                            sfile->frameno);
                    cache_misses++;
                  }
		    // *INDENT-OFF*
		  }}}}}
	  // *INDENT-ON*

        if (sfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->playing_file, sfile->frameno))
          sfile->last_vframe_played = sfile->frameno;
#endif
      }

      if (prefs->pbq_adaptive && scratch == SCRATCH_NONE) {
        if (requested_frame != last_req_frame || sfile->frames == 1) {
          if (sfile->frames == 1) {
            if (!spare_cycles) {
              //if (sfile->ext_src_type == LIVES_EXT_SRC_FILTER) {
              //weed_plant_t *inst = (weed_plant_t *)sfile->ext_src;
              double target_fps = fabs(sfile->pb_fps);//weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);
              if (target_fps) {
                mainw->inst_fps = get_inst_fps(FALSE);
                if (mainw->inst_fps < target_fps) {
                  update_effort(1 + target_fps / mainw->inst_fps, TRUE);
		    // *INDENT-OFF*
		  }}}}
	    // *INDENT-ON*
          else {
            /// update the effort calculation with dropped frames and spare_cycles
            if (dropped > 0) update_effort(abs(requested_frame - last_req_frame - 1), TRUE);
          }
          update_effort(spare_cycles + 1, FALSE);
        }
      }

      if (new_ticks > mainw->startticks) mainw->startticks = new_ticks;

      if (getahead < 0) {
        /// this is where we rebase the time for the next frame calculation
        /// if getahead >= 0 then we want to keep the base at the last "played" frame,
        //   and keep repeating getahead until we reach it
        /// but we did update last_start_ticks
        sfile->last_frameno = requested_frame;
        // set
        if (scratch != SCRATCH_NONE) {
          mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
          mainw->startticks = mainw->currticks;
        }
      }

      mainw->last_startticks = mainw->startticks;

#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("dropped = %d, %d scyc = %ld %d %d\n", dropped, mainw->effort, spare_cycles, requested_frame, sfile->frameno);
#endif
      drop_off = FALSE;
      if (mainw->force_show || ((sfile->frameno != mainw->actual_frame || (show_frame && sfile->frames == 1 && sfile->frameno == 1)
                                 || (mainw->playing_sel && sfile->start == sfile->end))
#ifdef ENABLE_PRECACHE
                                && getahead < 0)
          || (getahead > -1 && mainw->frame_layer_preload && !cleanup_preload && requested_frame != last_req_frame
#endif
             )) {
        spare_cycles = 0;
        mainw->record_frame = requested_frame;

        /// note the audio seek position at the current frame. We will use this when switching clips
        aplay_file = get_aplay_clipno();
        if (IS_VALID_CLIP(aplay_file)) {
          int qnt = mainw->files[aplay_file]->achans * (mainw->files[aplay_file]->asampsize >> 3);

#ifdef ENABLE_JACK
          if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) {
            mainw->files[aplay_file]->aseek_pos =
              (double)((off_t)((double) mainw->jackd->seek_pos / (double)mainw->files[aplay_file]->arps
                               / (mainw->files[aplay_file]->achans * mainw->files[aplay_file]->asampsize / 8
                                  + (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL)
                               * mainw->files[aplay_file]->fps + .5)) / mainw->files[aplay_file]->fps
              * mainw->files[aplay_file]->arps * qnt;

          }
#endif

#ifdef HAVE_PULSE_AUDIO
          if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
            mainw->files[aplay_file]->aseek_pos =
              (double)((off_t)((double) mainw->pulsed->seek_pos / (double)mainw->files[aplay_file]->arps
                               / (mainw->files[aplay_file]->achans * mainw->files[aplay_file]->asampsize / 8
                                  + (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL)
                               * mainw->files[aplay_file]->fps + .5)) / mainw->files[aplay_file]->fps
              * mainw->files[aplay_file]->arps * qnt;

            //g_print("ASEEK VALS %ld for %d -> %d\n", mainw->files[aplay_file]->aseek_pos, aplay_file, mainw->playing_file);

          }
#endif
        }
#if 0
        if (prefs->audio_player == AUD_PLAYER_NONE) {
          aplay_file = mainw->nullaudio->playing_file;
          if (IS_VALID_CLIP(aplay_file))
            mainw->files[aplay_file]->aseek_pos = nullaudio_get_seek_pos();
        }
#endif

        // load and display the new frame
        if (prefs->dev_show_caching) {
          /* 	    /\* g_print("playing frame %d / %d at %ld (%ld : %ld) %.2f %ld %ld\n", sfile->frameno, requested_frame, mainw->currticks, *\/ */
          /* 	    /\* 	    mainw->startticks, new_ticks, *\/ */
          /* #ifdef HAVE_PULSE_AUDIO */
          /* 	    prefs->audio_player == AUD_PLAYER_PULSE ? */
          /* 	      ((mainw->pulsed->in_use && IS_VALID_CLIP(mainw->pulsed->playing_file) */
          /* 		&& mainw->files[mainw->pulsed->playing_file]->arate != 0) */
          /* 	       ? (double)mainw->pulsed->seek_pos */
          /* 	       / (double)mainw->files[mainw->pulsed->playing_file]->arate */
          /* 	       / 4. * sfile->fps + 1. : 0. * sfile->fps + 1) : */

          /* #endif */
          /* #ifdef ENABLE_JACK */
          /* 	      prefs->audio_player == AUD_PLAYER_JACK ? */
          /* 	      ((mainw->jackd->in_use && IS_VALID_CLIP(mainw->jackd->playing_file) */
          /* 		&& mainw->files[mainw->jackd->playing_file]->arate != 0) */
          /* 	       ? (double)mainw->jackd->seek_pos */
          /* 	       / (double)mainw->files[mainw->jackd->playing_file]->arate */
          /* 	       / 4. * sfile->fps + 1. : 0. * sfile->fps + 1) : */
          /* #endif */
          /* 	      0, */
          /* #ifdef HAVE_PULSE_AUDIO */
          /* 	      prefs->audio_player == AUD_PLAYER_PULSE ? */
          /* 	      lives_get_relative_ticks(mainw->origsecs, mainw->orignsecs) : */
          /* #endif */
          /* #ifdef ENABLE_JACK */
          /* 	      prefs->audio_player == AUD_PLAYER_JACK ? */
          /* 	      lives_get_relative_ticks(mainw->origsecs, mainw->orignsecs) : */
          /* #endif */
          /* 	      0, */
          /* #ifdef HAVE_PULSE_AUDIO */
          /* 	      prefs->audio_player == AUD_PLAYER_PULSE ? */
          /* 	      mainw->pulsed->seek_pos : */
          /* #endif */
          /* #ifdef ENABLE_JACK */
          /* 	      prefs->audio_player == AUD_PLAYER_JACK ? */
          /* 	      mainw->jackd->seek_pos : */
          /* #endif */
          /* 	      0); */
          /* 	} */
          /* #ifdef HAVE_PULSE_AUDIO */
          /* 	if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed) last_seek_pos = mainw->pulsed->seek_pos; */
          /* #endif */
        }

        //g_print("DISK PR is %f\n", mainw->disk_pressure);
        cpuload = get_core_loadvar(0);
        if (*cpuload < CORE_LOAD_THRESH || mainw->pred_clip != -1) {
          load_frame_image(sfile->frameno);
        }
        mainw->inst_fps = get_inst_fps(FALSE);
        if (prefs->show_player_stats) {
          mainw->fps_measure++;
        }

        /// ignore actual value of actual_frame, since it can be messed with (e.g. nervous mode)
        mainw->actual_frame = sfile->frameno;
        mainw->fps_mini_measure++;
      } // end load_frame_image()
    } else spare_cycles++;

    last_req_frame = requested_frame;

#ifdef ENABLE_PRECACHE
    if (mainw->frame_layer_preload) {
      if (mainw->pred_clip != -1) {
        frames_t pframe = labs(mainw->pred_frame);
        if (mainw->pred_clip != mainw->current_file
            || (sfile->pb_fps >= 0. && (pframe <= requested_frame || pframe < sfile->frameno))
            || (sfile->pb_fps < 0. && (pframe >= requested_frame || pframe > sfile->frameno))) {
          cleanup_preload = TRUE;
          getahead = -1;
          drop_off = FALSE;
        }
      } else mainw->frame_layer_preload = NULL;
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
    }

    mainw->force_show = FALSE;
    /// set this in case we switch
    sfile->frameno = requested_frame;
    scratch = SCRATCH_NONE;
  } // end show_frame
  else spare_cycles++;


#ifdef ENABLE_PRECACHE
  if (cleanup_preload) {
    if (mainw->frame_layer_preload) {
      if (getahead > -1 || is_layer_ready(mainw->frame_layer_preload)) {
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
      }}}
  // *INDENT-ON*
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
            || (getahead > -1 &&
                mainw->pred_frame != -getahead))) {
#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("PRELOADING (%d %d %lu %p):", sfile->frameno, dropped,
      //spare_cycles, mainw->frame_layer_preload);
#endif
#ifdef ENABLE_PRECACHE
      if (!mainw->frame_layer_preload) {
        if (!mainw->preview) {
          mainw->pred_clip = mainw->playing_file;
          if (getahead > -1) mainw->pred_frame = getahead;
          else {
            lives_decoder_t *dplug = NULL;
            lives_decoder_sys_t *dpsys = NULL;
            int64_t pred_frame = mainw->pred_frame;
            double last_est_time = -1.;

            if (sfile->pb_fps > 0.)
              pred_frame = sfile->frameno + 1 + dropped;
            else
              pred_frame = sfile->frameno - 1 - dropped;

            if (sfile->clip_type == CLIP_TYPE_FILE) {
              dplug = (lives_decoder_t *)sfile->ext_src;
              if (dplug) dpsys = (lives_decoder_sys_t *)dplug->dpsys;
            }

            if (!mainw->xrun_active && !did_switch) {
              for (int zz = 0; zz < 6; zz++) {
                int64_t new_pred_frame;
                double est_time;

                if (is_virtual_frame(mainw->playing_file, pred_frame)) {
                  if (!dpsys || !dpsys->estimate_delay) break;
                  est_time = (*dpsys->estimate_delay)(dplug->cdata, get_indexed_frame(mainw->playing_file, pred_frame));
                } else {
                  // png timings
                  est_time = sfile->img_decode_time;
                }

                if (est_time <= 0.) break;
                if (last_est_time > -1. && est_time > last_est_time) break;
                new_pred_frame = sfile->frameno + (frames_t)(est_time * sfile->pb_fps + .9999);

                if (sfile->pb_fps > 0.) {
                  if (new_pred_frame == sfile->frameno) new_pred_frame++;
                } else {
                  if (new_pred_frame == sfile->frameno) new_pred_frame--;
                }
                if (new_pred_frame == pred_frame) break;
                pred_frame = new_pred_frame;
                //if (est_time < last_est_time) break;
                last_est_time = est_time;
              }
            }
            mainw->pred_frame = pred_frame;
          }

          if (mainw->pred_frame > 0 && mainw->pred_frame < sfile->frames) {
            const char *img_ext = get_image_ext_for_type(sfile->img_type);
            if (!is_virtual_frame(mainw->pred_clip, mainw->pred_frame)) {
              /* mainw->frame_layer_preload = lives_layer_new_for_frame(mainw->pred_clip, mainw->pred_frame); */
              /* pull_frame_threaded(mainw->frame_layer_preload, img_ext, (weed_timecode_t)mainw->currticks, 0, 0); */
            } else {
              // if the target is a clip-frame we have to decode it now, since we cannot simply decode 2 frames simultaneously
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
                g_print("cached frame %ld\n", mainw->pred_frame);
              }
              if (getahead > 0) {
                mainw->pred_frame = -getahead;
                mainw->force_show = TRUE;
              }
	      // *INDENT-OFF*
	    }}
	  else mainw->pred_frame = 0;
	}}
#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("frame %ld already in cache\n", mainw->pred_frame);
#endif
#endif
    }}
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

      if (mainw->cancelled != CANCEL_NONE) {
        cancel_process(FALSE);
        return ONE_MILLION + mainw->cancelled;
      }
      return 0;
    }
  }

  if (LIVES_IS_PLAYING) {
    if (mainw->record && !mainw->record_paused)
      event_list_add_end_events(mainw->event_list, TRUE);
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
