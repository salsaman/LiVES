// player.c
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
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

#define REC_IDEAL

#define ENABLE_PRECACHE

LIVES_GLOBAL_INLINE int lives_set_status(int status) {
  mainw->status |= status;
  return mainw->status;
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

  mainw->status = 0;

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

  if (((mainw->status & ACTIVE_STATUS) == LIVES_STATUS_PLAYING)
      && !mainw->multitrack && (mainw->record || mainw->record_paused))
    lives_set_status(LIVES_STATUS_RECORDING);
  else lives_unset_status(LIVES_STATUS_RECORDING);

  if (mainw->preview || mainw->preview_rendering ||
      (!mainw->multitrack && LIVES_IS_PLAYING
       && mainw->event_list && !(mainw->status & LIVES_STATUS_RECORDING)))
    lives_set_status(LIVES_STATUS_PREVIEW);
  else lives_unset_status(LIVES_STATUS_PREVIEW);

  if (mainw->error) mainw->status |= LIVES_STATUS_ERROR;

  return mainw->status & ACTIVE_STATUS;
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

  if (lives_get_status() != LIVES_STATUS_RENDERING && mainw->play_window && LIVES_IS_WIDGET(mainw->preview_image)) {
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
  if (lives_get_status() == LIVES_STATUS_RENDERING) {
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
  if (!mainw->helper_procthreads[PT_LAZY_RFX] && !prefs->vj_mode)
    lives_widget_set_sensitive(mainw->rfx_submenu, FALSE);
  lives_widget_set_sensitive(mainw->import_theme, FALSE);
  lives_widget_set_sensitive(mainw->export_theme, FALSE);
}


void player_sensitize(void) {
  lives_widget_set_sensitive(mainw->rte_defs_menu, TRUE);
  lives_widget_set_sensitive(mainw->utilities_submenu, TRUE);
  if (!mainw->helper_procthreads[PT_LAZY_RFX] && !prefs->vj_mode)
    lives_widget_set_sensitive(mainw->rfx_submenu, TRUE);
  lives_widget_set_sensitive(mainw->import_theme, TRUE);
  lives_widget_set_sensitive(mainw->export_theme, TRUE);
}


void track_source_free(int i, int oclip) {
  if (i >= 0 && i < MAX_TRACKS) {
    if (mainw->track_sources[i] && (oclip == 0 || mainw->active_track_list[i] == oclip
                                    || (mainw->old_active_track_list[i] == oclip
                                        && mainw->active_track_list[i] != oclip))) {
      if (!oclip) oclip = mainw->old_active_track_list[i];
      if (IS_VALID_CLIP(oclip)) {
        lives_clip_src_t *dsource = mainw->track_sources[i];
        // need to set current layer if possible
        weed_layer_t *layer = dsource->layer;
        if (layer) weed_layer_set_invalid(layer, TRUE);
        if (dsource->purpose == SRC_PURPOSE_TRACK)
          clip_source_remove(oclip, i, SRC_PURPOSE_TRACK);
        else {
          dsource->flags |= SRC_FLAG_INACTIVE;
          if (!mainw->multitrack) {
            chill_decoder_plugin(oclip); /// free buffers to relesae memory
          }
        }
      }
      mainw->track_sources[i] = NULL;
    }
    if (mainw->active_track_list[i] == mainw->old_active_track_list[i])
      mainw->active_track_list[i] = 0;
    mainw->old_active_track_list[i] = 0;
  }
}



LIVES_GLOBAL_INLINE void init_track_sources(void) {
  for (int i = 0; i < MAX_TRACKS; i++) {
    mainw->track_sources[i] = NULL;
    mainw->old_active_track_list[i] = mainw->active_track_list[i] = 0;
  }
}


LIVES_GLOBAL_INLINE void free_track_sources(void) {
  for (int i = 0; i < MAX_TRACKS; i++) {
    if (mainw->track_sources[i]) {
      mainw->old_active_track_list[i] = 0;
      track_source_free(i, mainw->active_track_list[i]);
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

#define LAGFRAME_TRIGGER 8
#define BFC_LIMIT 1000

static frames_t cache_hits = 0, cache_misses = 0;

static void play_toy(void) {
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
  }
}

static char *framecount = NULL;
static char *fname_next = NULL, *info_file = NULL;
static int opwidth = 0, opheight = 0;
static const char *img_ext = NULL;

weed_layer_t **map_sources_to_tracks(boolean rndr) {
  weed_layer_t **layers = NULL;
  int oclip, nclip, i;

  if (!rndr) {
    if (!mainw->multitrack) {
      layers = (weed_layer_t **)lives_calloc(3, sizeof(weed_layer_t *));
      lives_freep((void **)&mainw->clip_index);
      lives_freep((void **)&mainw->frame_index);
      if (mainw->num_tr_applied && IS_VALID_CLIP(mainw->blend_file)
          && (mainw->blend_file != mainw->playing_file || prefs->tr_self)) {
        mainw->num_tracks = 2;
        mainw->clip_index = (int *)lives_calloc(2, sizint);
        mainw->frame_index = (frames64_t *)lives_calloc(2, sizeof(frames64_t));
        mainw->active_track_list[1] = mainw->blend_file;
        mainw->clip_index[1] = mainw->blend_file;
        mainw->frame_index[1] = mainw->files[mainw->blend_file]->frameno;;
      } else {
        mainw->num_tracks = 1;
        mainw->clip_index = (int *)lives_calloc(1, sizint);
        mainw->frame_index = (frames64_t *)lives_calloc(1, sizeof(frames64_t));
      }
      mainw->active_track_list[0] = mainw->playing_file;
      mainw->clip_index[0] = mainw->playing_file;
      mainw->frame_index[0] = mainw->actual_frame;
    } else {
      // layers here will be an array corresponding to the frame layers in the player
      // for Clip Editor we may have 1 or 2 (if transitions are active)
      // in multitrack mode, we can have any number 0 -> MAX_TRACKS
      // we can also separated backing audio tracks, but these are not represented her
      layers = (weed_layer_t **)lives_calloc((mainw->num_tracks + 1), sizeof(weed_layer_t *));

      // get list of active tracks from mainw->filter map
      // for multitrack, the most recent filter_map defines how the layers are combined;
      // if there is no active filter map then we only see the frontmost layer
      // The mapping of clips / frames at the current playback time is held in clip_index / frame_index
      // these are set from Frame events
      get_active_track_list(mainw->clip_index, mainw->num_tracks, mainw->filter_map);
    }
  } else layers = (weed_layer_t **)lives_calloc(mainw->num_tracks + 1, sizeof(weed_layer_t *));

  // here we compare the mapping of clips -> tracks with the previous values
  // if the clip mapping for a track has changed we have to note this
  // if we have an extra copy of an used clip, then we add an extra decoder for the clip
  // otherwise we would end up jumping back and forth in the same decoder
  // first we check if the primary decoder (primary_src) is in use

  for (i = 0; i < mainw->num_tracks; i++) {
    if (rndr) {
      layers[i] = lives_layer_new_for_frame(mainw->clip_index[i], mainw->frame_index[i]);
      weed_layer_ref(layers[i]);
      weed_layer_set_palette(layers[i], (mainw->clip_index[i] == -1 ||
                                         mainw->files[mainw->clip_index[i]]->img_type ==
                                         IMG_TYPE_JPEG) ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32);
    }

    // if the clip mapping changed for a track, then we need to check if its primary_src is in use
    // if not we may use that, otherwise we have to add an additional decoder
    if ((oclip = mainw->old_active_track_list[i]) != (nclip = mainw->active_track_list[i])) {
      if (mainw->track_sources[i]) track_source_free(i, oclip);
      if (nclip > 0) {
        if (mainw->files[nclip]->clip_type == CLIP_TYPE_FILE) {
          if (mainw->files[nclip]->primary_src->flags & SRC_FLAG_INACTIVE) {
            mainw->track_sources[i] = mainw->files[nclip]->primary_src;
            mainw->files[nclip]->primary_src->flags &= ~SRC_FLAG_INACTIVE;
          } else {
            mainw->track_sources[i] = get_clip_source(nclip, i, SRC_PURPOSE_TRACK);
            if (!mainw->track_sources[i]) {
              //g_print("CLONING\n");
              //reset_timer_info();
              add_decoder_clone(mainw->blend_file, i, SRC_PURPOSE_TRACK);
              mainw->track_sources[i] = get_clip_source(mainw->blend_file, i, SRC_PURPOSE_TRACK);
              //show_timer_info();
            }
          }
          mainw->old_active_track_list[i] = mainw->active_track_list[i] = nclip;
          if (rndr) {
            if (nclip > 0)
              // set alt src in layer
              lives_layer_set_source(layers[i], mainw->track_sources[i]);
            else weed_layer_pixel_data_free(layers[i]);
	      // *INDENT-OFF*
	    }}}}}
  // *INDENT-ON*
  layers[i] = NULL;

  if (!rndr && !mainw->multitrack) {
    // in clip edit mode, we can sometimes end up with fg clip being a track decoder and bg
    // clip being the primary
    if (mainw->track_sources[0] != mainw->files[mainw->playing_file]->primary_src
        && mainw->track_sources[1] == mainw->files[mainw->playing_file]->primary_src) {
      swap_clip_sources(mainw->playing_file, -1, SRC_PURPOSE_PRIMARY, SRC_PURPOSE_TRACK);
    }
  }
  return layers;
}


// retval 1 == goto lfi_done
static int render_frame(frames_t frame) {
  weed_layer_t **layers = NULL;
  weed_timecode_t tc = 0;
  lives_clip_t *sfile = mainw->files[mainw->playing_file];
  boolean rndr = FALSE;
  int bad_frame_count = 0;
  int retval;

  if ((mainw->is_rendering && !(mainw->proc_ptr && mainw->preview))) {
    rndr = TRUE;
    tc = mainw->cevent_tc;
  }

  // here if we are rendering from multitrack, previewing a recording, or applying realtime effects to a selection
  //if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
  if (rndr && mainw->scrap_file != -1 && mainw->clip_index[0] == mainw->scrap_file && mainw->num_tracks == 1) {
    // pull from scrap_file -
    // do not apply fx, just pull frame
    lives_clip_src_t *dsource;
    mainw->frame_layer = lives_layer_new_for_frame(mainw->clip_index[0], mainw->frame_index[0]);
    weed_layer_ref(mainw->frame_layer);
    dsource = get_clip_source(mainw->clip_index[0], -1, SRC_PURPOSE_PRIMARY);
    lives_layer_set_source(mainw->frame_layer, dsource);
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
    // - first, set all the values of mainw->primary_src_used to FALSE for each entry in active_track_list
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
    // mainw->track_sources points to the decoder for each track, this can either be NULL, the original decoder, or the clone
    //
    // in clip editor (VJ) mode, this is only necessary if self transitions are enabled

    // if blend_file == playing_file, then the blend_file ONLY must get a clone
    // e.g if playing_file is switched to equal blend_file - without this, blend_file would maintain the original,
    //         playing_file would get the clone - in this case we swap the track decoders, so blend_file gets the clone, and
    //          playing_file gets the original
    // - if either playing_file or blend_file are changed, if they then have differing values, blend_file
    //     can reqlinquish the clone and get the original, we must also take care that if all transitions are switched off,
    //   blend_file releases its clone before evaporating

    layers = map_sources_to_tracks(rndr);

    if (rndr) {
      mainw->frame_layer = weed_apply_effects(layers, mainw->filter_map,
                                              tc, opwidth, opheight, mainw->pchains);
      for (int i = 0; layers[i]; i++) {
        weed_layer_unref(layers[i]);
        if (layers[i] != mainw->frame_layer) {
          check_layer_ready(layers[i]);
          weed_layer_unref(layers[i]);
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
        return 1;
      }
    } else {
      if (prefs->dev_show_timing)
        g_printerr("pull_frame @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      // normal playback in the clip editor, or applying a non-realtime effect
      if (!mainw->preview || lives_file_test(fname_next, LIVES_FILE_TEST_EXISTS)) {
        if (!mainw->frame_layer) {
          lives_clip_src_t *dsource;
          mainw->frame_layer = lives_layer_new_for_frame(mainw->current_file, mainw->actual_frame);
          weed_layer_ref(mainw->frame_layer);
          weed_layer_set_invalid(mainw->frame_layer, FALSE);
          dsource = get_clip_source(mainw->playing_file, -1, SRC_PURPOSE_PRIMARY);
          lives_layer_set_source(mainw->frame_layer, dsource);
        }

        if (!img_ext) img_ext = get_image_ext_for_type(cfile->img_type);
        if (mainw->preview && !mainw->frame_layer
            && (!mainw->event_list || cfile->opening)) {
          // external frame manipulation preview
          //g_print("loaing flay2 as %p\n", mainw->frame_layer);
          if (!pull_frame_at_size(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks,
                                  cfile->hsize, cfile->vsize, WEED_PALETTE_END)) {
            if (mainw->frame_layer) {
              weed_layer_unref(mainw->frame_layer);
              weed_layer_unref(mainw->frame_layer);
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
          // no preiv - realtime pb
          // check first if got handed an external layer to play
          if (mainw->ext_layer) {
            mainw->frame_layer = weed_layer_copy(NULL, mainw->ext_layer);
          } else {
            // then check if we a have preloaded (cached) frame
            // there are two ways we an get a preload - either normal playback when we have spare cycles
            // or when we are lagging and trying to jump ahead
            // the frame number is in mainw->pred_frame
            // once the frame is loaded (is_layer_ready returns TRUE), we check it is within range
            // if it is behind mainw->actual_frame, then we discard it (set mainw->pred_frame == 0)
            // otherwise, set mainw->pred_frame = -mainw->pred_frame
            // if it is beyond requested_frame + LAGFRAME_TRIGGER / 2, it is too far ahead and we ignore it for now
            // otherwise we play it; if it is <= requested_frame we know we will play it only once, so we
            // steal the pixel_data and set mainw->pred_frame to 0, else we copy the pixel_data
            boolean got_preload = FALSE;
            /* g_print("check for preload %p %ld %d\n", */
            /* 	mainw->frame_layer_preload, mainw->pred_frame, */
            /* 	is_layer_ready(mainw->frame_layer_preload)); */

            if (mainw->pred_frame && mainw->frame_layer_preload && mainw->pred_clip == mainw->playing_file
                && is_layer_ready(mainw->frame_layer_preload)) {

              frames_t delta_a = (labs(mainw->pred_frame) - mainw->actual_frame) * sig(cfile->pb_fps);
              frames_t delta_r = (labs(mainw->pred_frame) - sfile->last_frameno) * sig(cfile->pb_fps);
              if (delta_a < 0) {
                mainw->pred_frame = 0;
              } else {
                if (delta_r <= LAGFRAME_TRIGGER / 2) {
                  if (mainw->pred_frame > 0) {
                    mainw->pred_frame = -mainw->pred_frame;
                    check_layer_ready(mainw->frame_layer_preload);
                    if (weed_layer_get_pixel_data(mainw->frame_layer_preload)) {
                      if (sfile->clip_type == CLIP_TYPE_FILE) {
                        // depending on frame value we either make a deep or shallow copy of the cache frame
                        lives_clip_src_t *dsource;
                        swap_clip_sources(mainw->playing_file, 0, SRC_PURPOSE_PRIMARY, SRC_PURPOSE_PRECACHE);
                        dsource = get_clip_source(mainw->playing_file, -1, SRC_PURPOSE_PRIMARY);
                        weed_layer_set_invalid(mainw->frame_layer, FALSE);
                        lives_layer_set_source(mainw->frame_layer, dsource);
                        mainw->track_sources[0] = dsource;
			  // *INDENT-OFF*
			}}}
		  // *INDENT-ON*

                  /* g_print("THANKS for %p,! %d %ld should be %d, right  --  %d", */
                  /*         mainw->frame_layer_preload, mainw->pred_clip, */
                  /*         labs(mainw->pred_frame), sfile->last_frameno, */
                  /*         sfile->last_frameno + LAGFRAME_TRIGGER / 2 * (int)(sig(sfile->pb_fps))); */

                  got_preload = TRUE;
                  mainw->actual_frame = labs(mainw->pred_frame);

                  // +ve value...make a deep copy, e.g we got the frame too early
                  // and we may need to reshow it several times
                  if (mainw->frame_layer) {
                    weed_layer_pixel_data_free(mainw->frame_layer);
                  }
                  if (delta_r > 0)  {
                    mainw->frame_layer = weed_layer_copy(NULL, mainw->frame_layer_preload);
                    weed_leaf_dup(mainw->frame_layer, mainw->frame_layer_preload, WEED_LEAF_CLIP);
                    weed_leaf_dup(mainw->frame_layer, mainw->frame_layer_preload, WEED_LEAF_FRAME);
                    weed_leaf_dup(mainw->frame_layer, mainw->frame_layer_preload, WEED_LEAF_NATURAL_SIZE);
                  } else {
                    mainw->frame_layer = mainw->frame_layer_preload;
                    mainw->frame_layer_preload = NULL;
		    // *INDENT-OFF*
		  }

		}}}
	    // *INDENT-ON*

            if (!got_preload) {
              // if we didn't have a preloaded frame, we kick off a thread here to load it
              lives_clip_src_t *dsource = get_clip_source(mainw->playing_file, -1, SRC_PURPOSE_PRIMARY);
              weed_layer_set_invalid(mainw->frame_layer, FALSE);
              lives_layer_set_source(mainw->frame_layer, dsource);
              pull_frame_threaded(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
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
          return 1;
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
        return 1;
      }

      if (!mainw->frame_layer && (!mainw->preview || (mainw->multitrack && !cfile->opening))) {
        lives_freep((void **)&info_file);
        return 1;
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
          return 1;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
  return 0;
}

static weed_layer_t *old_frame_layer = NULL;

weed_layer_t *get_old_frame_layer(void) {return old_frame_layer;}
void reset_old_frame_layer(void) {old_frame_layer = NULL;}

weed_layer_t *load_frame_image(frames_t frame) {
  // this is where we do the actual load/record of a playback frame
  // it is called (ideally) every 1/fps from do_progress_dialog() via process_one()
  // frame is the (default) frame to be played in the 'foreground' clip
  // this may differ from the actual frame displayed / returned, due to preload caching and other factors
  // the resulting layer will be post effects / palette conversion / resize or letteboxing, and gamma correction

  // for the multitrack window we can use thr return value; this is used to display the
  // preview image
  //
  // returned layer is guaranteed not to be unreffed until at least any subsequent call to load_frame_image
  // (or until playback ends)

  // the function works in stages - first we check if the layer to b eplayed is loaded (e.g. cached)
  // if not we start threads to fetch it
  //
  // if effects are active we call a function to apply them - the function will return an effected frame
  // otherwise we wait if the frame is not loaded
  //
  // following this we prepare the frame for display / streaming
  // - if the frame is in RGB format and the target isnt, we apply any gamma correction
  //  plus overlays
  // - convert the palette to the target, if target is RGB then we try to do gamma correction simultaneously
  // - check gamma to be sure
  // - resize or letterbox
  //     check palette again as resize can alter this
  // - , apply overlays if we didnt before

  void **pd_array = NULL, **retdata = NULL;
  LiVESPixbuf *pixbuf = NULL;
  weed_layer_t *frame_layer = NULL;

  LiVESInterpType interp;
  char *tmp;

  boolean was_preview = FALSE;
  boolean rec_after_pb = FALSE;
  boolean success = FALSE;
  boolean size_ok = TRUE;
  boolean player_v2 = FALSE;
  boolean conv_done = FALSE;

  int layer_palette, cpal;
  int retval;

  static int old_pwidth = 0, old_pheight = 0, oseq = -1;
  int pwidth, pheight;
  int lb_width = 0, lb_height = 0;
  int fg_file = mainw->playing_file;
  int tgt_gamma = WEED_GAMMA_UNKNOWN;
  boolean was_letterboxed = FALSE;

  if (mainw->play_sequence != oseq) old_frame_layer = NULL;
  oseq = mainw->play_sequence;

  framecount = NULL;
  fname_next = info_file = NULL;
  opwidth = opheight = 0;
  img_ext = NULL;

  mainw->scratch = SCRATCH_NONE;

  mainw->frame_layer = NULL;

  if (LIVES_UNLIKELY(cfile->frames == 0 && !mainw->foreign && !mainw->is_rendering
                     && AUD_SRC_INTERNAL)) {
    // playing a clip with zero frames...
    if (mainw->record && !mainw->record_paused) {
      // add blank frame
      weed_plant_t *event = get_last_event(mainw->event_list);
      weed_plant_t *event_list = insert_blank_frame_event_at(mainw->event_list, lives_get_relative_ticks(mainw->origsecs,
                                 mainw->orignsecs), &event);
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

    return NULL;
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
          else {
            if (AUD_SRC_INTERNAL && AV_CLIPS_EQUAL &&
                !(prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS)
                && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
              mainw->scratch = SCRATCH_JUMP;
            } else if (mainw->scratch != SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;
	    // *INDENT-OFF*
	  }}}
      // *INDENT-ON*

      if (mainw->opening_loc || !CURRENT_CLIP_IS_NORMAL) {
        framecount = lives_strdup_printf("%9d", mainw->actual_frame);
      } else {
        framecount = lives_strdup_printf("%9d / %d", mainw->actual_frame, cfile->frames);
      }

      /////////////////////////////////////////////////

      // record performance
      if (LIVES_IS_RECORDING) {
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
          mainw->scrap_file_size = mainw->files[mainw->scrap_file]->f_size;
          bg_file = -1;
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
          play_toy();
        }
      }
    }
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
          if (CURRENT_CLIP_HAS_VIDEO && cfile->frames != 123456789)
            framecount = lives_strdup_printf("%9d / %d", frame, cfile->frames);
          else
            framecount = lives_strdup_printf("%9d", frame);
        }
        if (mainw->toy_type != LIVES_TOY_NONE)
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
      // if we are playing a backend preview, we may need to call this several times until the
      // backend frame is rendered
      // otherwise this will pull the foreground frame (or more likely, start a thread to do this)
      // and possibly kick off another thread to fetch a background frame
      retval = render_frame(frame);
    } while (!retval && !mainw->frame_layer && mainw->cancelled == CANCEL_NONE
             && cfile->clip_type == CLIP_TYPE_DISK);

    lives_freep((void **)&info_file);

    if (retval == 1) goto lfi_done;

    if (LIVES_UNLIKELY((!mainw->frame_layer) || mainw->cancelled != CANCEL_NONE)) {
      // NULL frame or user cancelled
      if (mainw->frame_layer) {
        check_layer_ready(mainw->frame_layer);
        weed_layer_unref(mainw->frame_layer);
        mainw->frame_layer = NULL;
      }
      goto lfi_done;
    }

    if (was_preview) lives_free(fname_next);

    // OK. Here is the deal now. We have a layer from the current file, current frame.
    // (or at least we sent out a thread to fetch it).
    // (and possibly a thread to fetch the background frame)
    // We will pass these layers into the effects, and we will get back a layer.
    // The palette of the effected layer could be any Weed palette, so we may need to convert
    // resize, and do gamma correction.
    // We will pass the layer to all playback plugins. Some plugins support multiple palettes, however
    // switching palettes during playback can cause visual discrpencies, so this is avoided
    // Finally we may want to end up with a GkdPixbuf (unless the playback plugin is VPP_DISPLAY_LOCAL
    // and we are in full screen mode).

    size_ok = TRUE;

    if (!prefs->vj_mode && (mainw->current_file != mainw->scrap_file || mainw->multitrack)
        && mainw->pwidth > 0 && mainw->pheight > 0
        && !(mainw->is_rendering && !(mainw->proc_ptr && mainw->preview))
        && !cfile->opening && !mainw->resizing && CURRENT_CLIP_IS_NORMAL
        && !is_virtual_frame(mainw->current_file, mainw->actual_frame)
        && is_layer_ready(mainw->frame_layer)) {
      // if we are pulling the frame from an IMAGE and playing back normally, check the size is what it should be
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
      if (mainw->record && (prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
          (mainw->vpp->capabilities & VPP_CAN_RETURN)) {
        rec_after_pb = TRUE;
      }

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
      int lwidth, lheight;
      weed_layer_t *return_layer = NULL;

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

      // use frame_layer instead of mainw->frame_layer

      if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
          ((weed_palette_is_rgb(layer_palette) &&
            !(weed_palette_is_rgb(mainw->vpp->palette))) ||
           (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette)))) {
        // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
        // so copy layer and convert, retaining original
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      } else {
        frame_layer = mainw->frame_layer;
      }

      if (prefs->apply_gamma) {
        // gamma correction
        tgt_gamma = WEED_GAMMA_SRGB;
        if (weed_palette_is_rgb(mainw->vpp->palette)) {
          if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
            // some playback plugins may prefer linear gamma
            tgt_gamma = WEED_GAMMA_LINEAR;
        }
      }

      if (weed_palette_is_rgb(layer_palette)
          && !weed_palette_is_rgb(mainw->vpp->palette)) {
        // TODO - delay this if by chance we are downscaling
        if (tgt_gamma != WEED_GAMMA_UNKNOWN)
          gamma_convert_layer(tgt_gamma, frame_layer);

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
          // render the timecode for multitrack playback
          if (!check_for_overlay_text(frame_layer))
            if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode)
              frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);

          if (tgt_gamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
            if (frame_layer == mainw->frame_layer)
              frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
            gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
          }
        }
        conv_done = TRUE;
      }

      // final palette conversion to whatever the playback plugin needs
      if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
      if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                      mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgt_gamma)) {
        goto lfi_done;
      }
      if (prefs->dev_show_timing)
        g_print("cl palette done %d to %d @ %f\n", weed_layer_get_palette(frame_layer), mainw->vpp->palette,
                lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (!player_v2) {
        // vid plugin v1 expects compacted rowstrides (i.e. no padding/alignment after pixel row)
        if (!compact_rowstrides(frame_layer)) {
          goto lfi_done;
        }
      }
      if (prefs->dev_show_timing)
        g_print("comp rs done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
      if (mainw->stream_ticks == -1) mainw->stream_ticks = mainw->currticks;

      if (!conv_done && weed_palette_is_rgb(mainw->vpp->palette)) {
        if (tgt_gamma != WEED_GAMMA_UNKNOWN)
          gamma_convert_layer(tgt_gamma, frame_layer);

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
          // render the timecode for multitrack playback
          if (!check_for_overlay_text(frame_layer))
            if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode)
              frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);

          if (tgt_gamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
            if (frame_layer == mainw->frame_layer)
              frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
            gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
          }
        }
      }

      if (prefs->dev_show_timing)
        g_print("gamma conv done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (rec_after_pb) {
        // record output from playback plugin

        int retwidth = mainw->pwidth / weed_palette_get_pixels_per_macropixel(mainw->vpp->palette);
        int retheight = mainw->pheight;

        return_layer = weed_layer_create(retwidth, retheight, NULL, mainw->vpp->palette);

        if (weed_palette_is_yuv(mainw->vpp->palette)) {
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
      if (player_v2) {
        // no letterboxing -> stretch it to fill window
        weed_set_double_value(frame_layer, "x_range", 1.0);
        weed_set_double_value(frame_layer, "y_range", 1.0);
      }

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
          if (frame_layer == mainw->frame_layer)
            frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
          if (!letterbox_layer(frame_layer, pwidth, pheight, lb_width, lb_height, interp,
                               mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
          was_letterboxed = TRUE;
        }
      }

      // RENDER VIA PLUGIN
      //g_print("PLAY PT 1\n");
      lwidth = weed_layer_get_width(frame_layer);
      if (!player_v2) pd_array = weed_layer_get_pixel_data_planar(frame_layer, NULL);
      if ((player_v2 && !(*mainw->vpp->play_frame)(frame_layer, mainw->currticks - mainw->stream_ticks, return_layer))
          || (!player_v2 && !(*mainw->vpp->render_frame)(lwidth, weed_layer_get_height(frame_layer),
              mainw->currticks - mainw->stream_ticks, pd_array, retdata, mainw->vpp->play_params))) {
        //vid_playback_plugin_exit();
        if (return_layer) {
          weed_layer_unref(return_layer);
          lives_free(retdata);
          return_layer = NULL;
        }
      } else success = TRUE;

      if (!player_v2) lives_free(pd_array);
      if (prefs->dev_show_timing)
        g_printerr("rend fr done @ %f\n\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

      if (frame_layer && frame_layer != mainw->frame_layer) {
        if (weed_layer_get_pixel_data(frame_layer)
            == weed_layer_get_pixel_data(mainw->frame_layer))
          weed_layer_nullify_pixel_data(frame_layer);
        weed_layer_unref(frame_layer);
      }
      frame_layer = NULL;

      if (return_layer) {
        save_to_scrap_file(return_layer);
        weed_layer_unref(return_layer);
        lives_free(retdata);
        return_layer = NULL;
      }

      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
      // DONE for vpp with resize
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

      frame_layer = mainw->frame_layer;

      if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) && !(mainw->vpp->capabilities & VPP_CAN_RESIZE) &&
          ((mainw->vpp->fwidth  < mainw->pwidth || mainw->vpp->fheight < mainw->pheight))) {
        // mainw->frame_layer will be downsized for the plugin but upsized for screen
        // so copy layer and convert, retaining original
        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      }

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

      if (prefs->apply_gamma) {
        // gamma correction
        tgt_gamma = WEED_GAMMA_SRGB;
        if (weed_palette_is_rgb(mainw->vpp->palette)) {
          if (mainw->vpp->capabilities & VPP_LINEAR_GAMMA)
            // some playback plugins may prefer linear gamma
            tgt_gamma = WEED_GAMMA_LINEAR;
        }
      }

      if (weed_palette_is_rgb(layer_palette)) {
        // TODO - delay this if by chance we are downscaling
        if (tgt_gamma != WEED_GAMMA_UNKNOWN)
          gamma_convert_layer(tgt_gamma, frame_layer);

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
          // render the timecode for multitrack playback
          if (!check_for_overlay_text(frame_layer))
            if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode)
              frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);

          if (tgt_gamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
            if (frame_layer == mainw->frame_layer)
              frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
            gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
          }
        }
        conv_done = TRUE;
      }

      if (player_v2) {
        weed_set_double_value(frame_layer, "x_range", 1.0);
        weed_set_double_value(frame_layer, "y_range", 1.0);
      }

      was_letterboxed = FALSE;

      if ((mainw->multitrack && prefs->letterbox_mt) || (!mainw->multitrack && prefs->letterbox)) {
        /// letterbox external
        lb_width = weed_layer_get_width_pixels(mainw->frame_layer);
        lb_height = weed_layer_get_height(mainw->frame_layer);
        get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, FALSE);
        if (pwidth != lb_width || pheight != lb_height) {
          needs_lb = TRUE;
          if (!(mainw->vpp->capabilities & VPP_CAN_LETTERBOX)) {
            if (frame_layer == mainw->frame_layer) {
              frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
            }
            if (layer_palette != mainw->vpp->palette && (pwidth > lb_width || pheight > lb_height)) {
              if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
              if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                              mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgt_gamma)) {
                goto lfi_done;
              }
            }
            //g_print("LB to %d X %d and from %d X %d\n", pwidth, pheight, lb_width, lb_height);
            if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
            if (!letterbox_layer(frame_layer, pwidth, pheight, lb_width, lb_height, interp,
                                 mainw->vpp->palette, mainw->vpp->YUV_clamping)) goto lfi_done;
            was_letterboxed = TRUE;
          } else {
            // plugin cannot resize, but it can letterbox, so just resize to lb size
            // and let the plugin centre it
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

      pwidth = weed_layer_get_width_pixels(frame_layer);
      pheight = weed_layer_get_height(frame_layer);

      //g_print("clp start %d %d   %d %d @\n", weed_layer_get_palette(frame_layer),
      //mainw->vpp->palette, weed_layer_get_gamma(frame_layer), tgt_gamma);
      if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
      if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                      mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgt_gamma)) {
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

      if (!conv_done && weed_palette_is_rgb(mainw->vpp->palette)) {
        if (tgt_gamma != WEED_GAMMA_UNKNOWN) {
          if (was_letterboxed)
            gamma_convert_sub_layer(tgt_gamma, 1.0, frame_layer, (pwidth - lb_width) >> 1,
                                    (pheight - lb_height) >> 1, lb_width, lb_height, TRUE);
          else
            gamma_convert_layer(tgt_gamma, frame_layer);
        }

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
          // render the timecode for multitrack playback
          if (!check_for_overlay_text(frame_layer))
            if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode)
              frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);

          if (tgt_gamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
            if (frame_layer == mainw->frame_layer)
              frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
            if (was_letterboxed)
              gamma_convert_sub_layer(tgt_gamma, 1.0, frame_layer, (pwidth - lb_width) >> 1,
                                      (pheight - lb_height) >> 1, lb_width, lb_height, TRUE);
            else
              gamma_convert_layer(tgt_gamma, frame_layer);
          }
        }
      }

      if (rec_after_pb) {
        // record output from playback plugin
        int retwidth = mainw->vpp->fwidth;
        int retheight = mainw->vpp->fheight;

        if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        return_layer = weed_layer_create(retwidth, retheight, NULL, mainw->vpp->palette);

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

      // gamma of any returned layer will be the same
      if (return_layer) weed_layer_set_gamma(return_layer, weed_layer_get_gamma(frame_layer));

      //g_print("PLAY PT 2\n");
      if (!player_v2) pd_array = weed_layer_get_pixel_data_planar(frame_layer, NULL);
      if ((player_v2 && !(*mainw->vpp->play_frame)(frame_layer,
           mainw->currticks - mainw->stream_ticks, return_layer))
          || (!player_v2 && !(*mainw->vpp->render_frame)(weed_layer_get_width(frame_layer),
              weed_layer_get_height(frame_layer),
              mainw->currticks - mainw->stream_ticks, pd_array, retdata,
              mainw->vpp->play_params))) {
        //vid_playback_plugin_exit();
        if (return_layer) {
          weed_layer_unref(return_layer);
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
          if (tgt_gamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
            // return layer will have WEED_GAMMA_MONITOR
            // TODO - save w. screen_gamma
            gamma_convert_layer(WEED_GAMMA_SRGB, return_layer);
          }
          save_to_scrap_file(return_layer);
          lives_freep((void **)&framecount);
        }
        lives_free(retdata);
        weed_layer_unref(return_layer);
        return_layer = NULL;
      }

      if (frame_layer && frame_layer != mainw->frame_layer) {
        if (weed_layer_get_pixel_data(frame_layer)
            == weed_layer_get_pixel_data(mainw->frame_layer))
          weed_layer_nullify_pixel_data(frame_layer);
        weed_layer_unref(frame_layer);
      }
      frame_layer = NULL;

      // frame display was handled by a playback plugin, skip the rest
      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
    } // EXT PB done

    ////////////////////////////////////////////////////////
    // local display - either we are playing with no playback plugin, or else the playback plugin has no
    // local display of its own
    if (prefs->dev_show_timing)
      g_printerr("clr @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    if (prefs->apply_gamma) tgt_gamma = WEED_GAMMA_SRGB;

    check_layer_ready(mainw->frame_layer); // wait for all threads to complete
    //g_print("FLL is %p %p\n", mainw->frame_layer, weed_layer_get_pixel_data(mainw->frame_layer));;

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

    ////////////////////////////////////////

    /// special value to compact the rowstrides
    if (mainw->ext_playback && !player_v2) THREADVAR(rowstride_alignment_hint) = -1 ;
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

    if (weed_palette_is_rgb(layer_palette)) {
      // TODO - delay this if by chance we are downscaling
      if (tgt_gamma != WEED_GAMMA_UNKNOWN)
        gamma_convert_layer(tgt_gamma, frame_layer);
      conv_done = TRUE;
    }

    if (prefs->dev_show_timing)
      g_printerr("res start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    lb_width = weed_layer_get_width_pixels(mainw->frame_layer);
    lb_height = weed_layer_get_height(mainw->frame_layer);
    was_letterboxed = FALSE;

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

    if (!convert_layer_palette_full(mainw->frame_layer, cpal, 0, 0, 0, tgt_gamma)) goto lfi_done;


    if (prefs->dev_show_timing)
      g_printerr("clp end @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    if (LIVES_IS_PLAYING) {
      if (!check_for_overlay_text(mainw->frame_layer)) {
        if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
          mainw->frame_layer = render_text_overlay(mainw->frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);
        }
      }
    }

    frame_layer = mainw->frame_layer;

    if (tgt_gamma == WEED_GAMMA_SRGB && prefs->use_screen_gamma) {
      frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      if (was_letterboxed)
        gamma_convert_sub_layer(WEED_GAMMA_MONITOR, 1.0, frame_layer, (pwidth - lb_width) >> 1,
                                (pheight - lb_height) >> 1, lb_width, lb_height, TRUE);
      else
        gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
    }

    //////////////////////////////////

    if (prefs->dev_show_timing)
      g_printerr("l2p start @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    pixbuf = layer_to_pixbuf(frame_layer, TRUE, TRUE);

    if (frame_layer != mainw->frame_layer) weed_layer_free(frame_layer);
    frame_layer = NULL;

    if (prefs->dev_show_timing)
      g_printerr("l2p @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

    // internal player, double size or fullscreen, or multitrack

    if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
      set_drawing_area_from_pixbuf(mainw->preview_image, pixbuf, mainw->pi_surface);
      lives_widget_queue_draw_noblock(mainw->preview_image);
    } else {
      pwidth = lives_widget_get_allocation_width(mainw->play_image);
      pheight = lives_widget_get_allocation_height(mainw->play_image);
      pthread_mutex_lock(&mainw->play_surface_mutex);
      if (pwidth < old_pwidth || pheight < old_pheight)
        clear_widget_bg(mainw->play_image, mainw->play_surface);
      old_pwidth = pwidth;
      old_pheight = pheight;
      set_drawing_area_from_pixbuf(mainw->play_image, pixbuf, mainw->play_surface);
      pthread_mutex_unlock(&mainw->play_surface_mutex);
      lives_widget_queue_draw_noblock(mainw->play_image);
    }

    //can we save pixbufs ?;
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

    if (!cr) return NULL;

    if (mainw->rec_vid_frames == -1) {
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), (tmp = lives_strdup_printf("%9d", frame)));
    } else {
      if (frame > mainw->rec_vid_frames) {
        mainw->cancelled = CANCEL_KEEP;
        if (CURRENT_CLIP_HAS_VIDEO) cfile->frames = mainw->rec_vid_frames;
        return NULL;
      }

      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), (tmp = lives_strdup_printf("%9d / %9d",
                           frame, mainw->rec_vid_frames)));
      lives_free(tmp);
    }

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
    xwidth = gdk_window_get_width(mainw->foreign_window);
    xheight = gdk_window_get_height(mainw->foreign_window);
    if ((pixbuf = gdk_pixbuf_get_from_window(mainw->foreign_window, 0, 0, xwidth, xheight)))
#else
#if 0
    ;
#endif
    gdk_window_get_size(mainw->foreign_window, &xwidth, &xheight);
    if ((pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(mainw->foreign_window),
                  mainw->foreign_cmap, 0, 0, 0, 0, xwidth, xheight)))
#endif
#if 0
      ;
#endif
    {
#endif
      tmp = make_image_file_name(cfile, frame, get_image_ext_for_type(cfile->img_type));
      lives_snprintf(fname, PATH_MAX, "%s", tmp);
      lives_free(tmp);

      do {
        // TODO ***: add a timeout here
        if (gerror) lives_error_free(gerror);
        pixbuf_to_png(pixbuf, fname, cfile->img_type, 100, cfile->hsize, cfile->vsize, &gerror);
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
    return NULL;
  }

lfi_done:

  if (frame_layer && frame_layer != mainw->frame_layer) {
    if (weed_layer_get_pixel_data(frame_layer)
        == weed_layer_get_pixel_data(mainw->frame_layer))
      weed_layer_nullify_pixel_data(frame_layer);
    weed_layer_unref(frame_layer);
  }
  frame_layer = NULL;

  if (!mainw->video_seek_ready) video_sync_ready();

  if (framecount) {
    if ((!mainw->fs || (prefs->play_monitor != widget_opts.monitor && capable->nmonitors > 1) ||
         (mainw->ext_playback && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)))
        && !prefs->hide_framebar) {
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
    }
    lives_free(framecount);
    framecount = NULL;
  }

  THREADVAR(rowstride_alignment_hint) = 0;

  if (success) {
    if (!mainw->multitrack &&
        !mainw->faded && (!mainw->fs || (prefs->gui_monitor != prefs->play_monitor
                                         && prefs->play_monitor != 0 && capable->nmonitors > 1))
        && mainw->current_file != mainw->scrap_file) {
      double ptrtime = ((double)mainw->actual_frame - 1.) / cfile->fps;
      mainw->ptrtime = ptrtime;
      lives_widget_queue_draw_noblock(mainw->eventbox2);
    }
    if (LIVES_IS_PLAYING && mainw->multitrack && !cfile->opening) animate_multitrack(mainw->multitrack);
  }

  if (success && mainw->frame_layer) {
    // format is (int64_t)tc|(int32_t)nclips|(int32_t)clip|(int64_t)frame|.....|(double)pb_fps
    char *tmp, *msg;
    double pb_fps = 0.;
    int clip = mainw->clip_index[0];
    lives_clip_t *sfile = RETURN_VALID_CLIP(clip);
    if (sfile) pb_fps = sfile->pb_fps;
    tmp = lives_strdup_printf("%.8f|%d|", (double)mainw->currticks / TICKS_PER_SECOND_DBL, mainw->num_tracks);
    for (int i = 0; i < mainw->num_tracks; i++) {
      tmp = lives_strdup_concat(tmp, "|", "%d|%ld", mainw->clip_index[i], mainw->frame_index[i]);
    }
    msg = lives_strdup_printf("%s|%.3f", tmp, pb_fps);
    lives_free(tmp);
    lives_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH, (const char *)msg);
    lives_free(msg);
  }

  weed_layer_ref(mainw->frame_layer);

  if (old_frame_layer) {
    weed_layer_free(old_frame_layer);
  }

  old_frame_layer = mainw->frame_layer;

  if (!mainw->multitrack && !mainw->is_rendering)
    mainw->frame_layer = NULL;

  // TODO - set mainw->frame_layer = NULL
  return old_frame_layer;
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
      *tsource == LIVES_TIME_SOURCE_SOUNDCARD) && !mainw->xrun_active) {
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
                 && CLIP_HAS_AUDIO(mainw->pulsed->playing_file))))
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
  } else {
    if (*tsource == LIVES_TIME_SOURCE_SYSTEM) interticks = clock_ticks - mainw->cadjticks;
    else  interticks = current - mainw->adjticks;
  }

  if (interticks + mainw->syncticks < 0) break_me("neg time");

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


RECURSE_GUARD_START;

frames_t clamp_frame(int clipno, frames_t nframe) {
  lives_clip_t *sfile;
  boolean is_pbframe = FALSE;
  if (clipno == -1) {
    clipno = mainw->playing_file;
    is_pbframe = TRUE;
  }
  if (!(sfile = RETURN_NORMAL_CLIP(clipno))) return 0;
  else {
    frames_t first_frame = 1, last_frame = sfile->frames;
    if (clipno == mainw->playing_file) {
      if ((mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
        last_frame = mainw->playing_sel ? sfile->end : mainw->play_end;
        if (last_frame > sfile->frames) last_frame = sfile->frames;
        first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
        if (first_frame > sfile->frames) first_frame = sfile->frames;
      }
    }
    if (nframe >= first_frame && nframe <= last_frame) return nframe;
    else {
      double fps = sfile->pb_fps;
      frames_t selrange = (1 + last_frame - first_frame);
      lives_direction_t dir, ndir;
      int nloops;

      if (!LIVES_IS_PLAYING || (fabs(fps) < 0.001 && mainw->scratch != SCRATCH_NONE))
        fps = sfile->fps;

      if (is_pbframe) {
        // check if video stopped playback
        if (mainw->whentostop == STOP_ON_VID_END && !mainw->loop_cont) {
          mainw->cancelled = CANCEL_VID_END;
          mainw->scratch = SCRATCH_NONE;
          return 0;
        }
        mainw->scratch = SCRATCH_JUMP;
      }

      if (first_frame == last_frame) return first_frame;

      // let selrange be last_frame - first_frame + 1
      // then if input frame == F
      // if playing forwards, then F < first_frame -> first_frame
      // if playing backwards, then F > last_frame -> last_frame
      // else:
      // result is (F - first_frame) = (nloops * selrange) + remainder
      // thus remainder = (F - first_frame) - (nloops * selrange)
      // if remainder is < 0 then we subtract a loop to bring it > 0
      // the remainder is then added to first_frame
      //
      // eg. first_frame == 1000, last_frame == 2000
      // fwd example: if F == 4100, then remander = (4100 - 1000) - nloops * 1000 => nloops == 3, rem 100
      // result is first_frame + remainder == 1100
      // back exmaple: F == 1900, nloops == 0, rem == 900
      // back example 2: F = 900, nloops == 0, rem == -100, -> nloops == -1, rem = 900
      //
      // if playing with ping pong loops, the direction changes each time we hit first_frame (backwards)
      // or last_frame (forwards), thus if nloops is odd, we swap the direction, if even, the direction remains
      // the same.
      // ping pong examples:
      // playing fwd, F == 3300, -> 3300 - 1000 == 2300 -> nloops == 2, rem 300, 2 is even so we keep fwd -> res = 1300
      // playing in reverse, F = -5700, -> 5700 - 1000 == -6700 -> nloops == -6, rem -700 -> nloops == -7, rem 300
      //    since 7 is odd, we change to fwd, thus 1000 + 300 = frame 1300

      ndir = dir = LIVES_DIRECTION_PAR(fps >= 0.);

      if (dir == LIVES_DIRECTION_FORWARD && nframe < first_frame) {
        // if FWD and before lower bound, just jump to lower bound
        return first_frame;
      }

      if (dir == LIVES_DIRECTION_BACKWARD && nframe  > last_frame) {
        // if BACK and after upper bound, just jump to upper bound
        return last_frame;
      }

      nframe -= first_frame;
      nloops = nframe / selrange;
      nframe -= nloops * selrange;
      if (nframe < 0) {
        nloops--;
        nframe = selrange - nframe;
      }
      nframe += first_frame;

      if (mainw->ping_pong) {
        ndir = LIVES_DIRECTION_PAR(dir + nloops);
        if (ndir != dir && ndir == LIVES_DIRECTION_BACKWARD && !clip_can_reverse(clipno))
          ndir = dir;
      }

      if (ndir != dir) {
        if (is_pbframe) {
          /// must set norecurse, otherwise we can end up in an infinite loop since dirchange_callback calls
          // calc_new_playback_position() which in turn calls this function
          RECURSE_GUARD_LOCK;
          dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
                             LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          RECURSE_GUARD_END;
        } else sfile->pb_fps = -sfile->pb_fps;
      }
    }
  }
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
        if ((mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV)) {
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

    if (mainw->scratch == SCRATCH_JUMP) {
      resync_audio(clipno, (double)nframe);
      retval = TRUE;
    }
  }

  if (mainw->scratch == SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;

  return retval;
}


frames_t calc_new_playback_position(int clipno, ticks_t otc, ticks_t *ntc) {
  // returns a frame number calculated from the last frame played by the clip, time delta since the
  // frame was played (current time - ideal time) and the clip playback fps
  // if the frame differs from last frame, ntc is adjusted backwards to timecode of the new frame
  // (ideal time)
  //
  // each clip also has a fps_scale value (default 1,) which acts like a multiplier to delta time
  // this can be used to make tempporary adjustments without alter the pb_fps (e.g scratch modes)
  //
  // IMPORTANT !!
  // No range checking is done here -
  // the returned frame should be passed to clamp_frame(),
  // and if the clip is also the audio playing clip, to check audio limits()

  ticks_t dtc = *ntc - otc;
  lives_clip_t *sfile;
  double fps;
  frames_t cframe, nframe;
  boolean is_pbframe = FALSE;
  if (clipno == -1) {
    clipno = mainw->playing_file;
    is_pbframe = TRUE;
  }
  sfile = RETURN_VALID_CLIP(clipno);

  if (!sfile) return 0;
  if (sfile->frames == 0 && !mainw->foreign) return 0;

  cframe = sfile->last_frameno;
  RETURN_VAL_IF_RECURSED(cframe);

  fps = sfile->pb_fps * sfile->fps_scale;
  if (!LIVES_IS_PLAYING || (fps < 0.001 && fps > -0.001 && mainw->scratch != SCRATCH_NONE))
    fps = sfile->fps;

  /* if (fabs(fps) < 0.001) { */
  /*   *ntc = otc; */
  /*   mainw->scratch = SCRATCH_NONE; */
  /*   if (AUD_SRC_INTERNAL) calc_aframeno(clipno); */
  /*   return cframe; */
  /* } */

  if (dtc < 0) dtc = 0;

  // dtc is delta ticks (last frame time - current time), quantise this to the frame rate and round down
  // - multiplied by the playback rate, this how many frames we will add or subtract
  dtc = q_gint64_floor(dtc, fabs(fps));

  // nframe is our new frame; convert dtc to seconds, and multiply by the frame rate,
  // then add or subtract from current frame number (last frame played)
  // the small constant is just to account for rounding errors when converting to double to int
  if (fps >= 0.)
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps + .001);
  else
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps - .001);

  // ntc is the ideal time when the returned frame SHOULD have been played
  // any remainder is carried over for subsequent calculations

  *ntc = otc + dtc;

  if (*ntc > mainw->currticks && is_pbframe) {
    // check for error - the ideal time should NEVER be ahead of the current time
    // - here we are reacting, not predicting
    g_print("ERR %ld and %ld and %ld %ld %ld %ld\n", otc, dtc, *ntc,
            mainw->currticks, mainw->startticks, mainw->syncticks);
  }

  if (nframe != cframe) {
    // generators, cameras, streams, etc are considered to have only 1 frame
    // with a varying timecode
    if (!IS_NORMAL_CLIP(clipno)) return 1;
    // when capturing external windows, we just advance a frame at a time
    if (mainw->foreign) return sfile->frameno + 1;
  }

  return nframe;
}


//#define SHOW_CACHE_PREDICTIONS

static short scratch = SCRATCH_NONE;

#define ANIM_LIM 100000 // ( divide by  100 000 000 get seconds, default is 5 msec)

// processing
static uint64_t spare_cycles, last_spare_cycles;
static ticks_t last_kbd_ticks;
static frames_t getahead = 0, test_getahead = -0, bungle_frames;

static frames_t recalc_bungle_frames = 0;
static boolean cleanup_preload;
static boolean init_timers = TRUE;
static boolean drop_off = FALSE;
static boolean check_getahead = FALSE;
static frames_t lagged, dropped, skipped;

static lives_time_source_t last_time_source;

static double jitter = 0.;

static weed_timecode_t event_start = 0;

void ready_player_one(weed_timecode_t estart) {
  event_start = 0;
  cleanup_preload = FALSE;
  mainw->pred_frame = 0;
  cache_hits = cache_misses = 0;
  lagged = dropped = skipped = 0;
  event_start = estart;
  /// INIT here
  init_timers = TRUE;
  last_kbd_ticks = 0;
  last_time_source = LIVES_TIME_SOURCE_NONE;
  getahead = 0;
  drop_off = FALSE;
  bungle_frames = -1;
  recalc_bungle_frames = 0;
  last_spare_cycles = spare_cycles = 0;
}


const char *get_cache_stats(void) {
  static char buff[1024];
  lives_snprintf(buff, 1024, "preload caches = %d, hits = %d "
                 "misses = %d,\nframe jitter = %.03f milliseconds.",
                 cache_hits + cache_misses, cache_hits, cache_misses, jitter * 1000.);
  return buff;
}


frames_t reachable_frame(int clipno, lives_decoder_t *dplug, frames_t stframe, frames_t enframe,
                         double * tsecs, double * tconf) {
  lives_decoder_sys_t *dpsys = NULL;
  lives_clip_t *sfile = RETURN_NORMAL_CLIP(clipno);
  double min_time = -1.;
  double est_time;
  double xconf = -1., xtconf = -1.;
  frames_t best_frame = 0;
  int level = 0;

  if (!sfile) return best_frame;

  if (enframe < stframe) {
    frames_t tmp = enframe;
    enframe = stframe;
    stframe = tmp;
  }

  if (sfile->clip_type == CLIP_TYPE_FILE) {
    if (dplug) dpsys = (lives_decoder_sys_t *)dplug->dpsys;
    if (dpsys) {
      if (dpsys->estimate_delay_full) level = 2;
      else if (dpsys && dpsys->estimate_delay) {
        *tconf = -1.;
        level = 1;
      }
    }
  } else {
    if (tsecs) *tsecs = sfile->img_decode_time;
    return enframe;
  }

  *tsecs = 0.;

  best_frame = stframe;

  if (stframe > 0) {
    for (frames_t frame = stframe; frame <= enframe; frame++) {
      if (is_virtual_frame(clipno, frame)) {
        if (!level) {
          // oops..
          if (tconf) *tconf = -1;
          if (tsecs) *tsecs = 0.;
          return stframe;
        }
        if (level == 2) {
          est_time = (*dpsys->estimate_delay_full)(dplug->cdata, get_indexed_frame(clipno, frame),
                     0, &xconf);
          if (fpclassify(est_time) != FP_NORMAL) continue;
          if (tconf && *tconf > 0.) {
            if (xconf < *tconf && min_time > 0.) continue;
          }
        } else est_time = (*dpsys->estimate_delay)(dplug->cdata, get_indexed_frame(clipno, frame));
        if (fpclassify(est_time) != FP_NORMAL) continue;
      } else {
        // png timings
        est_time = sfile->img_decode_time;
      }
      if (est_time > 0.) {
        if (min_time == -1 || est_time < min_time
            || (tsecs && est_time <= *tsecs)) {
          best_frame = frame;
          min_time = est_time;
          xtconf = xconf;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  if (tconf) *tconf = xtconf;
  if (tsecs) *tsecs = min_time;
  return best_frame;
}





#define CATCHUP_LIMIT .9


static frames_t find_best_frame(frames_t requested_frame, frames_t dropped, int64_t jumplim, lives_direction_t dir) {
  // try to calculate the best frame to play next
  // we cannot change direction, and we would like to be as near as possible to requested_frame
  // but we also want to find a frame we can reach in <= 1. / fps
  // if we are caching a frame then we will check if the cache frame is loaded and use that instead
  frames_t best_frame = -1;
  lives_clip_t *sfile = RETURN_VALID_CLIP(mainw->playing_file);
  if (sfile) {
    double target_fps = fabs(sfile->pb_fps);//weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);
    best_frame = requested_frame;
    if (dir * (best_frame - mainw->actual_frame) < 0) {
      best_frame = mainw->actual_frame;
      if (mainw->inst_fps < CATCHUP_LIMIT * target_fps
          || (dropped && dir * (mainw->actual_frame - requested_frame) - dropped < LAGFRAME_TRIGGER / 2)) {
        best_frame += dir;
      }
    }
    if (jumplim > 0 && dir * (best_frame - mainw->actual_frame) > jumplim) {
      best_frame = mainw->actual_frame + dir * jumplim;
    }
    if (best_frame != clamp_frame(mainw->playing_file, best_frame)) best_frame = -1;
    else {
      double targ_time = 1. / fabs(sfile->pb_fps);
      double tconf = 0.5;

      best_frame = reachable_frame(mainw->playing_file, sfile->primary_src
                                   ? (lives_decoder_t *)sfile->primary_src->source : NULL,
                                   mainw->actual_frame + dir, best_frame, &targ_time, &tconf);
      if (targ_time < 0. || targ_time > 1. / fabs(sfile->pb_fps)) best_frame = mainw->actual_frame + dir;
    }
  }
  return best_frame;
}


#ifdef RT_AUDIO
//#define ADJUST_AUDIO_RATE
#endif

#define CORE_LOAD_THRESH 120.

//#define SHOW_CACHE_PREDICTIONS
#define TEST_TRIGGER 9999

/// Values may need tuning for each clip - possible future targets for the autotuner
#define DROPFRAME_TRIGGER 4
#define JUMPFRAME_TRIGGER 16

// TODO - needs some rewriting, in particular: getahead, test_getahead, check_getahead, recalc_bungle_frames
// fixed_frame, can_realign, sync_delta, SCRATH_JUMP, SCRATCH_JUMP_NORESYNC, SCRATCH_REALIGN
// also xhexk if some external globals like switch_during_pb are still needed
int process_one(boolean visible) {
  // INTERNAL PLAYER
  // here we handle playback, as well as the "processing dialog"
  // visible == FALSE                         visible == TRUE
  //
  // for the player, the important variables are:
  // mainw->actual_frame - the last frame number shown
  // sfile->frameno == requested_frame - calculated from sfile->last_frameno and mainw->currticks - mainw->startticks
  // this is the "ideal" frame according to the clock time
  // --
  // we increment (or decrement) mainw->actual_frame each time
  // and compare with requested_frame - if we fall too far behind, we try to jump ahead and "land" on requested_frame
  // (predictive caching) plus a few extra frames (slack)
  // there is a complex set of rules to try to predict the target frame based on the previous jump time
  // possibly adjusted by estimates gathered by the decoder plugin itself
  //
  // the prediction is set in mainw->pred_frame, and we start loading this in an alternate decoder context
  // - until this is loaded, we continue with the previous frame. So that the eventual frame doe not cause too
  // much of a jump, we may drop alternate frames to get closer to the timecode.
  //
  // once loaded, we swith decoder contexts and check the predicted frame vs. the actual frame
  // - the delta is used to try to adjust the next prediction more accurately
  //
  // if pred_frame is earlier than the timecode,  we just show it (if it is ahead of actual_frame)
  // and then maybe jump again (bad)
  // if it is too far ahead, we keep the frame around and reshow it until requested_frame or pred_frame is ahead and loaded
  // this is also bad as the video will pause.
  //
  // if there is little lag and we have spare cycles, pred frame, may be used to cache a future frame
  //
  // NOTES: currently caching only operates for the foreground frame, any background frame is loaded on demand
  // for reverse playback, the method is the same, but the deltas / directions are reveresed
  // in this case we should rely more on estimates from the decoder, since it will be faster to decode frames
  // just after a keyframe, as opposed to theose further away
  // Decoder estimates are only available for certain decoders, the values are based on observed values such
  // as the seek time, byte loading time, time to decode a keyframe, time to decode an i frame and number of
  // iframes from keyframe to target. The estimates are not always accurate, so we make a first guess based
  // on recnt history, then use the estimations to adjust within a range.
  //
  // To keep things orderly, other threads may not action config changes (e.g going to / from fullscreen)
  // or clip switches at any time. Instead these are set as variables. or queued and actioned at "safe points"
  // The same holds for adding or removing effects, this actions are also deferred.
  //
  static frames_t last_req_frame = 0;
  /* #if defined HAVE_PULSE_AUDIO || defined ENABLE_JACK */
  /*   static off_t last_aplay_offset = 0; */
  /* #endif */
  volatile float *cpuload;
  //static lives_proc_thread_t gui_lpt = NULL;
  //double cpu_pressure;
  lives_clip_t *sfile = cfile;
  _vid_playback_plugin *old_vpp;
  ticks_t new_ticks;
  lives_time_source_t time_source = LIVES_TIME_SOURCE_NONE;
  static frames_t requested_frame = 0;
  static frames_t best_frame = -1;
  frames_t xrequested_frame = -1;
  boolean fixed_frame = FALSE;
  boolean show_frame = FALSE, showed_frame = FALSE;
  boolean did_switch = FALSE;
  boolean can_rec = FALSE;
  static boolean reset_on_tsource_change = FALSE;
  int old_current_file, old_playing_file;
  lives_cancel_t cancelled = CANCEL_NONE;
#ifdef ENABLE_PRECACHE
  int delta = 0;
  boolean can_precache = FALSE;
#endif
  int proc_file = -1;
  int aplay_file = -1;
#ifdef ADJUST_AUDIO_RATE
  static double audio_stretch = 1.0;
  ticks_t audio_ticks = -2;
#endif
  lives_clip_data_t *cdata = NULL;
  int64_t jumplim = 0;
  int retval = 0;

  // current video playback direction
  lives_direction_t dir = LIVES_DIRECTION_NONE;
  lives_hook_stack_t **phstacks = lives_proc_thread_get_hook_stacks(mainw->player_proc);
  old_current_file = mainw->current_file;
  old_playing_file = mainw->playing_file;

  if (visible) goto proc_dialog;

  //check_mem_status();

  sfile = mainw->files[mainw->playing_file];

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
player_loop:
  old_playing_file = mainw->playing_file;
  old_vpp = mainw->vpp;

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

  // TODO - make adjustable
  lives_nanosleep(10000);

  last_time_source = time_source;
  time_source = LIVES_TIME_SOURCE_NONE;

  mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
  if (mainw->currticks < mainw->startticks) {
    break_me("cur start");
  }
  //g_print("LOOP\n");
  if (mainw->currticks == -1) {
    if (time_source == LIVES_TIME_SOURCE_SOUNDCARD) handle_audio_timeout();
    mainw->cancelled = CANCEL_ERROR;
    retval = mainw->cancelled;
    goto err_end;
  }

  if (time_source != last_time_source && last_time_source != LIVES_TIME_SOURCE_NONE
      && reset_on_tsource_change) {
    reset_on_tsource_change = FALSE;
    mainw->last_startticks = mainw->startticks = mainw->fps_mini_ticks = mainw->currticks;
  }

  if (init_timers) {
    init_timers = FALSE;
    mainw->last_startticks = mainw->startticks = mainw->fps_mini_ticks = mainw->currticks;
    mainw->last_startticks--;
    mainw->fps_mini_measure = 0;
    last_req_frame = sfile->frameno - 1;
    getahead = 0;
    test_getahead = -1;
    mainw->actual_frame = sfile->frameno;
    mainw->offsetticks -= mainw->currticks;
    cdata = get_clip_cdata(mainw->playing_file);
    if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
      g_print("decoder: seek flags = %d, jump_limit = %ld, max_fps = %.4f\n",
              cdata->seek_flag,
              cdata->jump_limit, cdata->max_decode_fps);
      jumplim = cdata->jump_limit * 4;
      if (!jumplim) jumplim = JUMPFRAME_TRIGGER;
      jumplim = MIN(jumplim, JUMPFRAME_TRIGGER);
    } else jumplim = 0;
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
        (mainw->currticks - mainw->offsetticks) > TICKS_PER_SECOND * 10
        && ((audio_ticks = lives_jack_get_time(mainw->jackd)) > mainw->offsetticks || audio_ticks == -1)) {
      if (audio_ticks == -2) audio_ticks = lives_jack_get_time(mainw->jackd);
      if (audio_ticks == -1) {
        if (mainw->cancelled == CANCEL_NONE) {
          if (IS_VALID_CLIP(mainw->playing_file)  && !sfile->is_loaded) mainw->cancelled = CANCEL_NO_PROPOGATE;
          else mainw->cancelled = CANCEL_AUDIO_ERROR;
          retval = mainw->cancelled;
          goto err_end;
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

    IF_APLAYER_PULSE(
      double target_fps = fabs(sfile->pb_fps);//weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);
      if (AUD_SRC_INTERNAL && !LIVES_IS_RENDERING
    && (mainw->currticks - mainw->offsetticks) > TICKS_PER_SECOND * 10) {
    audio_stretch /= target_fps / mainw->inst_fps;
    // if too fast we increase the apparent sample rate so that it gets downsampled more
    // if too slow we decrease the apparent sample rate so that it gets upsampled more
    if (audio_stretch < 0.5) audio_stretch = 0.5;
      if (audio_stretch > 1.5) audio_stretch = 1.5;
      mainw->audio_stretch = audio_stretch;
      if (audio_stretch > 1.) audio_stretch = 1. + (audio_stretch - 1.) / 2.;
      else if (audio_stretch < 1.) audio_stretch = 1. - (1. - audio_stretch) / 2.;
    }
    )
  }
#endif

  /// SWITCH POINT

  /// during playback this is the only place to update certain variables,
  /// e.g. current / playing file, playback plugin. Anywhere else it should be deferred by setting the appropriate
  /// update value (e.g. mainw->new_clip, mainw->new_vpp)
  /// the code will enforce this so that setting the values directly will cause playback to end

  // we allow an exception only when starting or stopping a generator

  if (mainw->current_file != old_current_file || mainw->playing_file != old_playing_file
      || mainw->vpp != old_vpp) {
    mainw->cancelled = CANCEL_INTERNAL_ERROR;
    retval = mainw->cancelled;
    goto err_end;
  }

  if (mainw->new_vpp) {
    mainw->vpp = open_vid_playback_plugin(mainw->new_vpp, TRUE);
    mainw->new_vpp = NULL;
    old_vpp = mainw->vpp;
  }

switch_point:

  if (IS_VALID_CLIP(mainw->close_this_clip)) {
    // first deal with the case where we are asked to close a clip, and the
    // clip to switch to is invalid, or is the playing clip
    // - -we do the close but the clip will not be switched, instead when can_switch_clips is TREU
    // and noseek is also TRUE, the function returns the new clip
    // if this is the clip being played then we need do nothing mor
    if (mainw->close_this_clip == mainw->playing_file) {
      weed_layer_set_invalid(mainw->frame_layer, TRUE);
      weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);
    }
    if (mainw->close_this_clip == mainw->blend_file) weed_layer_set_invalid(mainw->blend_layer, TRUE);

    if (mainw->new_clip == mainw->close_this_clip || !IS_VALID_CLIP(mainw->new_clip)
        || mainw->new_clip == mainw->playing_file) {
      mainw->can_switch_clips = TRUE;
      mainw->current_file = mainw->close_this_clip;
      mainw->new_clip = close_current_file(mainw->playing_file);
      mainw->can_switch_clips = FALSE;
      if (!IS_VALID_CLIP(mainw->new_clip) && !IS_VALID_CLIP(mainw->playing_file)) {
        mainw->cancelled = CANCEL_INTERNAL_ERROR;
        retval = ONE_MILLION + mainw->cancelled;
        goto err_end;
      }
      if (mainw->new_clip == mainw->playing_file) {
        mainw->current_file = mainw->playing_file;
        mainw->new_clip = -1;
      }
      mainw->close_this_clip = -1;
    }
  }

  if (mainw->new_clip != mainw->playing_file && IS_VALID_CLIP(mainw->new_clip)) {
    if (AV_CLIPS_EQUAL && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) {
      sfile->frameno = sfile->last_frameno = requested_frame;
    }

    weed_layer_set_invalid(mainw->frame_layer, TRUE);
    weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);
    if (mainw->new_clip == mainw->blend_file || mainw->blend_file != mainw->new_blend_file)
      weed_layer_set_invalid(mainw->blend_layer, TRUE);

    if (IS_VALID_CLIP(mainw->playing_file)) {
      if (mainw->frame_layer_preload) {
        check_layer_ready(mainw->frame_layer_preload);
        weed_layer_unref(mainw->frame_layer_preload);
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

        /* g_print("HIB %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", sfile->frameno, sfile->last_frameno, */
        /*         last_req_frame, mainw->playing_file, */
        /*         aplay_file, sfile->aseek_pos, */
        /*         sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., (double)sfile->aseek_pos / */
        /*         (double)sfile->arps / 4. * sfile->fps + 1., */
        /*         mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
      }
    }
#endif
    // we cannot here simply have mainw->noswitch = FALSE,
    // as this might allow other threads to change current clip
    // so here we use mainw->can_switch_clips

    sfile->last_play_sequence = mainw->play_sequence;
    if (IS_VALID_CLIP(mainw->close_this_clip)) {
      mainw->can_switch_clips = TRUE;
      mainw->current_file = mainw->close_this_clip;
      mainw->new_clip = close_current_file(mainw->new_clip);
      mainw->can_switch_clips = FALSE;
    }

    lives_nanosleep_while_true(mainw->do_ctx_update);
    mainw->gui_much_events = TRUE;
    fg_stack_wait();
    lives_nanosleep_while_true(mainw->do_ctx_update);

    mainw->can_switch_clips = TRUE;
    do_quick_switch(mainw->new_clip);
    mainw->can_switch_clips = FALSE;

    fg_stack_wait();
    lives_nanosleep_while_true(mainw->do_ctx_update);

    did_switch = TRUE;

    // TODO - make sure we are resetting correctly with audio lock on
    if (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
        && (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP)
        && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS)) {
      mainw->scratch = SCRATCH_JUMP;
    } else if (mainw->scratch != SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;

    if (!IS_VALID_CLIP(mainw->playing_file)) {
      if (IS_VALID_CLIP(mainw->new_clip)) goto switch_point;
      mainw->cancelled = CANCEL_INTERNAL_ERROR;
      retval = ONE_MILLION + mainw->cancelled;
      goto err_end;
    }

    sfile = mainw->files[mainw->playing_file];

    mainw->actual_frame = sfile->frameno;

    mainw->force_show = TRUE;
    fixed_frame = TRUE;

    lagged = dropped = skipped = 0;
    check_getahead = FALSE;
    bungle_frames = 0;
    recalc_bungle_frames = 0;
    mainw->deltaticks = 0;

    getahead = 0;
    test_getahead = -1;

    cdata = get_clip_cdata(mainw->playing_file);
    if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
      jumplim = cdata->jump_limit * 4;
      if (!jumplim) jumplim = JUMPFRAME_TRIGGER;
      jumplim = MIN(jumplim, JUMPFRAME_TRIGGER);
    } else jumplim = 0;

    if (sfile->last_play_sequence != mainw->play_sequence) {
      if (mainw->audio_seek_ready) sfile->last_play_sequence = mainw->play_sequence;
      sfile->last_frameno = mainw->actual_frame = sfile->frameno;
    } else {
      mainw->startticks = mainw->currticks;
      mainw->syncticks += sfile->sync_delta;
      sfile->sync_delta = 0;
      avsync_force();
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
    mainw->close_this_clip = -1;

    //if (prefs->pbq_adaptive) reset_effort();
    // TODO: add a few to bungle_frames in case of decoder unchilling

    if (mainw->record && !mainw->record_paused) mainw->rec_aclip = mainw->current_file;

    drop_off = TRUE;
    requested_frame = last_req_frame = sfile->frameno;

    /// playing file should == current_file, but just in case store separate values.
    old_current_file = mainw->current_file;
    old_playing_file = mainw->playing_file;

    cleanup_preload = TRUE;

    /* if (sfile->arate) */
    /*   g_print("seek vals: vid %d %d %ld = %f %d %f\n", sfile->last_frameno, sfile->frameno, sfile->aseek_pos, */
    /*           (double)sfile->aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
    /*           sfile->arate, sfile->fps); */

    last_time_source = time_source;
    time_source = LIVES_TIME_SOURCE_NONE;
    mainw->last_startticks = mainw->startticks = mainw->currticks
                             = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
    //g_print("SWITCH %d %d %d %d\n", sfile->frameno, requested_frame, sfile->last_frameno, mainw->actual_frame);
  }

  if (IS_VALID_CLIP(mainw->new_blend_file)) {
    if (mainw->blend_layer) {
      check_layer_ready(mainw->blend_layer);
      weed_layer_unref(mainw->blend_layer);
      mainw->blend_layer = NULL;
    }
    track_source_free(1, mainw->blend_file);
    if (IS_VALID_CLIP(mainw->new_blend_file))
      mainw->blend_file = mainw->new_blend_file;
    else
      mainw->blend_file = old_playing_file;
    if (!IS_VALID_CLIP(mainw->blend_file))
      mainw->blend_file = -1;
    mainw->new_blend_file = -1;
    mainw->blend_palette = WEED_PALETTE_END;
  }

  /// end SWITCH POINT

  // playing back an event_list
  // here we need to add mainw->offsetticks, to get the correct position when playing back in multitrack
  if (!mainw->proc_ptr && cfile->next_event) {
    // playing an event_list
    if (0) {
      // TODO -retest
#ifdef ENABLE_JACK
      /* if (mainw->scratch != SCRATCH_NONE && mainw->multitrack && mainw->jackd_trans */
      /*     && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) */
      /*     && (prefs->jack_opts & JACK_OPTS_TRANSPORT_SLAVE) */
      /*     && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) { */
      /*   // handle transport jump in multitrack : end current playback and restart it from the new position */
      /*   ticks_t transtc = q_gint64(jack_transport_get_current_ticks(mainw->jackd_trans), sfile->fps); */
      /*   mainw->multitrack->pb_start_event = get_frame_event_at(mainw->multitrack->event_list, transtc, NULL, TRUE); */
      /*   if (mainw->cancelled == CANCEL_NONE) mainw->cancelled = CANCEL_EVENT_LIST_END; */
      /* } */
#endif
    } else {
      ticks_t currticks = mainw->currticks;
      if (mainw->multitrack) currticks += mainw->offsetticks; // add the offset of playback start time
      if (currticks >= event_start) {
        // see if we are playing a selection and reached the end
        if (mainw->multitrack && mainw->multitrack->playing_sel &&
            get_event_timecode(sfile->next_event) / TICKS_PER_SECOND_DBL >=
            mainw->multitrack->region_end) mainw->cancelled = CANCEL_EVENT_LIST_END;
        else {
          weed_event_t *next_event;
          next_event = process_events(cfile->next_event, FALSE, currticks);
          // need to get this again, as process_events can switch the current clip
          cfile->next_event = next_event;
          if (!cfile->next_event) mainw->cancelled = CANCEL_EVENT_LIST_END;
        }
      }
    }

    // screen update during event playback
    if (!mainw->do_ctx_update) {
      mainw->gui_much_events = TRUE;
      mainw->do_ctx_update = TRUE;
      lives_hooks_trigger(phstacks, SYNC_ANNOUNCE_HOOK);
    }

    if (mainw->cancelled == CANCEL_NONE) return 0;

    retval = mainw->cancelled;
    goto err_end;
  }

  // free playback

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

#ifdef ENABLE_PRECACHE
      can_precache = FALSE;
#endif

      if (mainw->scratch == SCRATCH_FWD || mainw->scratch == SCRATCH_BACK
          || mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) {
        sfile->fps_scale = KEY_RPT_INTERVAL * prefs->scratchback_amount
                           * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
        if (mainw->scratch == SCRATCH_BACK || mainw->scratch == SCRATCH_BACK_EXTRA) {
          sfile->fps_scale = -sfile->fps_scale;
        }
        if (mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA)
          sfile->fps_scale = 4.;
      } else sfile->fps_scale = 1.;

      if (mainw->scratch == SCRATCH_JUMP) {
        avsync_force(); // should reset video_seek_ready
      }
      if (!mainw->video_seek_ready) {
        requested_frame = sfile->last_frameno = sfile->frameno;
        new_ticks = mainw->startticks = mainw->currticks;
        mainw->force_show = TRUE;
        fixed_frame = TRUE;
        scratch = mainw->scratch;
        mainw->scratch = SCRATCH_NONE;
      } else {
        /* g_print("PRE: %ld %ld  %d %f\n", mainw->startticks, new_ticks, sfile->last_frameno, */
        /*         (new_ticks - mainw->startticks) / TICKS_PER_SECOND_DBL * sfile->pb_fps); */
        /* sfile->frameno = 0; */
        requested_frame
          = calc_new_playback_position(mainw->playing_file, mainw->startticks, &new_ticks);
        /* g_print("POST: %ld %ld %d (%ld %d)\n", mainw->startticks, new_ticks, requested_frame, mainw->pred_frame, getahead); */

        // by default we play the requested_frame, unless it is invalid
        // if we have a pre-cached frame ready, we may play that instead
        if (new_ticks != mainw->startticks) {
#ifdef ENABLE_PRECACHE
          if (spare_cycles > 0 && last_spare_cycles > 0) can_precache = TRUE;
#endif
          update_effort(spare_cycles + 1., FALSE);
          last_spare_cycles = spare_cycles;
          spare_cycles = 0;
        } else spare_cycles++;
      }

      //g_print("VALS %ld %ld %ld and %d %d\n", new_ticks, mainw->startticks, mainw->last_startticks, requested_frame, last_req_frame);
      if (new_ticks != mainw->startticks && new_ticks != mainw->last_startticks
          && (requested_frame != last_req_frame || sfile->frames == 1
              || (mainw->playing_sel && sfile->start == sfile->end))) {
        if (mainw->fixed_fpsd <= 0. && (!mainw->vpp || mainw->vpp->fixed_fpsd <= 0. || !mainw->ext_playback)) {
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
      //g_print("%ld %ld %ld %d %d %d\n", mainw->currticks, mainw->startticks, new_ticks,
      //sfile->last_frameno, requested_frame, last_req_frame);
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

    showed_frame = FALSE;
    best_frame = -1;

    if (show_frame && sfile->delivery != LIVES_DELIVERY_PUSH) {
      // time to show a new frame
      get_proc_loads(FALSE);
      dropped = 0;

      if ((mainw->rte || (mainw->is_rendering && !mainw->event_list))
          && (mainw->current_file != mainw->scrap_file)
          && (mainw->blend_file != mainw->playing_file
              || prefs->tr_self) && !mainw->multitrack && IS_VALID_CLIP(mainw->blend_file)) {
        /// will set mainw->blend_layer
        if (prefs->dev_show_timing) g_printerr("get blend layer start  @ %f\n",
                                                 lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        get_blend_layer((weed_timecode_t)mainw->currticks);
      }

      if (mainw->frame_layer_preload && mainw->pred_frame > 0) {
        lagged = (requested_frame - mainw->pred_frame) * dir;
      } else {
        lagged = (requested_frame - mainw->actual_frame) * dir;
      }

      if (lagged < 0) lagged = 0;

      dropped = dir * (requested_frame - last_req_frame) - 1;
      if (dropped < 0) dropped = 0;

      if (!fixed_frame && !sfile->play_paused) {
        best_frame = find_best_frame(requested_frame, dropped, jumplim, dir);
      }

      if ((best_frame & 3) && dir * (requested_frame - best_frame) > 1) best_frame += dir;

      if (best_frame != -1 && !fixed_frame) sfile->frameno = best_frame;

      if (mainw->scratch != SCRATCH_NONE) {
        scratch  = mainw->scratch;
        mainw->scratch = SCRATCH_NONE;
      }

      requested_frame = clamp_frame(-1, requested_frame);
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
        if (mainw->cancelled != CANCEL_NONE) {
          retval = mainw->cancelled;
          goto err_end;
        }
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
        getahead = 0;
        test_getahead = -1;
        cleanup_preload = TRUE;
        mainw->pred_frame = 0;
        drop_off = TRUE;
      }
#endif
      if (mainw->frame_layer_preload) {
        if ((mainw->pred_frame < 0 && (-mainw->pred_frame - requested_frame) * dir < LAGFRAME_TRIGGER / 2)
            || (mainw->pred_frame && (labs(mainw->pred_frame) - mainw->actual_frame) * dir <= 0)) {
          if (mainw->pred_frame > 0) cache_misses++;
          cleanup_preload = TRUE;
        }
      }

#ifdef ENABLE_PRECACHE
      if (mainw->pred_clip == -1) {
        /// failed load, just reset
        mainw->frame_layer_preload = NULL;
        cleanup_preload = FALSE;
        mainw->pred_frame = 0;
      } else if (cleanup_preload) {
        mainw->pred_frame = 0;
        if (mainw->frame_layer_preload && is_layer_ready(mainw->frame_layer_preload)) {
          check_layer_ready(mainw->frame_layer_preload);
          weed_layer_unref(mainw->frame_layer_preload);
          mainw->frame_layer_preload = NULL;
          cleanup_preload = FALSE;
          mainw->pred_clip = -1;
        }
      }
#endif

update_effort:
      if (prefs->pbq_adaptive && scratch == SCRATCH_NONE) {
        if (requested_frame != last_req_frame || sfile->frames == 1) {
          if (sfile->frames == 1) {
            if (!spare_cycles) {
              //if (sfile->primary_src->src_type == LIVES_SRC_TYPE_FILTER) {
              //weed_plant_t *inst = (weed_plant_t *)sfile->primary_src->source;
              double target_fps = fabs(sfile->pb_fps);//weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);
              if (target_fps) {
                if (scratch == SCRATCH_NONE && mainw->inst_fps < target_fps) {
                  update_effort(target_fps / mainw->inst_fps, TRUE);
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*
          else {
            /// update the effort calculation with dropped frames and spare_cycles
            if (dropped > 0) update_effort((double)dropped / sfile->pb_fps, TRUE);
	    // *INDENT-OFF*
          }}}
      // *INDENT-ON*

      if (sfile->delivery == LIVES_DELIVERY_PUSH
          || (sfile->delivery == LIVES_DELIVERY_PUSH_PULL && mainw->force_show)) {
        show_frame = TRUE;
        goto play_frame;
      }

#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("dropped = %d, %d scyc = %ld %d %d\n", dropped, mainw->effort, spare_cycles, requested_frame, sfile->frameno);
#endif
      drop_off = FALSE;

      // THE FIRST GATE: if we are allowed to pass, then we can consider playing a frame
      // otherwise, we skip past this and add one to spare_cycles

      // this may then cause the next frame or a getahead frame to be cached instead

      if (show_frame) {
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

        // THIS IS THE SECOND GATE, we only allow passage if force_show is set, or we we have a precached frame ready

play_frame:
        //g_print("DISK PR is %f\n", mainw->disk_pressure);
        if (1) {
          boolean can_realign = FALSE;
          float cpuloadval = 0.;

          if (!mainw->force_show) {
            cpuload = get_core_loadvar(0);
            cpuloadval = *cpuload;
          }

          //g_print("CCC %d %d %d %d\n", sfile->frameno, fixed_frame, mainw->actual_frame, best_frame);

          if (!fixed_frame) {
            //if (best_frame > 0 && !is_virtual_frame(mainw->playing_file, best_frame))
            if (best_frame > 0) {
              sfile->frameno = best_frame;
            } else {
              sfile->frameno = mainw->actual_frame + dir;
            }
          }

          skipped = (sfile->frameno - mainw->actual_frame) * dir - 1;
          if (skipped < 0) skipped = 0;
          if (scratch != SCRATCH_NONE || getahead) skipped = 0;
          // MAY ALTER  mainw->scratch

          if (!fixed_frame) {
            sfile->frameno = clamp_frame(-1, sfile->frameno);
            if (mainw->cancelled != CANCEL_NONE) {
              retval = ONE_MILLION + mainw->cancelled;
              goto err_end;
            }

            if (mainw->scratch == SCRATCH_REALIGN) {
              sfile->frameno = requested_frame;
              fixed_frame = TRUE;
              scratch = mainw->scratch = SCRATCH_JUMP_NORESYNC;
            }
          }

          if (scratch == SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP;
          if (mainw->scratch == SCRATCH_JUMP) can_realign = TRUE;

          // can change in clamp_frame()
          dir = LIVES_DIRECTION_SIG(sfile->pb_fps);

          if (!check_audio_limits(mainw->playing_file, sfile->frameno)) {
            if (mainw->cancelled != CANCEL_NONE) {
              retval = ONE_MILLION + mainw->cancelled;
              goto err_end;
            }
            if (can_realign) {
              if (fixed_frame) {
                if ((sfile->frameno >= sfile->frames)
                    || (mainw->playing_sel && sfile->frameno > sfile->end)) {
                  // may happpen e.g. if we have longer audio and user clicks on timeline
                  sfile->frameno = mainw->actual_frame + dir;
                } else requested_frame = sfile->frameno;
              } else requested_frame = sfile->frameno;
            }
          } else sfile->last_frameno = requested_frame;

          if (mainw->scratch == SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;

          if (sfile->frames == 1) sfile->frameno = 1;

          /* g_print("\nPLAY %d %d %d %d %ld %ld %d %d %.4f\n", sfile->frameno, requested_frame, sfile->last_frameno, */
          /*         mainw->actual_frame, */
          /*         mainw->currticks, mainw->startticks, mainw->video_seek_ready, mainw->audio_seek_ready, cpuloadval); */

#ifndef REC_IDEAL
          can_rec = TRUE;
#endif

          if (cpuloadval < CORE_LOAD_THRESH || mainw->force_show
#ifdef ENABLE_PRECACHE
              || (mainw->pred_frame && dir * ((labs(mainw->pred_frame)) - mainw->actual_frame) >= 0
                  && dir * (labs(mainw->pred_frame) - requested_frame) <= LAGFRAME_TRIGGER / 2
                  && is_layer_ready(mainw->frame_layer_preload))
#endif
             ) {

            //g_print("lfi in  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
            lives_nanosleep_while_true(mainw->do_ctx_update);
            load_frame_image(sfile->frameno);
            //g_print("lfi out  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

            fixed_frame = FALSE;

            if (!mainw->pred_frame && mainw->frame_layer_preload) cleanup_preload = TRUE;

            if (check_getahead) {
              if ((!mainw->frame_layer_preload || is_layer_ready(mainw->frame_layer_preload))
                  && mainw->pred_frame <= 0) {
                recalc_bungle_frames = sfile->frameno;
                check_getahead = FALSE;
                if (!mainw->frame_layer_preload) {
                  if ((getahead - requested_frame) * dir >= 0) ++cache_hits;
                  else ++cache_misses;
                } else ++cache_misses;
              }
            }

            if (getahead > 0 && !mainw->frame_layer_preload) getahead = -getahead;

            if (mainw->force_show) {
#ifdef REC_IDEAL
              can_rec = TRUE;
#endif
              mainw->force_show = FALSE;
            }
          } else mainw->actual_frame = sfile->frameno;

          scratch = mainw->scratch;
          mainw->scratch = SCRATCH_NONE;
          fixed_frame = FALSE;

          mainw->inst_fps = get_inst_fps(FALSE);
          mainw->fps_mini_measure++;
          if (skipped > 0) update_effort((double)skipped / sfile->pb_fps, TRUE);
          else update_effort(mainw->inst_fps / sfile->pb_fps, FALSE);

          if (sfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->playing_file, sfile->frameno))
            sfile->last_vframe_played = get_indexed_frame(mainw->playing_file, mainw->actual_frame);

          if (prefs->show_player_stats) {
            mainw->fps_measure++;
          }

          sfile->frameno = mainw->actual_frame;
          showed_frame = TRUE;
        }
      }

      if (LIVES_IS_RECORDING) {
        boolean rec_after_pb = FALSE, rec_to_scrap = FALSE;
        int numframes, nev;
        int bg_file = (IS_VALID_CLIP(mainw->blend_file)
                       && (prefs->tr_self || (mainw->blend_file != mainw->playing_file)))
                      ? mainw->blend_file : -1;
        if ((prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
            (mainw->vpp->capabilities & VPP_CAN_RETURN)) {
          rec_after_pb = TRUE;
        }

        if (rec_after_pb || !CURRENT_CLIP_IS_NORMAL ||
            (prefs->rec_opts & REC_EFFECTS && bg_file != -1 && !IS_NORMAL_CLIP(bg_file))) {
          if (!IS_VALID_CLIP(mainw->scrap_file)) can_rec = FALSE;
          else {
            rec_to_scrap = TRUE;
            mainw->record_frame = mainw->files[mainw->scrap_file]->frames;
          }
        } else mainw->record_frame = requested_frame;

#ifdef REC_IDEAL
        if ((rec_to_scrap && showed_frame)
            || (!rec_to_scrap && (scratch != SCRATCH_NONE || new_ticks != mainw->last_startticks))) {
          can_rec = TRUE;
        }
#else
        actual_ticks = mainw->currticks;
        mainw->record_frame = mainw->actual_frame;
#endif

        if (can_rec) {
          void **eevents;
          weed_event_list_t *event_list;
          int64_t *frames;
          int *clips;
          ticks_t actual_ticks;

          int fg_file = mainw->playing_file;
          frames_t fg_frame = mainw->record_frame;
          frames_t bg_frame = (bg_file > 0 && (prefs->tr_self || (bg_file != mainw->playing_file)))
                              ? mainw->files[bg_file]->frameno : 0;

#define REC_IDEAL
#ifdef REC_IDEAL
          if (new_ticks != mainw->last_startticks) actual_ticks = new_ticks;
          else actual_ticks = mainw->currticks;
          fg_frame = mainw->record_frame;
          fg_frame = clamp_frame(mainw->playing_file, fg_frame);
#else
          fg_frame = mainw->actual_frame;
#endif
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

              if (mainw->scrap_file_size != -1) {
                weed_set_int64_value(event, WEED_LEAF_HOST_SCRAP_FILE_OFFSET, mainw->scrap_file_size);
              }

              if (!mainw->mute) {
                if (mainw->rec_aclip != -1) {
                  if (AUD_SRC_INTERNAL || (mainw->rec_aclip == mainw->ascrap_file)) {
                    if (mainw->rec_aclip == mainw->ascrap_file) {
                      mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
                                         (double)(mainw->files[mainw->ascrap_file]->arps
                                                  * mainw->files[mainw->ascrap_file]->achans *
                                                  mainw->files[mainw->ascrap_file]->asampsize >> 3);
                      mainw->rec_avel = 1.;
                    } else {
                      if (1 || !did_switch) {
                        if (!await_audio_queue(LIVES_SHORT_TIMEOUT)) {
                          mainw->cancelled = handle_audio_timeout();
                          if (mainw->cancelled != CANCEL_NONE) {
                            retval = ONE_MILLION + mainw->cancelled;
                            goto err_end;
                          }
                        }
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
                    insert_audio_event_at(event, -1, mainw->rec_aclip, mainw->rec_aseek, mainw->rec_avel);
		    // *INDENT-OFF*
		  }}}}}
	  // *INDENT-ON*
          else mainw->elist_eom = TRUE;
          pthread_mutex_unlock(&mainw->event_list_mutex);
          mainw->record_starting = FALSE;
          mainw->rec_aclip = -1;
        }
      }
      scratch = SCRATCH_NONE;
      // end load_frame_image()
    }

#ifdef ENABLE_PRECACHE
    if (mainw->frame_layer_preload) {
      if (mainw->pred_clip == mainw->playing_file) {
        if (mainw->pred_clip != mainw->current_file
            || mainw->pred_frame < 0
            || (getahead > 0 && mainw->pred_frame != getahead)) {
          cleanup_preload = TRUE;
          drop_off = FALSE;
        }
      } else cleanup_preload = TRUE;
    }
#endif

    //g_print("lfi done @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
    if (mainw->last_display_ticks == 0) mainw->last_display_ticks = mainw->clock_ticks;
    else {
      if (mainw->vpp && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.)
        mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->vpp->fixed_fpsd;
      else if (mainw->fixed_fpsd > 0.)
        mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->fixed_fpsd;
      else mainw->last_display_ticks = mainw->clock_ticks;
    }
    //} // end show_frame

#ifdef ENABLE_PRECACHE
    if (cleanup_preload) {
      if (mainw->frame_layer_preload) {
        if (getahead > 0 || is_layer_ready(mainw->frame_layer_preload)) {
          if (mainw->pred_frame == labs(getahead)) {
            getahead = -1;
          }
          if (mainw->pred_clip > 0) {
            check_layer_ready(mainw->frame_layer_preload);
            weed_layer_unref(mainw->frame_layer_preload);
          }
          mainw->frame_layer_preload = NULL;
          mainw->pred_frame = 0;
          mainw->pred_clip = 0;
          cleanup_preload = FALSE;
        }
	// *INDENT-OFF*
      }}
    // *INDENT-ON*

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

      /* time_source = LIVES_TIME_SOURCE_NONE; */
      /* xnew_ticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source); */
      /* lframe = sfile->last_frameno; */
      /* sfile->last_frameno = requested_frame; */
      /* xrequested_frame */
      /*   = calc_new_playback_position(mainw->playing_file, FALSE, new_ticks, &xnew_ticks) + LAGFRAME_TRIGGER / 2; */
      /* sfile->last_frameno = lframe; */

      delta = (requested_frame - recalc_bungle_frames);
      recalc_bungle_frames = 0;

      if (1 || prefs->dev_show_caching) {
        g_print("gah (%d) %d, act %d %d, bungle %d, shouldabeen %d %s", mainw->effort, test_getahead,
                mainw->actual_frame, requested_frame,
                bungle_frames, bungle_frames - delta, !getahead ? "(calibrating)" : "");
        if (delta < 0) g_print(" !!!!!\n");
        if (delta == 0) g_print(" EXACT\n");
        if (delta > 0) g_print(" >>>>\n");
      }
      if (delta > 0) {
        if (delta < 3 && bungle_frames > 1) bungle_frames--;
        else bungle_frames >>= 1;
      } else {
        if (delta == 0)  bungle_frames++;
        else bungle_frames -= delta;
      }
      if (bungle_frames <= -dir) bungle_frames = 0;
      if (bungle_frames > 100) bungle_frames /= 2;
    }

    if (!spare_cycles) {
      if (scratch == SCRATCH_NONE) {
        if ((!mainw->frame_layer_preload || (cleanup_preload && is_layer_ready(mainw->frame_layer_preload)))
            && getahead <= 0) {
          lives_decoder_t *dplug = NULL;
          lives_clip_data_t *cdata = NULL;
          boolean jumpframe_trigger = FALSE;
          boolean lag_trigger = FALSE;

          if (mainw->frame_layer_preload) {
            check_layer_ready(mainw->frame_layer_preload);
            weed_layer_unref(mainw->frame_layer_preload);
            mainw->frame_layer_preload = NULL;
            mainw->pred_frame = 0;
            mainw->pred_clip = 0;
          }

          dplug = get_decoder_clone(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
          if (!dplug)
            dplug = add_decoder_clone(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
          if (dplug) cdata = dplug->cdata;

          if (dropped > skipped) best_frame = mainw->actual_frame + dir * (1 + dropped);
          else best_frame = mainw->actual_frame + dir * (1 + skipped);

          // recalc this as we may have got a cached frame
          lagged = (requested_frame - mainw->actual_frame) * dir;
          if (lagged < 0) lagged = 0;

          if (lagged) best_frame += dir;

          if (dir * (best_frame - requested_frame) > LAGFRAME_TRIGGER / 2) {
            best_frame = requested_frame + LAGFRAME_TRIGGER / 2 * dir;
            if (dir * (best_frame - mainw->actual_frame) < 1) best_frame = mainw->actual_frame + dir;
          }

          if (best_frame != clamp_frame(mainw->playing_file, best_frame)) best_frame = -1;
          if (best_frame > 0) {
            if (is_virtual_frame(mainw->playing_file, best_frame)
                && (((best_frame < mainw->actual_frame &&
                      !clip_can_reverse(mainw->playing_file))
                     || (cdata && jumplim > 0 && cdata->last_frame_decoded >= 0
                         && get_indexed_frame(mainw->playing_file, best_frame)
                         - cdata->last_frame_decoded > jumplim)))) {
              //g_print("vframe jump will be %d\n", requested_frame - sfile->last_vframe_played);
              jumpframe_trigger = TRUE;
            }
          }

          if (!drop_off && lagged >= MIN(TEST_TRIGGER, LAGFRAME_TRIGGER)) lag_trigger = TRUE;
          if (jumpframe_trigger || lag_trigger) {
            g_print("TRIG %d and %d and %d %d %ld %d\n", jumpframe_trigger, lag_trigger, lagged,
                    requested_frame, mainw->pred_frame,
                    mainw->actual_frame);
            if (bungle_frames < 1) bungle_frames = 1;
            //bungle_frames += requested_frame - mainw->actual_frame - dir;

            if (xrequested_frame <= 0) {
              frames_t lframe;
              ticks_t xnew_ticks;
              time_source = LIVES_TIME_SOURCE_NONE;
              xnew_ticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, &time_source);
              lframe = sfile->last_frameno;
              sfile->last_frameno = requested_frame;
              xrequested_frame
                = calc_new_playback_position(mainw->playing_file, new_ticks, &xnew_ticks) + LAGFRAME_TRIGGER / 2;
              sfile->last_frameno = lframe;
            }

            // TRY TO PREDICT the next frame from the target using statistical method
            test_getahead = xrequested_frame + (bungle_frames + dropped) * dir;

            if (test_getahead == clamp_frame(mainw->playing_file, test_getahead)) {
              frames_t start_frame = xrequested_frame;
              while (1) {
                double targ_time = (double)(test_getahead - xrequested_frame) / sfile->pb_fps;
                double tconf = 0.5;

                frames_t bf = reachable_frame(mainw->playing_file, dplug,
                                              start_frame, test_getahead, &targ_time, &tconf);
                /* g_print("checked timings in range %d to %d, with target time <= %.4f.\n" */
                /*         "Best estimate is we can reach frame %d in %.4f sec\n", */
                /*         start_frame, test_getahead, */
                /*         (double)(test_getahead - xrequested_frame) / sfile->pb_fps, bf, targ_time); */
                if (bf == test_getahead || targ_time <= 0.) {
                  best_frame = test_getahead;
                  break;
                }
                if ((double)(bf - xrequested_frame - dir) / sfile->pb_fps >= targ_time) {
                  best_frame = bf;
                  break;
                }
                start_frame = bf + dir;
                if ((start_frame - test_getahead) * dir >= 0) {
                  best_frame = test_getahead;
                  break;
                }
              }
              mainw->pred_frame = getahead = best_frame;
              //g_print("getahead jumping to %d\n", getahead);
              check_getahead = TRUE;
	      // *INDENT-OFF*
	    }
	    else {
	      getahead = -1;
	    }}}}
      // *INDENT-ON*
#endif

      // paused
      if (LIVES_UNLIKELY(sfile->play_paused)) {
        mainw->startticks = mainw->currticks;
        if (!mainw->video_seek_ready || !mainw->audio_seek_ready) video_sync_ready();
      }

#ifdef ENABLE_PRECACHE
      if (sfile->delivery != LIVES_DELIVERY_PUSH) {
        //g_print("VA:Z %d %p\n", getahead, mainw->frame_layer_preload);
        if ((!spare_cycles && can_precache) || getahead > 0) {
          if (!mainw->multitrack && scratch == SCRATCH_NONE && IS_NORMAL_CLIP(mainw->playing_file)
              && ((!fixed_frame && mainw->pred_frame <= 0
                   && ((show_frame && !showed_frame) || can_precache))
                  || (getahead > 0 && !mainw->frame_layer_preload))) {
#ifdef SHOW_CACHE_PREDICTIONS
            //g_print("PRELOADING (%d %d %lu %p):", sfile->frameno, dropped,
            //spare_cycles, mainw->frame_layer_preload);
#endif
            best_frame = -1;

            if (!mainw->frame_layer_preload) {
              if (!mainw->preview) {
                mainw->pred_frame = 0;
                mainw->pred_clip = mainw->playing_file;
                if (getahead > 0) {
                  if (dir * (getahead - mainw->actual_frame) > 0
                      && dir * (getahead - requested_frame) > 0) {
                    best_frame = getahead;
                  } else {
                    getahead = 0;
                  }
                }

                if (best_frame == -1) best_frame = find_best_frame(requested_frame, dropped, jumplim, dir);
                if (best_frame > 0) {
                  if (best_frame != clamp_frame(mainw->playing_file, best_frame)) {
                    best_frame = -1;
                    getahead = 0;
                  }
                }
                if (best_frame > 0) {
                  const char *img_ext = get_image_ext_for_type(sfile->img_type);
                  mainw->pred_frame = best_frame;
                  mainw->pred_clip = mainw->playing_file;
                  //g_print("CACHE xx1122 %ld %d and %d\n", mainw->pred_frame, best_frame, mainw->actual_frame);

                  mainw->frame_layer_preload = lives_layer_new_for_frame(mainw->pred_clip, mainw->pred_frame);
                  if (sfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->playing_file, mainw->pred_frame)) {
                    lives_clip_src_t *dsource = get_clip_source(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
                    if (!dsource) add_decoder_clone(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
                    dsource = get_clip_source(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
                    lives_layer_set_source(mainw->frame_layer_preload, dsource);
                  }
                  //g_print("get cache start  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
                  pull_frame_threaded(mainw->frame_layer_preload, img_ext, (weed_timecode_t)mainw->currticks, 0, 0);
                  if (mainw->pred_clip != -1) {
                    if (prefs->dev_show_caching) {
                      /* double av = (double)get_aplay_offset() */
                      /* 	/ (double)sfile->arate / (double)(sfile->achans * (sfile->asampsize >> 3)); */
                      //g_print("cached frame %ld for %d %f\n", mainw->pred_frame, requested_frame, av);
                    }
		    // *INDENT-OFF*
		  }}
		else mainw->pred_frame = 0;
	      }}}}
#ifdef SHOW_CACHE_PREDICTIONS
	//g_print("frame %ld already in cache\n", mainw->pred_frame);
#endif
      }
#endif
      // *INDENT-ON*

      if (mainw->video_seek_ready) {
        last_req_frame = requested_frame;
        sfile->last_frameno = requested_frame;

        if (new_ticks > mainw->startticks) {
          mainw->last_startticks = mainw->startticks;
          mainw->startticks = new_ticks;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*
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
      if (!visible) {
        if ((xrfx = (lives_rfx_t *)mainw->vrfx_update) != NULL && fx_dialog[1]) {
          // the audio thread wants to update the parameter window
          mainw->vrfx_update = NULL;
          update_visual_params(xrfx, FALSE);
        }

        // the audio thread wants to update the parameter scroll(s)
        if (mainw->ce_thumbs) ce_thumbs_apply_rfx_changes();

        // events like fullscreen on / off are not acted on directly, instead these are stacked
        // for execution at this point. The callbacks are triggered and will pass requests to the main
        // thread. We must allow idlefunc(s) to run during this time

        if (!mainw->do_ctx_update) {
          lives_hooks_trigger(phstacks, SYNC_ANNOUNCE_HOOK);
          mainw->gui_much_events = TRUE;
          mainw->do_ctx_update = TRUE;
        }
        if (!CURRENT_CLIP_IS_VALID) {
          if (IS_VALID_CLIP(mainw->new_clip)) goto switch_point;
          mainw->cancelled = CANCEL_INTERNAL_ERROR;
        }
      }
    }
    //else proc_file = THREADVAR(proc_file) = mainw->playing_file;
    if (mainw->cancelled != CANCEL_NONE) {
      retval = ONE_MILLION + mainw->cancelled;
    } else if (!visible) goto player_loop;
    goto err_end;
  }

  if (LIVES_IS_PLAYING) {
    mainw->jack_can_stop = FALSE;
  }

  retval = MILLIONS(2) + mainw->cancelled;
  goto err_end;

  ////

err_end:
  /* if (retval) { */
  /*   if (gui_lpt) { */
  /*     lives_proc_thread_request_cancel(gui_lpt, FALSE); */
  /*     lives_proc_thread_join_boolean(gui_lpt); */
  /*     lives_proc_thread_unref(gui_lpt); */
  /*     gui_lpt = NULL; */
  /*   } */
  /* } */
  return retval;
}


boolean clip_can_reverse(int clipno) {
  if (!LIVES_IS_PLAYING || mainw->internal_messaging || mainw->is_rendering || mainw->is_processing
      || !IS_VALID_CLIP(clipno) || mainw->preview) return FALSE;
  else {
    lives_clip_t *sfile = mainw->files[clipno];
    if (sfile->clip_type == CLIP_TYPE_DISK) return TRUE;
    if (sfile->next_event) return FALSE;
    if (sfile->clip_type == CLIP_TYPE_FILE) {
      lives_clip_data_t *cdata = cdata = get_clip_cdata(clipno);
      if (!cdata || !(cdata->seek_flag & LIVES_SEEK_FAST_REV)) return FALSE;
    }
  }
  return TRUE;
}


// objects / intents

// TODO - will be part of OBJ_INTENTION_CREATE_INSTANCE (req == subtype)
lives_object_instance_t *lives_player_inst_create(uint64_t subtype) {
  char *choices[2];
  weed_plant_t *gui;
  lives_obj_attr_t *attr;
  lives_object_instance_t *inst = lives_object_instance_create(OBJECT_TYPE_PLAYER, subtype);
  inst->state = OBJECT_STATE_PREPARED;
  attr = lives_object_declare_attribute(inst, ATTR_AUDIO_SOURCE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_SOURCE, _("Source"), WEED_PARAM_INTEGER);
  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  choices[0] = lives_strdup("Internal");
  choices[1] = lives_strdup("External");
  weed_set_string_array(gui, WEED_LEAF_CHOICES, 2, choices);
  lives_free(choices[0]); lives_free(choices[1]);
  lives_object_declare_attribute(inst, ATTR_AUDIO_RATE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_RATE, _("Rate Hz"), WEED_PARAM_INTEGER);
  lives_object_declare_attribute(inst, ATTR_AUDIO_CHANNELS, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_CHANNELS, _("Channels"), WEED_PARAM_INTEGER);
  lives_object_declare_attribute(inst, ATTR_AUDIO_SAMPSIZE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_SAMPSIZE, _("Sample size (bits)"), WEED_PARAM_INTEGER);
  lives_object_declare_attribute(inst, ATTR_AUDIO_STATUS, WEED_SEED_INT64);
  lives_object_declare_attribute(inst, ATTR_AUDIO_SIGNED, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_SIGNED, _("Signed"), WEED_PARAM_SWITCH);
  attr = lives_object_declare_attribute(inst, ATTR_AUDIO_ENDIAN, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_ENDIAN, _("Endian"), WEED_PARAM_INTEGER);
  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  choices[0] = lives_strdup("Little endian");
  choices[1] = lives_strdup("Big endian");
  weed_set_string_array(gui, WEED_LEAF_CHOICES, 2, choices);
  lives_free(choices[0]); lives_free(choices[1]);
  lives_object_declare_attribute(inst, ATTR_AUDIO_FLOAT, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_FLOAT, _("Is float"), WEED_PARAM_SWITCH);
  lives_object_declare_attribute(inst, ATTR_AUDIO_INTERLEAVED, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_INTERLEAVED, _("Interleaved"), WEED_PARAM_SWITCH);
  lives_object_declare_attribute(inst, ATTR_AUDIO_DATA_LENGTH, WEED_SEED_INT64);
  lives_object_declare_attribute(inst, ATTR_AUDIO_DATA, WEED_SEED_VOIDPTR);
  return inst;
}
