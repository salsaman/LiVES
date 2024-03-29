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
#include "nodemodel.h"

#define REC_IDEAL

#define EFF_UPD_THRESH ((ticks_t)(.1 * TICKS_PER_SECOND_DBL))
#define GOOD_EFF_MULT 1
#define BAD_EFF_MULT 4.

#define ENABLE_PRECACHE

#ifdef ENABLE_PRECACHE
#define MIN_JMP_THRESH ((frames_t)(sfile->pb_fps * dir) < 16 ? 2		\
			: (frames_t)(sfile->pb_fps / 16.) + 2)
#define MAX_JMP_THRESH ((frames_t)(sfile->pb_fps / 4.) * dir + 2)
#endif

LIVES_GLOBAL_INLINE int lives_set_status(int status) {
  mainw->status |= status;
  return mainw->status;
}

LIVES_GLOBAL_INLINE int lives_unset_status(int status) {
  mainw->status &= ~status;
  return status;
}


static boolean all_updated = TRUE;

static boolean updates_done(lives_proc_thread_t self, void *unused) {
  g_main_context_iteration(NULL, FALSE);
  all_updated = TRUE;
  return TRUE;
}


void clear_player_hooks(void) {
  lives_hook_stack_t *sah =
    lives_proc_thread_get_hook_stacks(mainw->player_proc)[SYNC_ANNOUNCE_HOOK];
  lives_microsleep_while_false(!mainw->do_ctx_update && all_updated);
  fg_stack_wait();
  if (sah->stack && all_updated) {
    all_updated = FALSE;
    lives_proc_thread_add_hook(mainw->player_proc, SYNC_ANNOUNCE_HOOK, 0, updates_done, NULL);
    mainw->gui_much_events = TRUE;
    mainw->do_ctx_update = TRUE;
    lives_proc_thread_trigger_hooks(mainw->player_proc, SYNC_ANNOUNCE_HOOK);
    lives_microsleep_while_false(!mainw->do_ctx_update && all_updated);
    fg_stack_wait();
  }
}


boolean is_all_updated(void) {return all_updated;}

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

#define AV_SYNC_LIMIT 10.

lives_result_t video_sync_ready(void) {
  // CALL to indicate then video is ready, will retutn
  // LIVES_RESULT_SUCCESS, LIVES_RESULT_FAIL, LIVES_RESULT_TIMEOUT
  double xtime;

  if (mainw->foreign || !LIVES_IS_PLAYING || AUD_SRC_EXTERNAL || prefs->force_system_clock
      || (mainw->event_list && !(mainw->record || mainw->record_paused)) || prefs->audio_player == AUD_PLAYER_NONE
      || !is_realtime_aplayer(prefs->audio_player) || (CURRENT_CLIP_IS_VALID && cfile->play_paused)) {
    mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    mainw->avsync_time = 0.;
    return LIVES_RESULT_SUCCESS;
  }

  IF_APLAYER_JACK
  (if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
  && !mainw->record_paused && mainw->jackd->is_paused)) {
  mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
  return LIVES_RESULT_SUCCESS;
})

  IF_APLAYER_PULSE
  (if (LIVES_UNLIKELY(mainw->event_list && LIVES_IS_PLAYING && !mainw->record
  && !mainw->record_paused && mainw->pulsed->is_paused)) {
  mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
  mainw->avsync_time = 0.;
  return LIVES_RESULT_SUCCESS;
})

  if (mainw->audio_seek_ready) {
    mainw->video_seek_ready = TRUE;
    mainw->avsync_time = 0.;
    return LIVES_RESULT_SUCCESS;
  }

  xtime = (lives_get_session_time() - mainw->avsync_time) / ONE_BILLION_DBL;
  if (xtime > AV_SYNC_LIMIT) {
    mainw->sync_err = xtime;
    return LIVES_RESULT_TIMEDOUT;
  }

  /* mainw->startticks -= lives_get_current_playback_ticks(mainw->origticks, NULL); */
  /* lives_microsleep_while_false(mainw->audio_seek_ready); */
  /* mainw->startticks += lives_get_current_playback_ticks(mainw->origticks, NULL); */

  /* mainw->fps_mini_measure = 0; */
  /* mainw->fps_mini_ticks = lives_get_session_ticks(); */
  /* mainw->last_startticks = mainw->startticks; */
  return LIVES_RESULT_FAIL;
}


void track_source_free(int i, int oclip) {
  // i here is track number, oclip will (should) be the clip which was playing on that track
  // (if i is 0, then we look up the clip which was playing)
  // if the source for the track is a TRACK source, we remove it, otherwise (Primary src)
  // set it to INACTIVE (removing the_source from the clip may also free it)
  // we invalidate any layer attached to the source
  // set track_sources[i] to NULL, and active / old_active tracks are set to zero

  // - update we now have clipsrc_gorups attached to tracks, so
  // when clip for a track changes we flag the srcgrp as idle
  //  then if we need a new srcgrp any idle ones get added first
  //
  // we will free / remove the srcgrp unless it is the primary group
  // but we dont free until all clip_idx are checked

  // this MUST be done BEFORE closing the clip, or removing the source

  // step 1, mark unused as IDLE
  if (i >= 0 && i < MAX_TRACKS) {
    if (mainw->track_sources[i] && (oclip == 0 || mainw->active_track_list[i] == oclip
                                    || (mainw->old_active_track_list[i] == oclip
                                        && mainw->active_track_list[i] != oclip))) {
      if (!oclip) oclip = mainw->old_active_track_list[i];
      if (IS_VALID_CLIP(oclip)) {
        lives_clipsrc_group_t *srcgrp = mainw->track_sources[i];
        if (srcgrp) {
          // need to set current layer if possible
          weed_layer_t *layer = srcgrp->layer;
          if (layer) weed_layer_set_invalid(layer, TRUE);
          if (srcgrp->purpose == SRC_PURPOSE_TRACK)
            srcgrp_remove(oclip, i, SRC_PURPOSE_TRACK);
          else {
            lives_clip_t *sfile;
            srcgrp->status = SRC_STATUS_IDLE;
            srcgrp->track = -1;
            sfile = RETURN_NORMAL_CLIP(oclip);
            if (sfile) {
              if (get_primary_src_type(sfile) == LIVES_SRC_TYPE_DECODER) {
                if (!mainw->multitrack) {
                  chill_decoder_plugin(oclip); /// free buffers to relesae memory
		  // *INDENT-OFF*
		}}}}}}
      // *INDENT-ON*
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


static lives_layer_t *check_for_overlay_text(weed_layer_t *layer) {
  lives_layer_t *xlayer = NULL;
  if (mainw->urgency_msg && prefs->show_urgency_msgs) {
    if (lives_sys_alarm_triggered(urgent_msg_timeout)) {
      lives_sys_alarm_disarm(urgent_msg_timeout, TRUE);
      lives_freep((void **)&mainw->urgency_msg);
      goto done;
    }
    if (layer == mainw->frame_layer) xlayer = weed_layer_copy(NULL, layer);
    else xlayer = layer;
    render_text_overlay(xlayer, mainw->urgency_msg, DEF_OVERLAY_SCALING);
    goto done;
  }

  if ((mainw->overlay_msg && prefs->show_overlay_msgs) || mainw->lockstats) {
    if (mainw->lockstats) {
      lives_freep((void **)&mainw->overlay_msg);
      show_sync_callback(NULL, NULL, 0, 0, LIVES_INT_TO_POINTER(1));
      if (mainw->overlay_msg) {
        if (layer == mainw->frame_layer) xlayer = weed_layer_copy(NULL, layer);
        else xlayer = layer;
        render_text_overlay(xlayer, mainw->overlay_msg, DEF_OVERLAY_SCALING);
        if (prefs->render_overlay && mainw->record && !mainw->record_paused) {
          weed_plant_t *event = get_last_frame_event(mainw->event_list);
          if (event) weed_set_string_value(event, WEED_LEAF_OVERLAY_TEXT, mainw->overlay_msg);
        }
      }
      goto done;
    } else {
      if (!mainw->preview_rendering) {
        if (lives_sys_alarm_triggered(overlay_msg_timeout)) {
          lives_sys_alarm_disarm(overlay_msg_timeout, TRUE);
          lives_freep((void **)&mainw->overlay_msg);
          goto done;
        }
        if (layer == mainw->frame_layer) xlayer = weed_layer_copy(NULL, layer);
        else xlayer = layer;
        render_text_overlay(xlayer, mainw->overlay_msg, DEF_OVERLAY_SCALING);
        if (mainw->preview_rendering) lives_freep((void **)&mainw->overlay_msg);
      }
      goto done;
    }
  }
done:
  return xlayer ? xlayer : layer;
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
static const char *img_ext = NULL;

// this function readies the mainw->layers array for the next frame(s)
// if playing from the same exec_plan (but a new plan cycle), the function should be called
// with map_only set to TRUE
// If the nodemodel is being rebuilt, then this function should be called with map_only
// set to FALSE. This will update active_track_list and track_srcs

// during playback, for clip editor there are currently only 2 tracks: fg and bg
// mainw->frame_layer maps to fg and mainw->playing_file and track 0 / layers[0]
// in multitrack mode there can be any number of layers in the stack, the clip_index will be read from a FRAME event
// If same clip appears a multiple tracks, new clipsrc_group will be created for each additional instance.
//
// We create the mainw->layers array for plan_cycles, and clip_index for nodemodel.
// When creating the layers array we do make NULL layers. There are 3 ways that layers can be loaded
// automatic - simply set frame_idx in the plan_cycle and the plan runner will take responsibility for loading it
// semi auto - create the layer, set frame number and set layer status to PREPARED
// fully manual - load the frame and set the layer status to LOADED

weed_layer_t **map_sources_to_tracks(boolean rndr, boolean map_only) {
  static int onum_tracks = 0;
  lives_clipsrc_group_t *srcgrp;
  weed_layer_t **layers = mainw->layers;
  int oclip, nclip, i, j;

  if (map_only) {
    if (layers) {
      for (i = 0; i < mainw->num_tracks; i++) layers[i] = NULL;
      return layers;
    }
    return LIVES_CALLOC_SIZEOF(weed_layer_t *, mainw->num_tracks);
  }

  if (mainw->layers) {
    for (i = 0; i < mainw->num_tracks; i++) {
      if (mainw->layers[i])
        weed_layer_unref(STEAL_POINTER(mainw->layers[i]));
    }
    lives_free(STEAL_POINTER(mainw->layers));
  }

  if (!rndr) {
    // non rendering - ie normal playback
    if (!mainw->multitrack) {
      // clip editor mode
      lives_freep((void **)&mainw->clip_index);
      lives_freep((void **)&mainw->frame_index);
      if (mainw->num_tr_applied && IS_VALID_CLIP(mainw->blend_file)
          && (mainw->blend_file != mainw->playing_file || prefs->tr_self)) {
        // if a transition effect is active we have 2 tracks
        mainw->num_tracks = 2;
        mainw->clip_index = (int *)lives_calloc(2, sizint);
        mainw->clip_index[1] = mainw->blend_file;
      } else {
        // otherwise only 1 track
        mainw->num_tracks = 1;
        mainw->clip_index = (int *)lives_calloc(1, sizint);
      }
      mainw->clip_index[0] = mainw->playing_file;
    } else {
      // in multitrack mode, we can have any number 0 -> MAX_TRACKS
      // (we can also separated backing audio tracks, but these are not represented here)
      // get list of active tracks from mainw->filter map
      // for multitrack, the most recent filter_map defines how the layers are combined;
      // if there is no active filter map then we only see the frontmost layer
      // The mapping of clips / frames at the current playback time is held in clip_index / frame_index
      // these are set from Frame events
      get_active_track_list(mainw->clip_index, mainw->num_tracks, mainw->filter_map);
    }
    // when rendering we only care about number of tracks
  }

  if (mainw->num_tracks < onum_tracks) {
    for (i = mainw->num_tracks; i < onum_tracks; i++) {
      if (mainw->track_sources[i]) track_source_free(i, 0);
    }
  }

  onum_tracks = mainw->num_tracks;

  for (i = 0; i < mainw->num_tracks; i++) {
    oclip = mainw->old_active_track_list[i] = mainw->active_track_list[i];
    nclip = mainw->active_track_list[i] = mainw->clip_index[i];
  }

  // here we compare the mapping of clips -> tracks with the previous values
  // if the clip mapping for a track has changed we have to note this
  // and invalidate the track_source for that track. However, we only create new track sources
  // once we have determined that the track is visible in the output layer, otherwise the track can
  // be ignored for now.

  // do 2 passes, first if oclip changed, mark the srcgrp as idle
  // then pass 2, set or create new srcgrps
  // if we have an idle srcgrp, we use that
  // then finally any still idle sources get freed, unless they are flagged with nofree (e.g primary srcgrp)

  for (i = 0; i < mainw->num_tracks; i++) {
    oclip = mainw->old_active_track_list[i];
    if (oclip > 0) {
      nclip = mainw->active_track_list[i];
      if (nclip != oclip) {
        srcgrp = mainw->track_sources[i];
        if (srcgrp) {
          srcgrp->status = SRC_STATUS_IDLE;
          srcgrp->track = -1;
          mainw->track_sources[i] = NULL;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  for (i = 0; i < mainw->num_tracks; i++) {
    nclip = mainw->active_track_list[i];
    if (nclip > 0) {
      if (!mainw->track_sources[i]) {
        lives_clip_t *sfile = RETURN_VALID_CLIP(nclip);
        if (sfile) {
          srcgrp = NULL;
          // find_a clipgrp for this track
          // start by checking for IDLE clipsrcs, by prefenrence, PRIMARY, then we will just reuse this
          // else we must create a clone srcgrp
          for (j = 0; j < sfile->n_src_groups; j++) {
            if (sfile->src_groups[j] && sfile->src_groups[j]->status == SRC_STATUS_IDLE) {
              srcgrp = sfile->src_groups[j];
              if (srcgrp->purpose == SRC_PURPOSE_PRIMARY) break;
            }
          }
          if (!srcgrp) {
            for (j = 0; j < sfile->n_src_groups; j++) {
              if (sfile->src_groups[j] && sfile->src_groups[j]->status == SRC_STATUS_IDLE) {
                srcgrp = sfile->src_groups[j];
                if (srcgrp->purpose == SRC_PURPOSE_TRACK) break;
              }
            }
          }

          /// TODO... (sync_locked tracks)
          // as an option, if we have two or more tracks for the same clip, and we want them in sync
          // (all loading the same frame), we can create a dummy sourcegroup with no srcs,
          // but with its own apparent_pal, apparent_gamma. If we use this as a track_Source, then
          // it can be added as an output clone for the node representing the original track_source,
          // and when the frame is loaded for the original, the clip_src will fill pixel_data,
          // and this will then be converted to srcgroup apparent pal/ gamm, avoiding the need to
          // load the frame multiple times
          if (!srcgrp)
            srcgrp = clone_srcgrp(nclip, nclip, i, SRC_PURPOSE_TRACK);
          if (srcgrp) {
            srcgrp->status = SRC_STATUS_READY;
            mainw->track_sources[i] = srcgrp;
            srcgrp->track = i;
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*

  for (i = 0; i < mainw->num_tracks; i++) {
    // remove / free any track srcgrps which still have status IDLE
    oclip = mainw->old_active_track_list[i];
    if (oclip > 0) {
      nclip = mainw->active_track_list[i];
      if (nclip != oclip) {
        lives_clip_t *sfile = RETURN_VALID_CLIP(oclip);
        if (sfile) {
          for (j = 0; j < sfile->n_src_groups; j++) {
            srcgrp = sfile->src_groups[j];
            if (srcgrp->purpose != SRC_PURPOSE_TRACK) continue;
            if (srcgrp && srcgrp->status == SRC_STATUS_IDLE) {
              srcgrp_remove(oclip, srcgrp->track, SRC_PURPOSE_TRACK);
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-ON*
  }
  if (!rndr && !mainw->multitrack && mainw->blend_file == mainw->playing_file) {
    // in clip edit mode, we can sometimes end up with track 0  being a track srcgroup
    // and track 1 being the primary source,  We need to swap these otherwise precaching will not
    // function correctly
    if (mainw->track_sources[0]->purpose == SRC_PURPOSE_TRACK
        && mainw->track_sources[1] == SRC_PURPOSE_PRIMARY) {
      swap_srcgrps(mainw->playing_file, 1, SRC_PURPOSE_PRIMARY, 0, SRC_PURPOSE_TRACK);
    }
  }

  /* if (rndr || mainw->multitrack) { */
  /*   // for rendering or multitrack we create the layers - for clip editor the layers are created on the fly */
  /*   // this allows for the layer to be switched with a precached layer if one is ready */
  /*   for (i = 0; i < mainw->num_tracks; i++) { */
  /*     nclip = mainw->active_track_list[i]; */
  /*     if (nclip > 0) { */
  /*       // TODO - create blanks if clip < 0 */
  /*       layers[i] = lives_layer_new_for_frame(nclip, mainw->frame_index[i]); */
  /*       lives_layer_set_track(layers[i], i); */
  /*       lives_layer_set_srcgrp(layers[i], mainw->track_sources[i]); */
  /*       lives_layer_set_status(layers[i], LAYER_STATUS_PREPARED); */
  /*       /\* if (rndr) { *\/ */
  /*       /\* 	weed_layer_set_palette(layers[i], (mainw->clip_index[i] == -1 || *\/ */
  /*       /\* 					   mainw->files[nclip]->img_type == *\/ */
  /*       /\* 					   IMG_TYPE_JPEG) ? WEED_PALETTE_RGB24 : WEED_PALETTE_RGBA32); *\/ */
  /*       /\* } *\/ */
  /*     } */
  /*   } */
  /*   layers[i] = NULL; */
  /* } */
  return NULL;
}


static lives_result_t prepare_frames(frames_t frame) {
  // here we prepare the layers in mainw->layers so they can be loaded
  // mostly this involves checking for precached frames for the player in clip editor mode
  lives_clip_t *sfile = mainw->files[mainw->playing_file];
  boolean rndr = FALSE;
  int bad_frame_count = 0;
  int retval;

  mainw->frame_layer = NULL;

  if (mainw->preview && (!mainw->event_list || cfile->opening)) {
    info_file = lives_build_filename(prefs->workdir, cfile->handle, LIVES_STATUS_FILE_NAME, NULL);
  }

  if ((mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)))
    rndr = TRUE;

  // here if we are rendering from multitrack, previewing a recording, or applying realtime effects to a selection
  //if (mainw->is_rendering && !(mainw->proc_ptr && mainw->preview)) {
  if (rndr && mainw->scrap_file != -1 && mainw->clip_index[0] == mainw->scrap_file && mainw->num_tracks == 1) {
    // do nothing
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

    if (rndr) {
      // if rendering, set timecode for all layers
      for (int i = 0; mainw->layers[i]; i++) {
        ticks_t *timing_data;
        lock_layer_status(mainw->layers[i]);
        timing_data = _get_layer_timing(mainw->layers[i]);
        timing_data[LAYER_STATUS_TREF] = mainw->cevent_tc;
        weed_layer_ref(mainw->layers[i]);
        _set_layer_timing(mainw->layers[i], timing_data);
        _lives_layer_set_status(mainw->layers[i], LAYER_STATUS_PREPARED);
        unlock_layer_status(mainw->layers[i]);
        lives_free(timing_data);
      }
      // end RENDERING
    } else {
      // normal playback in the clip editor, or applying a non-realtime effect
      if (!mainw->preview || lives_file_test(fname_next, LIVES_FILE_TEST_EXISTS)) {
        if (!img_ext) img_ext = get_image_ext_for_type(cfile->img_type);
        if (mainw->preview && !mainw->frame_layer
            && (!mainw->event_list || cfile->opening)) {
          // external frame manipulation preview
          // TODO - use imgdec clipsrc
          if (!pull_frame_at_size(mainw->frame_layer, img_ext, (weed_timecode_t)mainw->currticks,
                                  cfile->hsize, cfile->vsize, WEED_PALETTE_ANY)) {
            if (mainw->frame_layer)
              weed_layer_unref(STEAL_POINTER(mainw->frame_layer));

            if (cfile->clip_type == CLIP_TYPE_DISK &&
                cfile->opening && cfile->img_type == IMG_TYPE_PNG
                && sget_file_size(fname_next) <= 0) {
              if (++bad_frame_count > BFC_LIMIT) {
                mainw->cancelled = check_for_bad_ffmpeg();
                bad_frame_count = 0;
              } else lives_usleep(prefs->sleep_time);
            }
          } else mainw->layers[0] = mainw->frame_layer;
        } else {
          // no prev - realtime pb
          // if we have anything we can use, set mainw->frame_layer
          // check first if got handed an external layer to play
          // if so, just copy it (deep) to mainw->frame_layer

          int dir = sig(sfile->pb_fps);
          frames_t delta_a = 0, delta_l = 0, delta_r = 0;

          if (mainw->ext_layer) {
            if (!mainw->layers[0]) mainw->layers[0] = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
            weed_layer_copy(mainw->layers[0], mainw->ext_layer);
            mainw->frame_layer = mainw->layers[0];
            weed_layer_unref(STEAL_POINTER(mainw->ext_layer));
            lives_layer_set_status(mainw->frame_layer, LAYER_STATUS_LOADED);
            goto skip_precache;
          }
          if (!IS_PHYSICAL_CLIP(mainw->playing_file)) {
            mainw->frame_layer = mainw->layers[0];
            return LIVES_RESULT_SUCCESS;
          }
#ifdef ENABLE_PRECACHE
          // then check if we a have preloaded (cached) frame
          // there are two ways we an get a preload - either normal playback when we have spare cycles
          // or when we are lagging and trying to jump ahead
          // the frame number is in mainw->pred_frame
          // once the frame is loaded (is_layer_ready returns TRUE), we check it is within range
          // if it is behind mainw->actual_frame, then we discard it (set mainw->pred_frame == 0)
          // otherwise, we may use it
          // we may have up to 4 frame numbers: last frame played, current frame, pred_frame, cached frame, requested frame
          // (example for forwards playback)
          // if we are ahead of req frame thwn we only play cached or pred if they are 'close', if cached < pred, play cached and cache pred
          // othwequse, play pred

          // close == between min jmp and max
          frames_t ccframe = 0, lframe = sfile->last_frameno, rframe = sfile->last_req_frame, pframe = 0;
          boolean evict_cache = FALSE, use_cache = FALSE, cache_pre = FALSE;
          if (mainw->cached_frame) {
            int cclip = lives_layer_get_clip(mainw->cached_frame);
            if (cclip == mainw->playing_file) {
              ccframe = lives_layer_get_frame(mainw->cached_frame);
              if (dir * (ccframe - lframe) < 0) evict_cache = TRUE;
            } else evict_cache = TRUE;
          }

          if (mainw->pred_frame && mainw->frame_layer_preload) {
            if (!weed_layer_check_valid(mainw->frame_layer_preload) || mainw->pred_clip != mainw->playing_file) {
              goto no_precache;
            }

            if (is_layer_ready(mainw->frame_layer_preload) != LIVES_RESULT_SUCCESS) {
              goto no_precache;
            }

            pframe = mainw->pred_frame;
            delta_a = (pframe - rframe) * dir;
            delta_l = (pframe - lframe) * dir;
            delta_r = (lframe - rframe) * dir;

            d_print_debug("got ypreload, deltas are %d and %d\n", delta_a, delta_l);
            d_print_debug("THANKS for %p,! %d %ld, real was %d, range %d  --  %d\n",
                          mainw->frame_layer_preload, mainw->pred_clip,
                          pframe, rframe,
                          rframe + MIN_JMP_THRESH * dir,
                          rframe + MAX_JMP_THRESH * dir);

            if (delta_l <= 0) {
              // if before last_frame, or invalid,  we must discard
              //g_print("preload frame too early, must discard %d %d %d\n", pframe, lframe, rframe);
              weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);
              cache_misses++;
              pframe = 0;
              goto no_precache;
            }

            //got_pc = TRUE;

            if (delta_a > MAX_JMP_THRESH || delta_r > MIN_JMP_THRESH) {
              // too far in future
              pframe = 0;
              g_print("preload frame too far ahead, may cache for later %d %d %d\n", pframe, lframe, rframe);
              g_print("cached frame in use, can we release it now ?\n'");
              cache_misses++;
              if (!ccframe) cache_pre = TRUE;
              else {
                if ((pframe - ccframe) * dir > 0 &&
                    (ccframe - lframe) * dir < MAX_JMP_THRESH) {
                  use_cache = TRUE;
                  cache_pre = TRUE;
                }
              }
            }
          }
no_precache:
          if (!pframe) {
            if (ccframe && (ccframe - lframe) * dir > 0 && (ccframe - rframe) * dir <= MIN_JMP_THRESH) {
              use_cache = TRUE;
            }
          }
          if (use_cache) {
            if (!mainw->layers[0]) mainw->layers[0] =
                lives_layer_new_for_frame(lives_layer_get_clip(mainw->cached_frame),
                                          lives_layer_get_frame(mainw->cached_frame));

            weed_layer_copy(mainw->layers[0], mainw->cached_frame);
            ///
            weed_layer_unref(STEAL_POINTER(mainw->cached_frame));
            ///
            mainw->frame_layer = mainw->layers[0];
            mainw->actual_frame = pframe;
            mainw->frame_layer = mainw->layers[0];
            mainw->actual_frame = ccframe;
          }

          if (evict_cache)
            weed_layer_unref(STEAL_POINTER(mainw->cached_frame));

          if (cache_pre) {
            mainw->cached_frame = STEAL_POINTER(mainw->frame_layer_preload);
            pframe = 0;
          }

          if (use_cache) lives_layer_set_status(mainw->frame_layer, LAYER_STATUS_LOADED);

          if (pframe) {
            if (delta_a < MIN_JMP_THRESH) {
              // if before last_frame, or invalid,  we must discard
              d_print_debug("preload frame too early, but OK to use\n");
            }

            if (delta_a <= MAX_JMP_THRESH) {
              cache_hits++;

              if (!mainw->layers[0]) mainw->layers[0] =
                  lives_layer_new_for_frame(lives_layer_get_clip(mainw->frame_layer_preload),
                                            lives_layer_get_frame(mainw->frame_layer_preload));
              weed_layer_copy(mainw->layers[0], mainw->frame_layer_preload);
              ///
              weed_layer_unref(STEAL_POINTER(mainw->frame_layer_preload));
              ///
              mainw->frame_layer = mainw->layers[0];
              mainw->actual_frame = pframe;

              //if (delta_a > MIN_JMP_THRESH) {
              lives_clipsrc_group_t *srcgrp = get_primary_srcgrp(mainw->playing_file);
              if (srcgrp) {
                srcgrp->layer = NULL;
                swap_srcgrps(mainw->playing_file, -1, SRC_PURPOSE_PRIMARY, 0, SRC_PURPOSE_PRECACHE);
                srcgrp = get_primary_srcgrp(mainw->playing_file);
                lives_layer_set_srcgrp(mainw->frame_layer, srcgrp);
              }
              //}
              lives_layer_set_status(mainw->frame_layer, LAYER_STATUS_LOADED);
            }	      // depending on frame value we either make a deep or shallow copy of the cache frame
            /* else { */
            /*   // if pre frame is too far ahead, we will cache it, and continue playing normally */
            /*   // unti we are in range */
            /*   cache_misses++; */
            /*   if (mainw->cached_frame) { */
            /* 	if (mainw->cached_frame != get_old_frame_layer()) */
            /* 	  weed_layer_unref(mainw->cached_frame); */
            /*   } */
            /*   mainw->cached_frame = mainw->frame_layer_preload; */
            /*   mainw->frame_layer_preload = NULL; */
            /* } */
          }
        }
      }
#else
          if (1) {
#endif
#ifdef IGNORE_THIS
    }    // }
#endif
    //MSGMODE_OFF(DEBUG);
skip_precache:
    if (!mainw->frame_layer) {
      if (mainw->plan_cycle) mainw->plan_cycle->frame_idx[0] = frame;
      //lives_layer_set_status(mainw->layers[0], LAYER_STATUS_PREPARED);
    }
  }

  if ((!cfile->next_event && mainw->is_rendering && !mainw->clip_switched &&
       (!mainw->multitrack || (!mainw->multitrack->is_rendering && !mainw->is_generating))) ||
      ((!mainw->multitrack || (mainw->multitrack && mainw->multitrack->is_rendering)) &&
       mainw->preview && !mainw->frame_layer)) {
    // preview ended
    if (!cfile->opening) mainw->cancelled = CANCEL_NO_MORE_PREVIEW;
    if (mainw->cancelled) {
      lives_free(fname_next);
      lives_freep((void **)&info_file);
      return LIVES_RESULT_ERROR;
    }
    // in case we are opening via non-instant means. We keep trying until the next frame appears.
    mainw->currticks = lives_get_current_playback_ticks(mainw->origticks, NULL);
  }

  img_ext = NULL;

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
      return LIVES_RESULT_FAIL;
      // *INDENT-OFF*
    }}}
  // *INDENT-ON*

if (!mainw->layers) return LIVES_RESULT_ERROR;
return LIVES_RESULT_SUCCESS;
}


static weed_layer_t *old_frame_layer = NULL;

weed_layer_t *get_old_frame_layer(void) {return mainw->layers ? old_frame_layer : NULL;}

void reset_old_frame_layer(void) {
  weed_layer_t *layer = (weed_layer_t *)STEAL_POINTER(old_frame_layer);
  if (layer) {
    if (layer != mainw->cached_frame && layer != mainw->frame_layer_preload) {
      weed_layer_unref(layer);
      if (layer == mainw->frame_layer && (!mainw->ext_player_layer ||
                                          mainw->ext_player_layer == mainw->frame_layer)) mainw->frame_layer = NULL;
      if (mainw->layers && layer == mainw->layers[0]) mainw->layers[0] = NULL;
    }
  }
}


static boolean vpp_processed_flag = FALSE;

void reset_ext_player_layer(boolean ign_flag) {
  weed_layer_t *layer = STEAL_POINTER(mainw->ext_player_layer);
  if (layer) {
    if (!ign_flag) {
      lives_microsleep_while_false(vpp_processed_flag);
      vpp_processed_flag = FALSE;
    }
    weed_layer_unref(layer);
  }
}


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

  char *tmp, *osc_sync_msg = NULL;

  boolean was_preview = FALSE;
  boolean rec_after_pb = FALSE;
  boolean success = TRUE;
  boolean player_v2 = FALSE;

  lives_result_t res;
  int pwidth, pheight;
  int lb_width = 0, lb_height = 0;
  int fg_file = mainw->playing_file;
  int errpt = 0;

  LIVES_ASSERT(lives_proc_thread_get_hook_stacks(mainw->player_proc) != NULL);

  framecount = NULL;
  fname_next = info_file = NULL;
  img_ext = NULL;

  mainw->scratch = SCRATCH_NONE;

  mainw->frame_layer = NULL;

  if (LIVES_UNLIKELY(cfile->frames == 0 && !mainw->foreign && !mainw->is_rendering
                     && AUD_SRC_INTERNAL)) {
    // playing a clip with zero frames, but we can record audio
    if (mainw->record && !mainw->record_paused) {
      // add blank frame
      weed_plant_t *event = get_last_event(mainw->event_list);
      weed_plant_t *event_list = insert_blank_frame_event_at(mainw->event_list,
                                 lives_get_relative_ticks(mainw->origticks), &event);
      if (!mainw->event_list) mainw->event_list = event_list;
      if (mainw->rec_aclip != -1 && (prefs->rec_opts & REC_AUDIO) && !mainw->record_starting) {
        // we are recording, and the audio clip changed; add audio event
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
    // if autotransitioning from one clip to another, continue smooth transition
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
        int bg_file = (IS_VALID_CLIP(mainw->blend_file)
                       && (prefs->tr_self || (mainw->blend_file != mainw->playing_file)))
                      ? mainw->blend_file : -1;

        // should we record the output from the playback plugin ?
        if (mainw->record && (prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
            (mainw->vpp->capabilities & VPP_CAN_RETURN)) {
          rec_after_pb = TRUE;
        }

        if (rec_after_pb || !CURRENT_CLIP_IS_NORMAL ||
            (prefs->rec_opts & REC_EFFECTS && bg_file != -1 && !IS_NORMAL_CLIP(bg_file))) {
          // recording and we want to record post pb return, or fg or bg is not a normal clip
          // (e.g webcam / generator), we will save the frame to disk
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
      } else if (mainw->toy_type != LIVES_TOY_NONE) play_toy();
    }

    if (was_preview) {
      // preview - this is slightly different, instead of pulling frames via clip_srcs,
      // we will load image files as they are produced by a plugin
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
        errpt = 20;
        goto lfi_err;
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
        errpt = 1;
        goto lfi_err;
      }
    }

    /////////////////// TRIGGER PLAN CYCLE //////////////////////////////

    do {
      // if we are playing a backend preview, we may need to call this several times until the
      // backend frame is rendered
      // otherwise this will pull the foreground frame (or more likely, start a thread to do this)
      res = prepare_frames(frame);
    } while (res == LIVES_RESULT_FAIL && !mainw->frame_layer && mainw->cancelled == CANCEL_NONE
             && cfile->clip_type == CLIP_TYPE_DISK);

    // TODO -> mainw->cancelled should cancel plan_cycle
    if (LIVES_UNLIKELY((mainw->cancelled != CANCEL_NONE))) {
      // NULL frame or user cancelled
      errpt = 3;
      goto lfi_err;
    }

    if (!mainw->layers) {
      errpt = 2;
      goto lfi_err;
    }

    //MSGMODE_ON(DEBUG);

    if (was_preview) lives_free(fname_next);

    ///////// EXECUTE PLAN CYCLE ////////////

    //d_print_debug("wating for plan to complete\n");
    if (mainw->plan_cycle) {
      if (mainw->plan_cycle->state == PLAN_STATE_QUEUED
          || mainw->plan_cycle->state == PLAN_STATE_WAITING) {
        plan_cycle_trigger(mainw->plan_cycle);
      }
      mainw->plan_cycle->tdata->actual_start = lives_get_session_time();
    }

    if (!mainw->multitrack &&
        !mainw->faded && (!mainw->fs || (prefs->play_monitor != 0 && prefs->play_monitor != widget_opts.monitor + 1))
        && mainw->current_file != mainw->scrap_file) {
      THREADVAR(hook_hints) = HOOK_CB_PRIORITY;
      main_thread_execute_rvoid(paint_tl_cursors, 0, "vvv", mainw->eventbox2, NULL, mainw->eb2_psurf);
      THREADVAR(hook_hints) = 0;
    }

    /* in render frame, we would have set all frames to either prepared or loaded */
    /* so the plan runner should have started loading them already */
    /* the reamining steps will be run, applying all fx instances until we are left with the single output layer */
    lives_millisleep_while_false(!mainw->plan_cycle || mainw->plan_cycle->state == PLAN_STATE_COMPLETE
                                 || mainw->plan_cycle->state == PLAN_STATE_CANCELLED
                                 || mainw->plan_cycle->state == PLAN_STATE_ERROR
                                 || mainw->cancelled != CANCEL_NONE);

    if (mainw->plan_runner_proc) {
      lives_proc_thread_join(mainw->plan_runner_proc);
      mainw->plan_runner_proc = NULL;
    }

    if (mainw->layers && mainw->layers[0]) mainw->frame_layer = mainw->layers[0];

    if (mainw->refresh_model) {
      errpt = 7;
      goto lfi_err;
    }

    //
    if (!mainw->plan_cycle) {
      errpt = 11;
      goto lfi_err;
    }

    if (!mainw->layers || !mainw->layers[0]) {
      if (!mainw->layers) g_print("no layers !\n");
      else {
        if (!mainw->layers[0]) g_print("no layer[0] !\n");
        if (mainw->plan_cycle) {
          g_print("req frame %ld\n", mainw->plan_cycle->frame_idx[0]);
          g_print("plan state is %lu\n", mainw->plan_cycle->state);
        } else g_print("no plan cycle !\n");
      }
    }

    if (lives_layer_get_status(mainw->frame_layer) != LAYER_STATUS_READY) {
      if (!(mainw->plan_cycle->state == PLAN_STATE_CANCELLED
            || mainw->plan_cycle->state == PLAN_STATE_ERROR)) {
        mainw->plan_cycle->state = PLAN_STATE_ERROR;
        g_print("bad layer status %d\n", lives_layer_get_status(mainw->frame_layer));
      }
    }

    //d_print_debug("plan done\n");

    if (mainw->plan_cycle->state == PLAN_STATE_CANCELLED
        || mainw->plan_cycle->state == PLAN_STATE_ERROR) {
      if (mainw->plan_cycle->state == PLAN_STATE_CANCELLED)
        g_print("plan CANCELLED during execution\n");
      else g_print("plan ERROR during execution\n");
      errpt = 4;
      goto lfi_err;
    }

    if (lives_layer_get_clip(mainw->frame_layer) != mainw->playing_file) {
      g_print("frame layer clip mismatch, clip was %d and we are playing %d\n",
              lives_layer_get_clip(mainw->frame_layer), mainw->playing_file);
      mainw->plan_cycle->state = PLAN_STATE_ERROR;
      errpt = 5;
      goto lfi_err;
    }

    // chain any data to the playback plugin
    if (!(mainw->preview || mainw->is_rendering)) {
      // chain any data pipelines
      int64_t old_rte = mainw->rte;
      for (int i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
        if (rte_key_valid(i + 1, TRUE)) {
          pconx_chain_data(i, rte_key_getmode(i), FALSE);
        }
      }

      if (mainw->pconx) pconx_chain_data(FX_DATA_KEY_PLAYBACK_PLUGIN, 0, FALSE);
      if (mainw->cconx) cconx_chain_data(FX_DATA_KEY_PLAYBACK_PLUGIN, 0);

      if (mainw->rte != old_rte) mainw->refresh_model = TRUE;
    }

    lives_microsleep_while_false(!mainw->do_ctx_update && all_updated);

    fg_stack_wait();
    rte_keys_update();

    if (!mainw->refresh_model) {
      lives_hook_stack_t *sah =
        lives_proc_thread_get_hook_stacks(mainw->player_proc)[SYNC_ANNOUNCE_HOOK];
      if (sah->stack) {
        all_updated = FALSE;
        lives_proc_thread_add_hook(mainw->player_proc, SYNC_ANNOUNCE_HOOK, 0, updates_done, NULL);
        mainw->gui_much_events = TRUE;
        mainw->do_ctx_update = TRUE;
        lives_proc_thread_trigger_hooks(mainw->player_proc, SYNC_ANNOUNCE_HOOK);
      }
    }

    if (mainw->refresh_model) {
      d_print_debug("node model invalidated, show frame then let's rebuild the model\n");
      errpt = 14;
    } else {
      if (run_next_cycle() != LIVES_RESULT_SUCCESS) {
        errpt = 6;
        goto lfi_err;
      }
    }

    ////////////////////////

    osc_sync_msg = osc_make_sync_msg(mainw->clip_index, mainw->plan_cycle->frame_idx,
                                     (double)mainw->currticks / TICKS_PER_SECOND_DBL,
                                     mainw->num_tracks);

    // save to scrap_file now if we have to
    if (mainw->record && !mainw->record_paused && mainw->scrap_file != -1 && fg_file == mainw->scrap_file) {
      if (mainw->record && (prefs->rec_opts & REC_AFTER_PB) && mainw->ext_playback &&
          (mainw->vpp->capabilities & VPP_CAN_RETURN)) {
        rec_after_pb = TRUE;
      }

      if (!rec_after_pb) {
#ifndef NEW_SCRAPFILE
        save_to_scrap_file(mainw->frame_layer);
#endif
      }
    }

    if (mainw->internal_messaging) {
      // this happens if we are calling from multitrack view frame,
      // or apply rte.  or when rendering / transcoding. We get our mainw->frame_layer and exit.
      // we are not in playback mode, and we just return mainw->frame_layer
      lives_freep((void **)&framecount);
      return mainw->frame_layer;
    }

    if (mainw->ext_playback) {
      weed_layer_t *return_layer = NULL;
      int lwidth, lheight, rs_align = 0;
      int layer_palette = weed_layer_get_palette(mainw->frame_layer);
      int tgt_gamma = WEED_GAMMA_UNKNOWN;

      /// check if function exists - it accepts rowstrides
      if (mainw->vpp->play_frame) player_v2 = TRUE;

      // use frame_layer instead of mainw->frame_layer
      if (!frame_layer) frame_layer = mainw->frame_layer;

      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) {
        // render the timecode for multitrack playback
        frame_layer = check_for_overlay_text(frame_layer);
        if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
          if (frame_layer == mainw->frame_layer) frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
          frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);
        }
      } else {
        if ((weed_palette_is_rgb(layer_palette) &&
             !(weed_palette_is_rgb(mainw->vpp->palette))) ||
            (weed_palette_is_lower_quality(mainw->vpp->palette, layer_palette))) {
          // TODO - should set target pal in nodemodel
          //
          // mainw->frame_layer is RGB and so is our screen, but plugin is YUV
          // so copy layer and convert, retaining original
          if (frame_layer == mainw->frame_layer) {
            frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
          }
        }
      }

      if (!player_v2) {
        rs_align = THREADVAR(rowstride_alignment_hint);
        THREADVAR(rowstride_alignment_hint) = -1;
      }

      if (layer_palette != mainw->vpp->palette) {
        // should never happen with PLAN
        if (frame_layer == mainw->frame_layer) {
          frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        }
        if (!convert_layer_palette_full(frame_layer, mainw->vpp->palette, mainw->vpp->YUV_clamping,
                                        mainw->vpp->YUV_sampling, mainw->vpp->YUV_subspace, tgt_gamma)) {
          if (!player_v2) THREADVAR(rowstride_alignment_hint) = rs_align;
          errpt = 17;
          goto lfi_err;
        }
      }

      if (!player_v2) {
        // vid plugin v1 expects compacted rowstrides (i.e. no padding/alignment after pixel row)
        if (frame_layer == mainw->frame_layer)
          frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        if (!compact_rowstrides(frame_layer)) {
          errpt = 18;
          goto lfi_err;
        }
      }

      if (rec_after_pb) {
        // record output from playback plugin
        int retwidth = mainw->pwidth / weed_palette_get_pixels_per_macropixel(mainw->vpp->palette);
        int retheight = mainw->pheight;

        return_layer = weed_layer_create(retwidth, retheight, NULL, mainw->vpp->palette);
        weed_layer_set_gamma(return_layer, weed_layer_get_gamma(frame_layer));

        if (weed_palette_is_yuv(mainw->vpp->palette)) {
          weed_set_int_value(return_layer, WEED_LEAF_YUV_CLAMPING, mainw->vpp->YUV_clamping);
          weed_set_int_value(return_layer, WEED_LEAF_YUV_SUBSPACE, mainw->vpp->YUV_subspace);
          weed_set_int_value(return_layer, WEED_LEAF_YUV_SAMPLING, mainw->vpp->YUV_sampling);
        }

        // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
        //if (!player_v2) THREADVAR(rowstride_alignment_hint) = -1;
        if (create_empty_pixel_data(return_layer, FALSE, TRUE))
          retdata = weed_layer_get_pixel_data_planar(return_layer, NULL);
        else return_layer = NULL;
      }

      // chain any data or alpha chanels connections

      if (return_layer) lives_leaf_dup(return_layer, frame_layer, WEED_LEAF_GAMMA_TYPE);
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
      } else THREADVAR(rowstride_alignment_hint) = rs_align;

      if ((!mainw->multitrack && prefs->letterbox) || (mainw->multitrack && prefs->letterbox_mt)) {
        // plugin can resize but may not be able to letterbox...
        if (mainw->vpp->capabilities & VPP_CAN_LETTERBOX) {
          // plugin claims it can letterbox, so request that
          // - all we need to do is set the ratios of the image in the frame to center it in the player
          // the player will resize the image to fill the reduced area
          get_letterbox_sizes(&pwidth, &pheight, &lb_width, &lb_height, FALSE);
          weed_set_double_value(frame_layer, "x_range", (double)lb_width / (double)pwidth);
          weed_set_double_value(frame_layer, "y_range", (double)lb_height / (double)pheight);
        }
      }

      // ensure previous frame has been displayed
      /* fg_stack_wait(); */
      /* lives_microsleep_while_true(mainw->do_ctx_update); */

      // RENDER VIA PLUGIN
      //g_print("PLAY PT 1\n");
      lwidth = weed_layer_get_width(frame_layer);
      if (!player_v2) pd_array = weed_layer_get_pixel_data_planar(frame_layer, NULL);

      if (mainw->vpp->capabilities & VPP_RETURN_AND_NOTIFY) {
        if (mainw->ext_player_layer) reset_ext_player_layer(FALSE);
        mainw->ext_player_layer = frame_layer;
        weed_layer_ref(frame_layer);
        weed_set_voidptr_value(mainw->ext_player_layer, LIVES_LEAF_VPP_PROCESSED_PTR, &vpp_processed_flag);
        //frame_layer = NULL;
      }

      if ((player_v2 && !(*mainw->vpp->play_frame)(mainw->ext_player_layer, mainw->currticks - mainw->stream_ticks, return_layer))
          || (!player_v2 && !(*mainw->vpp->render_frame)(lwidth, weed_layer_get_height(frame_layer),
              mainw->currticks - mainw->stream_ticks, pd_array, retdata, mainw->vpp->play_params))) {
        if (return_layer) {
          weed_layer_unref(return_layer);
          lives_free(retdata);
          return_layer = NULL;
        }
        if (!player_v2) lives_free(pd_array);
        errpt = 19;
        goto lfi_err;
      }

      if (!player_v2) lives_free(pd_array);

      if (return_layer) {
        save_to_scrap_file(return_layer);
        weed_layer_unref(return_layer);
        lives_free(retdata);
        return_layer = NULL;
      }

      if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) goto lfi_done;
    } // EXT PB done

    ////////////////////////////////////////////////////////
    // local display - either we are playing with no playback plugin, or else the playback plugin has no
    // local display of its own


    if (!weed_layer_get_width(mainw->frame_layer)) {
      errpt = 20;
      goto lfi_err;
    }
    if ((mainw->sep_win && !prefs->show_playwin) || (!mainw->sep_win && !prefs->show_gui)) {
      // no display to output, skip the rest
      errpt = 21;
      goto lfi_done;
    }

    if (!frame_layer) frame_layer = mainw->frame_layer;

    frame_layer = check_for_overlay_text(frame_layer);

    if (mainw->multitrack && mainw->multitrack->opts.overlay_timecode) {
      // render the timecode for multitrack playback
      if (frame_layer == mainw->frame_layer) {
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
        frame_layer = render_text_overlay(frame_layer, mainw->multitrack->timestring, DEF_OVERLAY_SCALING);
      }
    }

    if (prefs->use_screen_gamma) {
      if (frame_layer == mainw->frame_layer) {
        frame_layer = weed_layer_copy(NULL, mainw->frame_layer);
      }
      gamma_convert_layer(WEED_GAMMA_MONITOR, frame_layer);
    }

    ////////////////////////////////////////
    if (frame_layer ==  mainw->frame_layer) {
      frame_layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);
      weed_layer_copy(frame_layer, mainw->frame_layer);
    }
    if (!mainw->debug_ptr)
      mainw->debug_ptr = frame_layer;

    // this will ensure the layer is unreffed even if the func data is replaced by UNIQUE_DATA
    // otherwise only free_lpt is unreffed
    lives_proc_thread_t free_lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                   weed_layer_unref, 0, "v", frame_layer);

    if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
      lives_proc_thread_add_hook_full(mainw->player_proc, SYNC_ANNOUNCE_HOOK, HOOK_UNIQUE_DATA |
                                      HOOK_CB_HAS_FREEFUNCS | HOOK_OPT_FG_LIGHT,
                                      lives_layer_draw, 0, "vv", mainw->preview_image, NULL, frame_layer, free_lpt);

    } else {
      lives_proc_thread_add_hook_full(mainw->player_proc, SYNC_ANNOUNCE_HOOK, HOOK_UNIQUE_DATA | HOOK_CB_PRIORITY |
                                      HOOK_CB_HAS_FREEFUNCS | HOOK_OPT_FG_LIGHT,
                                      lives_layer_draw, 0, "vv", mainw->play_image, NULL, frame_layer, free_lpt);
    }

    frame_layer = NULL;
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

lfi_err:
  d_print_debug("LFI error %d %p\n", errpt, mainw->layers);
  success = FALSE;

  wait_layer_ready(mainw->frame_layer, FALSE);

lfi_done:
  if (frame_layer && frame_layer != mainw->frame_layer) {
    weed_layer_unref(frame_layer);
    frame_layer = NULL;
  }

  // this is reset when we call avsync_force()
  // the audio will have seeked to this frame and then be holding (outputting silence)
  // the audio player will signal when it reaches that point by setting audio_seek_ready to TRUE
  // once we set video_seek_ready to TRUE, the audio can continue
  // the timer will be advancing during this, so we discount the time spent waiting for audio_seek_ready
  if (!mainw->video_seek_ready) video_sync_ready();

  if (framecount) {
    if ((!mainw->fs || (prefs->play_monitor != 0 &&
                        prefs->play_monitor != widget_opts.monitor + 1))
        && !prefs->hide_framebar)
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
    lives_free(framecount);
    framecount = NULL;
  }

  if (success) {
    if (!mainw->multitrack &&
        !mainw->faded && (!mainw->fs || (prefs->play_monitor != 0 && prefs->play_monitor != widget_opts.monitor + 1))
        && mainw->current_file != mainw->scrap_file) {
      lives_proc_thread_add_hook_full(mainw->player_proc, SYNC_ANNOUNCE_HOOK, HOOK_UNIQUE_DATA | HOOK_OPT_FG_LIGHT,
                                      lives_widget_queue_draw, 0, "v", mainw->eventbox2);
    }
    if (LIVES_IS_PLAYING && mainw->multitrack && !cfile->opening) animate_multitrack(mainw->multitrack);

    if (mainw->frame_layer) {
      lives_notify(LIVES_OSC_NOTIFY_FRAME_SYNCH, (const char *)osc_sync_msg);
    }
  }

  lives_freep((void **)&osc_sync_msg);
  ////

  reset_old_frame_layer();
  old_frame_layer = STEAL_POINTER(mainw->frame_layer);

  if (!success) {
    if (!mainw->refresh_model) {
      if (errpt <= 15) {
        lives_microsleep_while_false(!mainw->do_ctx_update && all_updated);
        if (!mainw->refresh_model) {
          // free pixdata for frame_layer, then run the next cycle
          run_next_cycle();
        }
      }
    }
    return NULL;
  }

  //g_print("out of lfi at %s\n", lives_format_timing_string(lives_get_session_time()));
  return old_frame_layer;
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
    frames_t first_frame = 1, nframes = sfile->alt_frames ? sfile->alt_frames : sfile->frames, last_frame = nframes;
    if (clipno == mainw->playing_file) {
      if (mainw->scratch == SCRATCH_NONE || mainw->scratch == SCRATCH_REV) {
        last_frame = mainw->playing_sel ? sfile->end : mainw->play_end;
        if (last_frame > nframes) last_frame = nframes;
        first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
        if (first_frame > nframes) first_frame = nframes;
      }
    }
    if (nframe >= first_frame && nframe <= last_frame) return nframe;
    else {
      double fps = sfile->pb_fps;
      frames_t selrange = (1 + last_frame - first_frame);
      lives_direction_t dir, ndir;
      int nloops;

      g_print("CLAMP %d %d %d %ld %ld\n", nframe, first_frame, last_frame,
              mainw->startticks, mainw->currticks);

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

      g_print("selrange is %d\n", selrange);
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

      if (ndir == dir) mainw->scratch = SCRATCH_REALIGN;
      else {
        if (is_pbframe) {
          /// must set norecurse, otherwise we can end up in an infinite loop since dirchange_callback calls
          // calc_new_playback_position() which in turn calls this function
          RECURSE_GUARD_ARM;
          dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0,
                             LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          RECURSE_GUARD_END;
        } else sfile->pb_fps = -sfile->pb_fps;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
  g_print("nfr is %d\n", nframe);
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

  cframe = sfile->last_req_frame;
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
    g_print("ERR %ld and %ld and %ld %ld\n", otc, dtc, *ntc,
            mainw->currticks);
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
static double audio_start;

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
                         frames_t base, double fps, double * ttime, double * tconf) {
  // check range from stframe to enframe
  // calculate variance as (time for playhead to reach frame) - (time for player to reach it)
  // this is delta (frame - base) / fps - est_time_from_last_frame_decoded
  // ideally we want this to be positive. We also want to arrive with time to spare, this is done by adding to
  // (or subtracting from) base (depending on dir(fps)) [sign of delta and fp should always produce a =ve value,
  // so we skip over any -ve delta / fps)
  // if we find such a value going from stframe (up or down) to enframe, we return it
  // if there are no suitable targets we return the frame with the smallest abs(variance)
  lives_decoder_sys_t *dpsys = NULL;
  lives_clip_t *sfile = RETURN_NORMAL_CLIP(clipno);
  double minvary = -99999999., vary;
  double est_time, hdtime, ftime, min_time = -1.;
  double xconf = -1., xtconf = -1.;
  lives_direction_t dir;
  frames_t best_frame = 0;
  boolean can_est = FALSE;

  if (!sfile || fps == 0.) return 0.;
  dir = LIVES_DIRECTION_SIG(sfile->pb_fps);
  ftime = (double)dir / fps;

  if (dplug) dpsys = (lives_decoder_sys_t *)dplug->dpsys;
  if (dpsys && dpsys->estimate_delay) can_est = TRUE;

  hdtime = (double)(stframe - base) / fps;
  enframe += dir;
  if ((enframe - stframe) * dir < 0) dir = -dir;

  for (frames_t frame = stframe; ; frame += dir) {
    if (is_virtual_frame(clipno, frame)) {
      if (can_est)
        est_time = (*dpsys->estimate_delay)(dplug->cdata, get_indexed_frame(clipno, frame),
                                            0, &xconf);
      if (!can_est || fpclassify(est_time) != FP_NORMAL
          || (tconf && *tconf > 0. && xconf < *tconf && minvary > -99999999.)) {
        if (frame == enframe) break;
        hdtime += ftime;
        continue;
      }
    } else {
      // img timings
      est_time = sfile->img_decode_time;
    }
    if (est_time > 0.) {
      vary = hdtime - est_time;
      //g_print("frame %d (%d), hdtime = %f, est_time = %f\n", frame, base, hdtime, est_time);
      if (vary >= 0. && (frame - base) * dir >= 1) {
        if (ttime) *ttime = est_time;
        return frame;
      }
      if (vary > minvary) {
        minvary = vary;
        best_frame = frame;
        xtconf = xconf;
        min_time = est_time;
      }
    }
    hdtime += ftime;
    if (frame == enframe) break;
  }

  if (tconf) *tconf = xtconf;
  if (ttime) *ttime = min_time;
  return best_frame;
}


#define CATCHUP_LIMIT .9

static frames_t find_best_frame(lives_decoder_t *dplug, frames_t requested_frame, frames_t dropped,
                                int64_t jumplim, lives_direction_t dir) {
  // iteration 1:
  // try to calculate the best frame to play next
  // we cannot change direction, and we would like to be as near as possible to requested_frame
  // but we also want to find a frame we can reach in <= 1. / fps
  // if we are caching a frame then we will check if the cache frame is loaded and use that instead

  // the time calculation may vary inside the decoder, since the ordering of frames in the frame_index may not be sequential
  // hence the decoder can try to estimate the decoding time to seek to and decode requested frame, starting from the last frame it decoded
  // if the estimate is > target_time, then we check going forwards, it may be that there is a nearby keyframe which can be jumped to instantly
  // thus we check all frames in range, and select whichever gives the shortest estimate
  // iteration 3
  // here we are not trying to jump ahead of the timecode,
  // but we don't want to fall behind or shoot ahead
  // we should have calculated best_frame - some value between last frame played + dir and up to req. frame + few frames
  // now we analyse the range - if we are lagging we want to ensure that we advance more frames than the timecode does
  // but we dont want to do a big jump - that is the domain of getahead. So setting the range, we pick the largest delta =
  // player nframes - timecode nframes
  // - if we are ahead of the timecode we jusr play last frame + 1
  //
  // we have

  frames_t best_frame = -1;
  lives_clip_t *sfile = RETURN_VALID_CLIP(mainw->playing_file);
  static double fpf = 0.;
  static lives_clip_t *old_sfile = NULL;

  if (sfile) {
    if (sfile != old_sfile) fpf = 0;

    if (requested_frame < 0) {
      requested_frame = -requested_frame;

      double fratio = abs(sfile->pb_fps) / mainw->inst_fps;
      double align = dir * (requested_frame - sfile->last_frameno) / (2. * mainw->inst_fps);

      double slo;

      double fpfx = fratio + align;

      if (fpfx < 1.) {
        slo = 1. / -fpfx;
        if (slo < 0.95) slo = 0.95;
        return sfile->last_frameno + dir;
      }

      fpf += fpfx;

      if (fpf < 1.) fpf = 0.5;
      if (fpf > MAX_JMP_THRESH) fpf = MAX_JMP_THRESH;

      best_frame = sfile->last_frameno + dir * (frames_t)(fpf + .5);
      fpf -= (frames_t)fpf;

      if (mainw->frame_layer_preload && weed_layer_check_valid(mainw->frame_layer_preload)
          && mainw->pred_frame > 0) {
        frames_t pdelta = dir * (mainw->pred_frame - sfile->last_frameno);
        if (pdelta > 0) {
          frames_t bdelta = dir * (best_frame - sfile->last_frameno);
          if (bdelta < MIN_JMP_THRESH) bdelta = MIN_JMP_THRESH;
          if (mainw->cached_frame) {
            frames_t cdelta = dir * (lives_layer_get_frame(mainw->cached_frame) - sfile->last_frameno);
            if (cdelta > 0 && bdelta > cdelta) bdelta = cdelta;
          }
          pdelta >>= 1;
          if (bdelta >= pdelta) {
            bdelta = pdelta;
          }
          if (!bdelta) bdelta = 1;
          best_frame = sfile->last_frameno + dir * bdelta;
        }
      }
    } else {
      double targ_time;
      double tconf = 0.5;
      frames_t max = MIN(LAGFRAME_TRIGGER, dir * (requested_frame - sfile->last_frameno)) / 2;
      if (max < 0) max = 0;
      max += best_frame;
      max += dropped;

      if ((max - requested_frame) * dir > dropped) max = requested_frame + dir * dropped;
      if ((max - sfile->last_frameno) * dir > MAX_JMP_THRESH) max = sfile->last_frameno
            + MAX_JMP_THRESH * dir;
      if (dir * (max - best_frame) <= 0) return max;

      best_frame = reachable_frame(mainw->playing_file, dplug, best_frame,
                                   max, sfile->last_frameno,
                                   sfile->pb_fps, &targ_time, &tconf);
      /* g_print("FRAMEVALS %d %d %d %d %d %d\n", best_frame, sfile->last_frameno, mainw->actual_frame, dir, */
      /*         requested_frame, dropped); */
    }

    if (!best_frame || (best_frame - sfile->last_frameno) * dir < 1) best_frame = sfile->last_frameno + dir;

    if (dir * (best_frame - sfile->last_req_frame) > MAX_JMP_THRESH)
      best_frame = sfile->last_req_frame + MAX_JMP_THRESH;

    if ((sfile->last_frameno - requested_frame) * dir > 0) {
      best_frame = sfile->last_frameno + dir;
      if (best_frame != clamp_frame(mainw->playing_file, best_frame)) best_frame = -1;
      return best_frame;
    }

    if (sfile->pb_fps > 0. || clip_can_reverse(mainw->playing_file)) {
      if (jumplim > 0 && dir * (best_frame - sfile->last_frameno) > jumplim) {
        best_frame = sfile->last_frameno + dir * jumplim;
      }
      if ((best_frame - requested_frame) * dir > dropped) best_frame = requested_frame + dropped * dir;
      if ((best_frame - sfile->last_frameno) * dir < 1) best_frame = sfile->last_frameno + dir;
    }

    if (mainw->cached_frame) {
      frames_t cdelta = dir * (lives_layer_get_frame(mainw->cached_frame) - sfile->last_frameno);
      if (cdelta > 0) {
        frames_t bdelta = dir * (best_frame - sfile->last_frameno);
        cdelta >>= 1;
        if (bdelta >= cdelta) {
          bdelta = cdelta;
          if (!bdelta) bdelta = 1;
          best_frame = sfile->last_frameno + dir * bdelta;
        }
      }
    }

    if (best_frame != clamp_frame(mainw->playing_file, best_frame)) best_frame = -1;
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

static int process_one(void) {
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
  //  static frames_t last_req_frame = 0;
  /* #if defined HAVE_PULSE_AUDIO || defined ENABLE_JACK */
  /*   static off_t last_aplay_offset = 0; */
  /* #endif */
  volatile float const *cpuload;
  //static lives_proc_thread_t gui_lpt = NULL;
  //double cpu_pressure;
  static double last_eff_upd_time = 0.;
  lives_clip_t *sfile = cfile;
  _vid_playback_plugin *old_vpp;
  ticks_t new_ticks;
  lives_time_source_t time_source = LIVES_TIME_SOURCE_NONE;
  static frames_t requested_frame = 0;
  static frames_t best_frame = -1;
  frames_t xrequested_frame = -1;
  frames_t fixed_frame = 0;
  //double xtime = 0.;
  boolean show_frame = FALSE, showed_frame = FALSE;
  boolean can_rec = FALSE;
  static boolean reset_on_tsource_change = FALSE;
  int old_current_file, old_playing_file, old_blend_file;
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
  //int coun = 0;
  lives_clip_data_t *cdata = NULL;
  int64_t jumplim = 0;
  int retval = 0;
  int close_this_clip, new_clip, new_blend_file;
  boolean frame_invalid = FALSE;
  boolean can_realign = FALSE;
  float cpuloadval = 0.;

  lives_hook_stack_t *sah =
    lives_proc_thread_get_hook_stacks(mainw->player_proc)[SYNC_ANNOUNCE_HOOK];

  // current video playback direction
  lives_direction_t dir = LIVES_DIRECTION_NONE;
  old_current_file = mainw->current_file;
  old_playing_file = mainw->playing_file;

  //check_mem_status();

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

  mainw->pulsed_read->in_use = TRUE;

player_loop:
  // do the following each cycle:
  // - check if we need to close a file (e.g. a generator ended)
  //    or if the playinhg file / bg file changed
  // - check if we need to rebuild the nodemodel, exec plan
  // - kick off a new plan cycle
  // - get the current adjusted timecode, reinit timers if necessary
  // - handle trickplay keys
  // - get the current frame to play
  // - update effort based on instant fps (for adaptive qualiry)
  // - possibly play a frame, and kick off next plan cycle
  // - possibly record a frame event / frame
  // - recalibrate frame predictions if appropriate
  // - check if we should pre-cache a future frame
  // - if all gfx events have been completed, trigger queued updaues
  //     this can include player frame updates, timeline cursor.
  //     as well as switching to / from sepwin / fullscreen
  //    - apply cached fx toggles
  //  - check if any chabges require a rebuild of the nodemodel
  //  - return if playback was stopped for any reason

  pthread_yield();

  sfile = mainw->files[mainw->playing_file];

  /* if (++coun == 100) { */
  /*   show_pbtimer_stats(); */
  /*   coun = 0; */
  /* } */

  old_playing_file = mainw->playing_file;
  old_blend_file = mainw->blend_file;
  old_vpp = mainw->vpp;

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

  //fixed_frame = -1;

switch_point:
  if (mainw->close_this_clip != -1 || mainw->new_clip != mainw->playing_file
      || mainw->new_blend_file != mainw->blend_file) {

    do {
      close_this_clip = mainw->close_this_clip;
      new_clip = mainw->new_clip;
      new_blend_file = mainw->new_blend_file;
      if (IS_VALID_CLIP(close_this_clip) || mainw->new_clip != mainw->playing_file
          || mainw->new_blend_file != mainw->blend_file) {
        fg_stack_wait();
        lives_microsleep_while_true(mainw->do_ctx_update);
      }
    } while (mainw->close_this_clip != close_this_clip || mainw->new_clip != new_clip
             || mainw->new_blend_file != new_blend_file);

close_clip:

    if (IS_VALID_CLIP(close_this_clip)) {
      // first deal with the case where we are asked to CLOSE A CLIP
      // - currently this happens if and only if a generator / webcam etc.
      // non physical clip is finished with

      if (new_blend_file == close_this_clip)
        new_blend_file = mainw->new_blend_file = mainw->blend_file;

      if (new_clip == close_this_clip)
        new_clip = mainw->new_clip = mainw->playing_file;

      if (IS_VALID_CLIP(close_this_clip)) {
        mainw->refresh_model = TRUE;

        if (close_this_clip == mainw->playing_file) {
          // by default, switch to the currently playing clip, this will work if closing bg clip
          if (new_clip == mainw->playing_file || !IS_VALID_CLIP(new_clip)) {
            new_clip = -1;
            if (IS_VALID_CLIP(mainw->pre_src_file)) new_clip = mainw->pre_src_file;
            // if clip to be closed is fg clip, and we have a bg clip, we will switch to bg clip (making it fg clip)
            else if (mainw->blend_file != mainw->playing_file && IS_VALID_CLIP(mainw->blend_file))
              new_clip = mainw->blend_file;
          }
        }

        // breifly set current_file to clip to be closed. After closing it, new_clip will be returned if valid
        // and we switch current_file (but NOT playing file)
        mainw->current_file = close_this_clip;
        mainw->noswitch = FALSE;
        //mainw->playing_file = mainw->current_file
        new_clip = close_current_file(new_clip);
        mainw->noswitch = TRUE;
        //sfile = mainw->files[mainw->playing_file];
      }

      // if closed clip was going to be new blend_file
      // blend__file will be swithced to playing_file
      // if playing_file is invalid, playing_file, current-file and blend_file will all be changed
      if (mainw->blend_file == close_this_clip) {
        if (new_blend_file == close_this_clip) {
          new_blend_file = mainw->new_blend_file = mainw->playing_file;
        }
      } else if (new_blend_file == close_this_clip) {
        new_blend_file = mainw->new_blend_file = mainw->blend_file;
      }
    }

    close_this_clip = mainw->close_this_clip = -1;

    if (!IS_VALID_CLIP(new_clip)) {
      // if new_clip is invalid, do not switch
      if (!IS_VALID_CLIP(mainw->playing_file)) {
        // if playing_file is also invalid this is an error condition
        mainw->cancelled = CANCEL_INTERNAL_ERROR;
        retval = ONE_MILLION + mainw->cancelled;
        goto err_end;
      }
      new_clip = mainw->new_clip = mainw->playing_file;
    }

    if (new_clip == mainw->playing_file) {
      // playing_file must be valid. If we are told to switch to playing_file,
      // just do so quietly (i.e nothing changes)
      mainw->current_file = mainw->playing_file;
    } else {
#ifdef ENABLE_JACK
      if (mainw->xrun_active) mainw->xrun_active = FALSE;
#endif

#ifdef ENABLE_JACK
      if (prefs->audio_player == AUD_PLAYER_JACK) {
        /* if (sfile->arate) { */
        /*   g_print("HIB %d %d %d %d %ld %f %f %ld %ld %d %f\n", sfile->frameno, sfile->last_req_frame, mainw->playing_file, */
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
          /*         sfile->last_req_frame, mainw->playing_file, */
          /*         aplay_file, sfile->aseek_pos, */
          /*         sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., (double)sfile->aseek_pos / */
          /*         (double)sfile->arps / 4. * sfile->fps + 1., */
          /*         mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
        }
      }
#endif
      if (new_clip != mainw->playing_file) {
        // playing_file can be invalid, for example if it wa a generator which closed
        sfile = RETURN_VALID_CLIP(mainw->playing_file);
        if (sfile) {
          if ((sfile->last_req_frame - sfile->last_frameno) * dir >= 0) {
            sfile->last_frameno = sfile->frameno = sfile->last_req_frame;
            sfile->sync_delta = mainw->currticks - mainw->startticks;
          } else sfile->sync_delta = 0;
          sfile->last_play_sequence = mainw->play_sequence;
        }

        srcgrp_remove(mainw->playing_file, -1, SRC_PURPOSE_PRECACHE);

        mainw->noswitch = FALSE;
        // must be called even if just called close_current_file()
        // mainw-.current_file will be altered, but NOT mainw->playing_file
        do_quick_switch(new_clip);
        mainw->noswitch = TRUE;

        // must only be changed AFTER do_quick_switch()
        mainw->playing_file = mainw->current_file;

        sfile = mainw->files[mainw->playing_file];

        cdata = get_clip_cdata(mainw->playing_file);
        if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
          g_print("decoder: seek flags = %d, jump_limit = %ld, max_fps = %.4f\n",
                  cdata->seek_flag,
                  cdata->jump_limit, cdata->max_decode_fps);
          jumplim = cdata->jump_limit * 4;
          if (!jumplim) jumplim = JUMPFRAME_TRIGGER;
          jumplim = MIN(jumplim, JUMPFRAME_TRIGGER);
        } else jumplim = 0;
      }
      // if we switch from a gnerator, we may need tp close it
      if (IS_VALID_CLIP(mainw->close_this_clip)) {
        close_this_clip = mainw->close_this_clip;
        goto close_clip;
      }
    }

    if (new_blend_file != mainw->blend_file) {
      mainw->refresh_model = TRUE;

      if (IS_VALID_CLIP(new_blend_file)) {
        mainw->blend_file = new_blend_file;

        if (mainw->ce_thumbs && (mainw->active_sa_clips == SCREEN_AREA_BACKGROUND))
          ce_thumbs_highlight_current_clip();

      } else mainw->new_blend_file = new_blend_file = mainw->blend_file = -1;

      old_current_file = mainw->current_file;
      old_playing_file = mainw->playing_file;
    }


    /* if (mainw->playing_file != old_playing_file) { */
    /*   trim_frame_index(mainw->playing_file, &sfile->frameno, sfile->pb_fps > - 0. ? 1 : -1, 0); */
    /*   clamp_frame(-1, sfile->frameno); */
    /*   if (sfile->alt_frames != sfile->frames) { */
    /*     sfile->last_frameno = mainw->actual_frame = sfile->frameno; */
    /*   } */
    /* } */

    if (mainw->playing_file != old_playing_file
        || mainw->blend_file != old_blend_file) {
      mainw->refresh_model = TRUE;

      // TODO - make sure we are resetting correctly with audio lock on
      if (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
          && (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP)
          && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS)) {
        mainw->scratch = SCRATCH_JUMP;
      } else if (mainw->scratch != SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;

      //  reset player state

      if (mainw->playing_file != old_playing_file) {
        mainw->force_show = TRUE;
        mainw->actual_frame = sfile->last_frameno;
        requested_frame = xrequested_frame = sfile->last_req_frame = sfile->frameno;
        fixed_frame = sfile->frameno;

        lagged = dropped = skipped = 0;
        check_getahead = FALSE;
        bungle_frames = 0;
        recalc_bungle_frames = 0;

        getahead = 0;
        test_getahead = -1;

        cdata = get_clip_cdata(mainw->playing_file);
        if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
          jumplim = cdata->jump_limit * 4;
          if (!jumplim) jumplim = JUMPFRAME_TRIGGER;
        } else jumplim = 0;

        if (sfile->last_play_sequence != mainw->play_sequence) {
          sfile->last_play_sequence = mainw->play_sequence;
          sfile->last_frameno = mainw->actual_frame = sfile->last_req_frame = sfile->frameno;
        } else {
          if ((sfile->last_req_frame - sfile->last_frameno) * dir >= 0) {
            mainw->startticks = mainw->currticks - sfile->sync_delta;
          }
        }

        sfile->sync_delta = 0;

        if (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)) avsync_force();

#ifdef ENABLE_JACK
        if (prefs->audio_player == AUD_PLAYER_JACK) {
          /* if (sfile->arate) */
          /*   g_print("HIB2 %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", mainw->actual_frame, sfile->frameno, sfile->last_req_frame, */
          /*           mainw->playing_file, aplay_file, aseek_pos, */
          /*           sfile->fps * lives_jack_get_pos(mainw->jackd) + 1., */
          /*           (double)aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
          /*           mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
        }
#endif
#ifdef HAVE_PULSE_AUDIO
        if (prefs->audio_player == AUD_PLAYER_PULSE) {
          /* if (sfile->arate) */
          /*   g_print("HIB2 %d %d %d %d %d %ld %f %f %ld %ld %d %f\n", mainw->actual_frame, sfile->frameno, sfile->last_req_frame, */
          /* 	  mainw->playing_file, aplay_file, aseek_pos, */
          /* 	  sfile->fps * lives_pulse_get_pos(mainw->pulsed) + 1., */
          /* 	  (double)aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
          /* 	  mainw->currticks, mainw->startticks, sfile->arps, sfile->fps); */
        }
#endif

        cache_hits = cache_misses = 0;

        mainw->new_clip = new_clip = mainw->playing_file;

        if (mainw->record && !mainw->record_paused) mainw->rec_aclip = mainw->current_file;

        drop_off = TRUE;

        /// playing file should == current_file, but just in case store separate values.
        old_current_file = mainw->current_file;
        old_playing_file = mainw->playing_file;

        if (mainw->frame_layer_preload)
          weed_layer_unref(STEAL_POINTER(mainw->frame_layer_preload));
        if (mainw->cached_frame) weed_layer_unref(STEAL_POINTER(mainw->cached_frame));

        /* if (sfile->arate) */
        /*   g_print("seek vals: vid %d %d %ld = %f %d %f\n", sfile->last_frameno, sfile->frameno, sfile->aseek_pos, */
        /*           (double)sfile->aseek_pos / (double)sfile->arate / 4. * sfile->fps + 1., */
        /*           sfile->arate, sfile->fps); */

        last_time_source = time_source;
        time_source = LIVES_TIME_SOURCE_NONE;
        reset_playback_clock(mainw->origticks);
        mainw->last_startticks = mainw->startticks = mainw->currticks
                                 = lives_get_current_playback_ticks(mainw->origticks, &time_source);

        //g_print("SWITCH %d %d %d %d\n", sfile->frameno, requested_frame, sfile->last_frameno, mainw->actual_frame);

        /// end SWITCH POINT

        if (!CURRENT_CLIP_IS_VALID) lives_abort("Invalid playback clip");
      }

      frame_invalid = FALSE;
    }
  }

  if (mainw->refresh_model) rebuild_nodemodel();

  /* if (!mainw->refresh_model) { */
  /*   if (mainw->plan_cycle) { */
  /*     if (mainw->plan_cycle->state == PLAN_STATE_DISCARD */
  /*         || mainw->plan_cycle->state == PLAN_STATE_ERROR */
  /*         || mainw->plan_cycle->state == PLAN_STATE_CANCELLED) { */
  /*       //g_print("pl state is %lu\n", mainw->plan_cycle->state); */
  /*       if (mainw->plan_runner_proc) { */
  /*         lives_proc_thread_request_cancel(mainw->plan_runner_proc, FALSE); */
  /*         lives_proc_thread_join(mainw->plan_runner_proc); */
  /*         mainw->plan_runner_proc = NULL; */
  /*       } */
  /*       exec_plan_free(mainw->plan_cycle); */
  /*       mainw->plan_cycle = NULL; */
  /*     } */
  /*   } */
  /* } */

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

  mainw->currticks = lives_get_current_playback_ticks(mainw->origticks, &time_source);
  if (mainw->currticks < mainw->startticks) {
    /* delay_ticks += (mainw->currticks - mainw->startticks); */
    /* if (delay_ticks > mainw->startticks - mainw->currticks) { */
    /*   delay_ticks -= mainw->startticks - mainw->currticks; */
    /*   mainw->startticks = mainw->currticks; */
    /* } else { */
    /*   if (delay_ticks > 0) mainw->startticks -= delay_ticks; */
    /*   delay_ticks = 0; */
    /* } */
    //}
    //if (mainw->currticks < mainw->startticks) {
  }
  //g_print("LOOP\n");

  dir = LIVES_DIRECTION_SIG(sfile->pb_fps);

  if (mainw->currticks == -1) {
    if (time_source == LIVES_TIME_SOURCE_SOUNDCARD) handle_audio_timeout();
    mainw->cancelled = CANCEL_ERROR;
    retval = mainw->cancelled;
    goto err_end;
  }

  if (time_source != last_time_source && last_time_source != LIVES_TIME_SOURCE_NONE
      && reset_on_tsource_change) {
    reset_on_tsource_change = FALSE;
    mainw->last_startticks = mainw->startticks = mainw->currticks;
    mainw->fps_mini_ticks = lives_get_session_ticks();
  }

  if (mainw->startticks > mainw->currticks) mainw->startticks = mainw->currticks;

  if (init_timers) {
    init_timers = FALSE;
    mainw->currticks = lives_get_current_playback_ticks(mainw->origticks, NULL);
    mainw->last_startticks = mainw->startticks = mainw->currticks;
    mainw->fps_mini_ticks = lives_get_session_ticks();

    last_eff_upd_time = lives_get_session_ticks_lax();

    mainw->last_startticks--;
    mainw->fps_mini_measure = 0;
    getahead = 0;
    test_getahead = -1;

    sfile->last_req_frame = sfile->last_frameno =
                              mainw->actual_frame = sfile->frameno;
    if (sfile->last_play_sequence != mainw->play_sequence && CLIP_HAS_VIDEO(mainw->playing_file)
        && !sfile->frameno) sfile->frameno = mainw->play_start;

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

  //////////////
  if (!mainw->plan_cycle) {
    if (!mainw->do_ctx_update && all_updated) {
      rte_keys_update();
      if (!mainw->refresh_model) {
        fg_stack_wait();
        run_next_cycle();
      }
    }
  }
  /////////////

  mainw->audio_stretch = 1.;

  if (mainw->record_starting) {
    IF_APLAYER_JACK(jack_get_rec_avals(mainw->jackd);)
    IF_APLAYER_PULSE(pulse_get_rec_avals(mainw->pulsed);)
    mainw->record_starting = FALSE;
  }

#ifdef ADJUST_AUDIO_RATE
  boolean calc_astretch = FALSE;
  // adjust audio rate slightly if we are behind or ahead
  // shouldn't need this since normally we sync video to soundcard
  // - unless we are controlled externally (e.g. jack transport) or system clock is forced
  if (time_source != LIVES_TIME_SOURCE_SOUNDCARD) {
    IF_APLAYER_JACK(calc_atretch = TRUE;)
    IF_APLAYER_PULSE(calc_atretch = TRUE;)
    if (calc_astretch) {
      (double target_fps = fabs(sfile->pb_fps);//weed_get_double_value(inst, WEED_LEAF_TARGET_FPS, NULL);
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
      })
    }
  }
#endif

  // playing back an event_list
  // here we need to add mainw->offsetticks, to get the correct position when playing back in multitrack

  if (CURRENT_CLIP_IS_VALID && !mainw->proc_ptr && cfile->next_event) {
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
    if (!mainw->do_ctx_update && all_updated) {
      if (sah->stack) {
        all_updated = FALSE;
        lives_proc_thread_add_hook(mainw->player_proc, SYNC_ANNOUNCE_HOOK, HOOK_OPT_FG_LIGHT, updates_done, NULL);
        lives_proc_thread_trigger_hooks(mainw->player_proc, SYNC_ANNOUNCE_HOOK);
      }
      mainw->gui_much_events = TRUE;
    }

    // also allows keypresses
    mainw->do_ctx_update = TRUE;

    if (mainw->cancelled == CANCEL_NONE) {
      return 0;
    }
    retval = mainw->cancelled;
    goto err_end;
  }

  // free playback

  if (mainw->frame_layer_preload && mainw->pred_clip != mainw->playing_file)
    weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);

  if (sfile->next_frame > 0) {
    fixed_frame = sfile->next_frame;
    sfile->next_frame = 0;
    mainw->force_show = TRUE;
    mainw->scratch = SCRATCH_JUMP;
    /* }   */

    /* if (fixed_frame > 0) { */
    // next_frame can be set, eg. when jumping to a bookmark
    // we MUST show that frame
    sfile->frameno =
      sfile->last_req_frame = requested_frame = fixed_frame = sfile->next_frame;
  }

  // if we have a preload frame, we can keep it iff it is in range of forced frame
  // otherwise, invalidate it
  if (fixed_frame > 0) {
    if (mainw->frame_layer_preload) {
      int delta = dir * (mainw->pred_frame - fixed_frame);
      if (delta < 0 || delta > MAX_JMP_THRESH)
        weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);;
    }
  }

  if (mainw->scratch == SCRATCH_JUMP) {
    resync_audio(mainw->playing_file, (double)sfile->last_frameno + dir);
    mainw->scratch = SCRATCH_JUMP_NORESYNC;
  }

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
    fixed_frame = sfile->frameno;
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
      // calc_new_playback_position returns a frame request based on the player mode and the time delta
      //
      // mainw->startticks is the timecode of the last frame requested (sfile->last_req_frame)
      // new_ticks is the (adjusted) current time
      // sfile->pb_fps (if playing) or sfile->fps (if not) are also used in the calculation
      // as well as selection bounds and loop mode settings (if appropriate)
      //
      // on return, new_ticks is either left unaltered or set to the timecode of the next frame to show
      // which will always be be <= the current time
      // and  returned requested_frame is set to the frame to showk calculated from the time delta, mdofied by sfile->
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

      /////////////////////////////////////

      //trickplay section
      // here we deal witha adjustments to frame
      // or to pb_fps

      if (mainw->scratch == SCRATCH_FWD || mainw->scratch == SCRATCH_BACK
          || mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA) {
        sfile->fps_scale = KEY_RPT_INTERVAL * prefs->scratchback_amount
                           * USEC_TO_TICKS / TICKS_PER_SECOND_DBL;
        if (mainw->scratch == SCRATCH_BACK || mainw->scratch == SCRATCH_BACK_EXTRA) {
          sfile->fps_scale = -sfile->fps_scale;
        }
        if (mainw->scratch == SCRATCH_FWD_EXTRA || mainw->scratch == SCRATCH_BACK_EXTRA)
          sfile->fps_scale *= 4.;

        if (!clip_can_reverse(mainw->playing_file) && sfile->fps_scale < 0.)
          sfile->fps_scale = 1. / abs(sfile->fps_scale);

        if (AUD_SRC_EXTERNAL) sfile->last_req_frame = sfile->last_frameno;

        drop_off = TRUE;

        if (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED))
          mainw->scratch = SCRATCH_JUMP_NORESYNC;
        else mainw->scratch = SCRATCH_JUMP;
      } else sfile->fps_scale = 1.;

      // paused
      if (LIVES_UNLIKELY(sfile->play_paused)) {
        mainw->startticks = mainw->currticks;
        if (!mainw->video_seek_ready || !mainw->audio_seek_ready) video_sync_ready();
      }

      if (LIVES_LIKELY(mainw->cancelled == CANCEL_NONE)) {
        /// get frame position at current time

#ifdef DEBUG_FRAME_TIMING
        lives_printerr("PRE: %ld %ld  %d %f\n", mainw->startticks, new_ticks, sfile->last_req_frame,
                       (new_ticks - mainw->startticks) / TICKS_PER_SECOND_DBL * sfile->pb_fps);
#endif
        requested_frame = xrequested_frame
                          = calc_new_playback_position(mainw->playing_file, mainw->startticks,
                              &new_ticks);

#ifdef DEBUG_FRAME_TIMNING
        lives_printerr("POST: %ld %ld %d (%ld %d)\n", mainw->startticks, new_ticks, requested_frame, mainw->pred_frame, getahead);
#endif

        // deal with jumps and resyncs

        if (mainw->scratch == SCRATCH_JUMP) {
          avsync_force(); // should reset video_seek_ready
        }

        if (!mainw->video_seek_ready) {
          mainw->force_show = TRUE;
          requested_frame = fixed_frame = sfile->last_req_frame = sfile->last_frameno + dir;
        }

        if (mainw->scratch == SCRATCH_JUMP || mainw->scratch == SCRATCH_JUMP_NORESYNC) {
          fixed_frame = requested_frame;
        }

        /// check if we got a new frame, otherwise we can increment spare_cycles

        if (new_ticks != mainw->startticks) {
          last_spare_cycles = spare_cycles;
          spare_cycles = 0;
        } else spare_cycles++;

#ifdef ENABLE_PRECACHE
        if (spare_cycles > 0 && last_spare_cycles > 0
            && !mainw->frame_layer_preload) can_precache = TRUE;
#endif
        // we will show a frame if:
        // we have forced show
        // we have a fixed display rate (e.g. for streaming)
        // we have vatiablr display rate and got a new frame from the timer

        /* g_print("VALS %ld %ld %ld and %d %d\n", new_ticks, mainw->startticks,
           mainw->last_startticks, requested_frame, sfile->last_req_frame); */
        if (mainw->force_show) {
          show_frame = TRUE;
        } else {
          //g_print("%ld %ld %ld %d %d %d\n", mainw->currticks, mainw->startticks, new_ticks,
          //sfile->last_frameno, requested_frame, sfile->last_req_frame);
          if (mainw->fixed_fpsd > 0. || (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)) {
            ticks_t dticks;
            dticks = (mainw->clock_ticks - mainw->last_display_ticks) / TICKS_PER_SECOND_DBL;
            if ((mainw->fixed_fpsd > 0. && (dticks >= 1. / mainw->fixed_fpsd)) ||
                (mainw->vpp && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback &&
                 dticks >= 1. / mainw->vpp->fixed_fpsd)) {
              show_frame = TRUE;
            }
          }
        }
        if (new_ticks != mainw->startticks && new_ticks != mainw->last_startticks
            && (requested_frame != sfile->last_req_frame || sfile->frames == 1
                || (mainw->playing_sel && sfile->start == sfile->end))) {
          if (mainw->fixed_fpsd <= 0. && (!mainw->vpp || mainw->vpp->fixed_fpsd <= 0. || !mainw->ext_playback)) {
            show_frame = TRUE;
          }
          if (prefs->show_dev_opts) jitter = (double)(mainw->currticks - new_ticks) / TICKS_PER_SECOND_DBL;
        }
        sfile->last_req_frame = requested_frame;
      }
    }

    // calculate the "best frame" to play
    // we do this if we are going to show a frame
    // or if not showing, and we have spare cycles, we may precache it
    //
    // the best frame is not always the requested frame, it may very if
    // we are lagging or ahead.
    //
    showed_frame = FALSE;
    best_frame = -1;

    if ((show_frame && sfile->delivery != LIVES_DELIVERY_PUSH) || can_precache) {
      dropped = 0;

      if (mainw->frame_layer_preload && mainw->pred_frame > 0) {
        lagged = (requested_frame - mainw->pred_frame) * dir;
      } else {
        lagged = (requested_frame - mainw->actual_frame) * dir;
      }

      if (lagged < 0) lagged = 0;

      if (!drop_off) {
        dropped = dir * (requested_frame - sfile->last_req_frame) - 1;
        if (dropped < 0) dropped = 0;
      }

      if (!fixed_frame && !sfile->play_paused) {
        lives_decoder_t *dplug = NULL;
        if (sfile->clip_type == CLIP_TYPE_FILE) {
          if (get_primary_src_type(sfile) == LIVES_SRC_TYPE_DECODER)
            dplug = (lives_decoder_t *)get_primary_actor(sfile);
        }
        best_frame = find_best_frame(dplug, -requested_frame, dropped, jumplim, dir);
        //g_print("bf1 = %d\n", best_frame);
      }
    }

    if (show_frame && sfile->delivery != LIVES_DELIVERY_PUSH) {
      if (best_frame != -1 && !fixed_frame) {
        sfile->frameno = best_frame;
      }

      if (mainw->scratch != SCRATCH_NONE) {
        scratch  = mainw->scratch;
        mainw->scratch = SCRATCH_NONE;
      }

      requested_frame = clamp_frame(-1, requested_frame);

      if (new_ticks > mainw->startticks) {
        mainw->last_startticks = mainw->startticks;
        mainw->startticks = new_ticks;
      }

      if (mainw->foreign) {
        if (requested_frame >= sfile->frameno) {
          mainw->actual_frame = requested_frame;

          load_frame_image(sfile->frameno);
          sfile->last_frameno = mainw->actual_frame;
          g_print("INST FPS is %.4f\n", mainw->fps_mini_measure /
                  ((lives_get_session_ticks() - mainw->fps_mini_ticks) / TICKS_PER_SECOND_DBL));
        }

        if (mainw->cancelled != CANCEL_NONE) {
          retval = mainw->cancelled;
          goto err_end;
        }
        g_print("ret A4\n");
        return 0;
      }

      if (sfile->frames > 1 && prefs->noframedrop
          && (scratch == SCRATCH_NONE || scratch == SCRATCH_REV)) {
        // if noframedrop is set, we may not skip any frames
        // - the usual situation is that we are allowed to drop late frames
        // in this mode we may be forced to play at a reduced framerate
        sfile->frameno = sfile->last_frameno + dir;
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

#ifdef ENABLE_PRECACHE
      if (mainw->pred_clip == -1) {
        /// failed load, just reset
        mainw->frame_layer_preload = NULL;
        cleanup_preload = FALSE;
        mainw->pred_frame = 0;
      } else if (cleanup_preload) {
        if (mainw->frame_layer_preload) {
          weed_layer_set_invalid(mainw->frame_layer_preload, TRUE);
        }
      }
#endif
    }

update_effort:
    if (prefs->pbq_adaptive && scratch == SCRATCH_NONE) {
      // to calulate the "effort" we derive the value
      // MIN(abs(pb_fps), real_fps) / cloxk_ratio * last_play_cycle_duration
      // the MIN part concerns the fact that user can set the framerate very
      // high and the player cannot always be expected to keep up - we will almost
      // certainly have to skip frames; however, if we are managing to keep up with
      // unadjusted fps, this is goof enough.
      //
      // The values are divided by clock_ratio -denoting measured time vs. wall clock
      // time - i.e if the clock rate is slower, then the observed fps will be proportionally slower
      // to maintain sync, so this gives the real (wall clock fps)
      // then multiplying by the last plan cycle duration, theoretically, if this value
      // is < 1 then the target rate is achievable, however, empirically, there are overheads
      // and the pivot point is about 0.5
      //
      // we thus take the value and if < 0.5, this is "good" with value 1. / v
      // or if > 0.5, bad with value v * 4 (so at 0.5, both values are 2.0)
      // this then fed to the "effort calulator", where it is queued, averaged and
      // a laggind detivative gives the quality level: high, med. low
      int64_t now = lives_get_session_ticks_lax();
      if (now - last_eff_upd_time > EFF_UPD_THRESH) {
        // 0 == cpuload, 1 == last_cyc, 2 = tgt
        double dets[3], avcy;
        float friction;
        last_eff_upd_time = now;
        mainw->inst_fps = get_inst_fps(FALSE);
        IGN_RET(avcy = get_cycle_avg_time(dets));
        if (dets[1]) {
          // current target fps
          double crat = get_pbtimer_clock_ratio();
          if (!crat) crat = 1.;
          double tgt = abs(sfile->pb_fps) / crat;
          double rtgt = sfile->fps / crat;

          /* g_print("EFF calc: eff = %f, " */
          /* 	  "av cycle = %f, cpu = %f, last cyc = %f, instfps = %f\n" */
          /* 	  "inst * dur = %f, dur / avct = %f, timer load = %f\n" */
          /* 	  "target = %f, actual = %f\n", eff, */
          /* 	  avcy, dets[0], dets[1], mainw->inst_fps, */
          /* 	  mainw->inst_fps * dets[1], dets[1] / avcy, tload, */
          /* 	  MIN(tgt, rtgt), maxfps); */

          friction = MIN(tgt, rtgt) * dets[1];
          if (friction > EFFORT_RANGE_MAX / 32.) friction = EFFORT_RANGE_MAX / 32.;
          if (friction < 16. / EFFORT_RANGE_MAX) friction = 16. / EFFORT_RANGE_MAX;

          if (friction > .5) friction *= BAD_EFF_MULT;
          else friction = -GOOD_EFF_MULT / friction;

          //g_print("eff2 fric %.8f\n", friction);
          update_effort(friction);
          if (prefs->pb_quality != future_prefs->pb_quality)
            mainw->refresh_model = TRUE;
        }
      }
    }

    if ((sfile->delivery == LIVES_DELIVERY_PUSH
         || (sfile->delivery == LIVES_DELIVERY_PUSH_PULL && mainw->force_show))) {
      show_frame = TRUE;
    }

    if (show_frame) {

#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("dropped = %d, %d scyc = %ld %d %d\n", dropped, mainw->effort, spare_cycles, requested_frame, sfile->frameno);
#endif
      drop_off = FALSE;

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

      cpuloadval = 0.;

      //g_print("DISK PR is %f\n", mainw->disk_pressure);

      if (!mainw->force_show) {
        if (glob_timing) {
          pthread_mutex_lock(&glob_timing->upd_mutex);
          if (glob_timing->active)
            cpuloadval = glob_timing->curr_cpuload;
          pthread_mutex_unlock(&glob_timing->upd_mutex);
        }
        if (!cpuloadval) {
          cpuload = get_core_loadvar(0);
          cpuloadval = (float)(*cpuload);
        }
      }

      //g_print("CCC %d %d %d %d\n", sfile->frameno, fixed_frame, mainw->actual_frame, best_frame);

      // fixed_fram may be set to force playing of a specific frame, for example when we do have a
      // precached frame and we know it is loaded and ready to use
      // otherwise we aim for the "best_frame" as calculated earlier
      // if none of these are true, then we just advance fwd or back by one frame
      if (!fixed_frame) {
        //if (best_frame > 0 && !is_virtual_frame(mainw->playing_file, best_frame))
        if (best_frame > 0 && (best_frame - sfile->last_frameno) * dir > 0) {
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
          // if the player has been asked to resync with audio, then we need to jump to the requested frame
          // as the audio player will be holding there
          // in this case we accept a small gap in playback as the cost of resyncing
          fixed_frame = sfile->frameno = requested_frame;
          scratch = mainw->scratch = SCRATCH_JUMP_NORESYNC;
        }
      }

      // if the video is jumping (e.g. due to trickplay), then fixed_frame would have been set
      // and in some cases, we can pull in the audio to resync
      if (scratch == SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP;
      if (mainw->scratch == SCRATCH_JUMP) can_realign = TRUE;

      // can change in clamp_frame()
      dir = LIVES_DIRECTION_SIG(sfile->pb_fps);

      if (fixed_frame) sfile->frameno = fixed_frame;

      if (!check_audio_limits(mainw->playing_file, sfile->frameno)) {
        //
        if (mainw->cancelled != CANCEL_NONE) {
          retval = ONE_MILLION + mainw->cancelled;
          goto err_end;
        }
      }

      fixed_frame = clamp_frame(mainw->playing_file, sfile->frameno);

      if (can_realign || fixed_frame != sfile->frameno) {
        sfile->frameno = fixed_frame;
        if (mainw->frame_layer_preload)
          weed_layer_unref(STEAL_POINTER(mainw->frame_layer_preload));
        requested_frame = sfile->last_req_frame = sfile->frameno;
        mainw->scratch = SCRATCH_JUMP_NORESYNC;
      }

      can_realign = FALSE;

      // if we are resyncing with audio, that has already been handled, we do not want to resync more than once
      if (mainw->scratch == SCRATCH_JUMP) mainw->scratch = SCRATCH_JUMP_NORESYNC;

      if (sfile->frames == 1) sfile->frameno = 1;

#ifdef DEBUG_FRAME_TIMNING
      lives_printerr("\nPLAY %d %d %d %d %ld %ld %d %d %.4f\n", sfile->frameno, requested_frame, sfile->last_frameno,
                     mainw->actual_frame, mainw->currticks, mainw->startticks,
                     mainw->video_seek_ready,
                     mainw->audio_seek_ready, cpuloadval);
#endif
#ifndef REC_IDEAL
      can_rec = TRUE;
#endif

      if (cpuloadval < CORE_LOAD_THRESH || mainw->force_show
#ifdef ENABLE_PRECACHE
          || (mainw->pred_frame && is_layer_ready(mainw->frame_layer_preload) == LIVES_RESULT_SUCCESS)
#endif
         ) {
        weed_layer_t *frame_layer;

        // play a frame - on entry, sfile->frameno is the target frame we decided to play
        // mainw->actual_frame is the real (requested) frame
        // sfile->last_frameno is the previous frame played, i.e. the timebase frame

        mainw->actual_frame = requested_frame;

        /// >>>>>>>>> PLAY A FRAME

        //g_print("lfi in  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
        //lives_sleep_while_true(mainw->do_ctx_update);
        frame_layer = load_frame_image(sfile->frameno);
        //g_print("lfi out  @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

        sfile->last_frameno = mainw->actual_frame;
        mainw->fps_mini_measure++;

        ///// >>>>>>>>>>>>>>>>>>>>

        // on return, we have:
        // mainw->actual_frame == the frame that was actually played, this may differ from
        // sfile->frameno if for example a precached frame was played instead
        // we want to play forwards (back) from this then we can set sfilee->last_frameno equal to it
        // however, if actual frame is too far ahead, then we will keep the same last_frameno
        //

        fixed_frame = 0;

        if (getahead > 0 && mainw->pred_frame == getahead) {
          if (!mainw->frame_layer_preload) {
            //g_print("cache hit ! %ld, %d\n", mainw->pred_frame, requested_frame);
            getahead = -1;
          } else if (!weed_layer_check_valid(mainw->frame_layer_preload)) {
            //g_print("cache miss ! %ld, %d\n", mainw->pred_frame, requested_frame);
            getahead = -1;
          }
          /* if (check_getahead) { */
          /*   if (scratch == SCRATCH_NONE) { */
          /*     // this is for predictive caching - if the predicted frame has just loaded, */
          /*     // then we can re-calibrate based on the requested frame */
          /*     recalc_bungle_frames = -mainw->pred_frame; */
          /*   } */
          /*   check_getahead = FALSE; */
          /* } */
        }
        // the player will check the preload frame, and either consume it and set frame_layer_preload to NULL

        // or else set pred_Frame to 0 if the preload is unsuitable
        if ((mainw->frame_layer_preload && !weed_layer_check_valid(mainw->frame_layer_preload))
            || (!mainw->frame_layer_preload && mainw->pred_frame)) {
          cleanup_preload = TRUE;
        }

        if (mainw->pred_frame && !mainw->frame_layer_preload)
          mainw->pred_frame = 0;

        if (!frame_layer || !weed_layer_check_valid(frame_layer))
          frame_invalid = TRUE;
        else frame_invalid = FALSE;

        if (mainw->force_show) {
#ifdef REC_IDEAL
          can_rec = TRUE;
#endif
          mainw->force_show = FALSE;
        }
      }

      scratch = mainw->scratch;
      mainw->scratch = SCRATCH_NONE;
      fixed_frame = 0;

      if (prefs->show_player_stats) mainw->fps_measure++;
      showed_frame = TRUE;
    }

    if (mainw->last_display_ticks == 0) mainw->last_display_ticks = mainw->clock_ticks;
    else {
      if (mainw->vpp && mainw->ext_playback && mainw->vpp->fixed_fpsd > 0.)
        mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->vpp->fixed_fpsd;
      else {
        if (mainw->fixed_fpsd > 0.)
          mainw->last_display_ticks += TICKS_PER_SECOND_DBL / mainw->fixed_fpsd;
        else mainw->last_display_ticks = mainw->clock_ticks;
      }
    }

    // RECORDING //////////

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
        if (frame_invalid || !IS_VALID_CLIP(mainw->scrap_file)) can_rec = FALSE;
        else {
          rec_to_scrap = TRUE;
          mainw->record_frame = mainw->files[mainw->scrap_file]->frames;
        }
      } else mainw->record_frame = requested_frame;

      // we have two recording modes - real and ideal - for ideal we record the request frame
      // as if we just played that. For real method we record the actual frame played.
#ifdef REC_IDEAL
      if ((rec_to_scrap && showed_frame && !frame_invalid)
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
                    if (!await_audio_queue(LIVES_SHORT_TIMEOUT)) {
                      mainw->cancelled = handle_audio_timeout();
                      if (mainw->cancelled != CANCEL_NONE) {
                        retval = ONE_MILLION + mainw->cancelled;
                        goto err_end;
                      }
                    }
                    IF_APLAYER_JACK(jack_get_rec_avals(mainw->jackd);)
                    IF_APLAYER_PULSE(pulse_get_rec_avals(mainw->pulsed);)
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

    // PRE-CACHING (cleanup) /////////

#ifdef ENABLE_PRECACHE
    if (mainw->frame_layer_preload) {
      if (mainw->pred_clip != mainw->playing_file) cleanup_preload = TRUE;
    }
#endif

    // PRE-CACHING //////

    // we have 3 reasons why we may want to preload a frame
    // - the video stream is lagging behind the timecode, and we need to jump ahead
    // - we are playing in reverse and decoding frames out of sequence is slow
    // (TODO) - due to indexing, there is a big jump to the next frame
    // - we have spare cycles which can be utilised to begin preloading the next frame

    // to assist with this, the player is (almost) constantly predicting the next frame
    // and the predictions are compared against reality and this is used to recalibrate the
    // future predictions.

    // Some clipsrcs can also provide time estimates which can be incorporated in the estimates

    // In addition to the predictions, we have other considerations
    // we don;t want to jump to far ahead of the last frame played, unless unavoidable
    // neither do we want to waste resources by loading a frame whcih will be behing the player position
    // - here we need take into account that the player is still advancing while the future frame is being loaded

    // ideally we want to cache a frame just ahead of the timecode but not too far ahead
    // - here we need to account for the fact that the timecode is advancing while we load the frame
    // however, we do have the capacity to cache one frame which can be used at a later time
    // and if the cache is in use we can also hold the preloading

    //best_frame = -1;

#ifdef ENABLE_PRECACHE
    if (cleanup_preload ||
        (mainw->frame_layer_preload && !weed_layer_check_valid(mainw->frame_layer_preload))) {
      if (mainw->frame_layer_preload)
        weed_layer_unref(STEAL_POINTER(mainw->frame_layer_preload));
      mainw->pred_frame = 0;
      mainw->pred_clip = 0;
      cleanup_preload = FALSE;
    }

    if (scratch == SCRATCH_NONE && IS_PHYSICAL_CLIP(mainw->playing_file)) {
      // check last prediction v. reality. The error delta is encoded in bungle frames which we adjust post actively
      if (recalc_bungle_frames) {
        g_print("pt aaa3\n");
        delta = (requested_frame - recalc_bungle_frames);
        recalc_bungle_frames = 0;

        if (1 || prefs->dev_show_caching) {
          g_print("gah (%d) pred = %d, act %d  wanted %d, bungle %d, shouldabeen %d %s", mainw->effort, test_getahead,
                  mainw->actual_frame, requested_frame,
                  bungle_frames, bungle_frames + delta, !getahead ? "(calibrating)" : "");
          if (delta < 0) g_print(" !!!!!\n");
          if (delta == 0) g_print(" EXACT\n");
          if (delta > 0) g_print(" >>>>\n");
        }
        if (delta > 0) {
          if (delta < 3 && bungle_frames > 1) bungle_frames--;
          //else bungle_frames >>= 2;
          else bungle_frames += delta >> 1;

        } else {
          if (delta == 0) bungle_frames++;
          else bungle_frames -= delta;
        }
        if (bungle_frames <= -dir) bungle_frames = 0;
        else bungle_frames -= delta;
        if (bungle_frames > 100) bungle_frames >>= 1;
        check_getahead = TRUE;
      }

      if (!mainw->multitrack && sfile->delivery != LIVES_DELIVERY_PUSH && !mainw->refresh_model
          && !mainw->frame_layer_preload && getahead <= 0) {
        // try to predict the next frame for the player, we use this to try to calibrate our predictions
        // then if we are lagging too far behind, or if playiung in reverse, we calulate a target frame
        // using the average cycle time from the plan runner, we can estimate where the play head will be at the next
        // cycle.
        // having done that we add some extra frames to get ahead of the play position, and either guess the seek
        // time from past jumps, or if the decoder supports it, ask it to check the range going forward, and return
        // the nearest frame that we can reach before the play head arrives there (plus the extra frames)
        if (show_frame) {
          // if we are not already preloading a frame, or if the preload frame has been consumed,
          // check if conditions trigger a new preload frame
          double targ_time = 0.;

          if (LIVES_UNLIKELY(!drop_off && ((sfile->pb_fps < 0. && !clip_can_reverse(mainw->playing_file)) ||
                                           sfile->last_req_frame - sfile->last_frameno >= MAX_JMP_THRESH))) {
            lives_decoder_t *dplug = NULL;

            if (sfile->clip_type == CLIP_TYPE_FILE) {
              lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
              if (srcgrp && srcgrp->n_srcs) {
                lives_clip_src_t *src = get_clip_src(srcgrp, mainw->playing_file, 0, LIVES_SRC_TYPE_DECODER, NULL, NULL);
                if (src) dplug = (lives_decoder_t *)src->actor;
              }
              if (!dplug) {
                if (get_primary_src_type(sfile) == LIVES_SRC_TYPE_DECODER)
                  dplug = (lives_decoder_t *)(get_primary_actor(sfile));
              }
            }

            /* // current requested frame */
            frames_t rframe = sfile->last_req_frame + dir * (dropped + 4);
            frames_t lframe = sfile->last_frameno;

            double tconf = 0.5;

            frames_t min_frame = lframe + dir;
            frames_t max_frame = rframe + dir * (MIN_JMP_THRESH + (sfile->last_req_frame - sfile->last_frameno));

            if (max_frame == min_frame) best_frame = min_frame;
            if ((max_frame - min_frame) * dir > 0) {
              //g_print("best 444 is %d %d %d\n", best_frame, min_frame, max_frame);
              getahead = reachable_frame(mainw->playing_file, dplug,
                                         min_frame, max_frame, rframe,
                                         sfile->pb_fps, &targ_time, &tconf);
              if (!getahead) getahead = -1;
              else {
                //g_print("test %d - %d, %d\n", getahead, sfile->last_req_frame, MIN_JMP_THRESH);
                if ((getahead - sfile->last_req_frame) * dir > MIN_JMP_THRESH) {
                  if (mainw->cached_frame && lives_layer_get_frame(mainw->cached_frame) == getahead)
                    getahead = -1;
                } else getahead = -1;
              }
            }
          }
          if (getahead > 0) {
            best_frame = getahead;
            //g_print("bf2 = %d\n", best_frame);
          }
        }

        if (getahead <= 0 && can_precache && !showed_frame && best_frame > 0) {
          lives_decoder_t *dplug = NULL;
          if (sfile->clip_type == CLIP_TYPE_FILE) {
            lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
            if (srcgrp && srcgrp->n_srcs) {
              lives_clip_src_t *src = get_clip_src(srcgrp, mainw->playing_file, 0,
                                                   LIVES_SRC_TYPE_DECODER, NULL, NULL);
              if (src) dplug = (lives_decoder_t *)src->actor;
            }
            if (!dplug) {
              if (get_primary_src_type(sfile) == LIVES_SRC_TYPE_DECODER)
                dplug = (lives_decoder_t *)(get_primary_actor(sfile));
            }
          }
          best_frame = find_best_frame(dplug, requested_frame, dropped, jumplim, dir);
          if (dir * (best_frame - sfile->last_frameno) < MIN_JMP_THRESH)
            can_precache = FALSE;
          //else g_print("bf3 = %d\n", best_frame);
        } else can_precache = FALSE;

        if (getahead > 0 || can_precache) {
          /* if (best_frame == -1 && (triggered || (!spare_cycles && can_precache) */
          /* 			   || (show_frame && !showed_frame && !fixed_frame))) { */
          /*   if (dropped > skipped) best_frame = sfile->last_req_frame + dir * (1 + dropped); */
          /*   else best_frame = sfile->last_req_frame + dir * (1 + skipped); */
          /*   lagged = (requested_frame - sfile->last_req_frame) * dir; */
          /*   if (lagged < 0) lagged = 0; */
          /*   if (lagged) best_frame += dir; */

          /*   if (dir * (best_frame - requested_frame) < MIN_JMP_THRESH) { */
          /*     best_frame = requested_frame + MIN_JMP_THRESH * dir; */
          /*     if (dir * (best_frame - sfile->last_req_frame) < 1) best_frame = sfile->last_req_frame + dir; */
          /*   } */
          /*   targ_time = ((double)(best_frame - sfile->last_req_frame + 1.) / sfile->pb_fps); */
          /*   plframes = (frames_t)(targ_time / cycle_avg + 1.); */
          /*   if (plframes < 1) plframes = 1; */
          /*   plframes *= skipped; */
          /*   if ((best_frame - sfile->last_req_frame) * dir < plframes + MIN_JMP_THRESH) */
          /*     best_frame = sfile->last_req_frame + (plframes + MIN_JMP_THRESH) * dir; */
          /*   if ((best_frame - sfile->last_req_frame) * dir > plframes + MAX_JMP_THRESH) */
          /*     best_frame = sfile->last_req_frame + (plframes + MAX_JMP_THRESH) * dir; */
          /* } */

          if (best_frame > 0 &&  best_frame != clamp_frame(mainw->playing_file, best_frame)) {
            g_print("pt aaa2\n");
            best_frame = -1;
          }
          if (best_frame == -1) getahead = -1;

#ifdef SHOW_CACHE_PREDICTIONS
          //g_print("PRELOADING (%d %d %lu %p):", sfile->frameno, dropped,
          //spare_cycles, mainw->frame_layer_preload);
#endif
          if (best_frame > 0 && (best_frame - sfile->last_frameno) * dir > MIN_JMP_THRESH) {
            mainw->pred_frame = best_frame;
            mainw->pred_clip = mainw->playing_file;
            /* g_print("CACHE xx1122 %ld %d and %d %d....%d\n", */
            /* 	    mainw->pred_frame, best_frame, sfile->last_req_frame, sfile->last_frameno, getahead); */
            /* 	  g_print("prel\n"); */

            if (mainw->frame_layer_preload) weed_layer_unref(STEAL_POINTER(mainw->frame_layer_preload));

            mainw->frame_layer_preload = lives_layer_new_for_frame(mainw->pred_clip, mainw->pred_frame);
            if (sfile->clip_type == CLIP_TYPE_FILE && is_virtual_frame(mainw->playing_file, mainw->pred_frame)) {
              lives_clipsrc_group_t *srcgrp = get_srcgrp(mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
              if (!srcgrp) {
                g_print("no srcgrp found for clip %d\n", mainw->playing_file);
                srcgrp = clone_srcgrp(mainw->playing_file, mainw->playing_file, 0, SRC_PURPOSE_PRECACHE);
                g_print("added grp %p\n", srcgrp);
              }
              lives_layer_set_srcgrp(mainw->frame_layer_preload, srcgrp);
            }

            /////////////// PRELOAD ////////////
            //g_print("pred frame %ld\n", mainw->pred_frame);
            pull_frame_threaded(mainw->frame_layer_preload, 0, 0);
            //////////////////////////////////////////////////

            if (mainw->pred_clip != -1) {
              if (prefs->dev_show_caching) {
                /* double av = (double)get_aplay_offset() */
                /* 	/ (double)sfile->arate / (double)(sfile->achans * (sfile->asampsize >> 3)); */
                //g_print("cached frame %ld for %d %f\n", mainw->pred_frame, requested_frame, av);
              }
	      // *INDENT-OFF*
	    }}}}}
#ifdef SHOW_CACHE_PREDICTIONS
      //g_print("frame %ld already in cache\n", mainw->pred_frame);
#endif

#endif

    // *INDENT-ON*
    if (mainw->video_seek_ready) {
      if (new_ticks > mainw->startticks) {
        mainw->last_startticks = mainw->startticks;
        mainw->startticks = new_ticks;
	// *INDENT-OFF*
      }}
    // *INDENT-ON*
  }

  cancelled = THREADVAR(cancelled) = mainw->cancelled;
  proc_file = THREADVAR(proc_file) = mainw->playing_file;

  // final section, then player_loop

  if (LIVES_LIKELY(cancelled == CANCEL_NONE)) {
    if (proc_file != mainw->current_file) {
      g_print("ret A2\n");
      return 0;
    } else {
      lives_rfx_t *xrfx;
      // final section, then player_loop
      // VISUAL UPDATES
      if ((xrfx = (lives_rfx_t *)mainw->vrfx_update) != NULL && fx_dialog[1]) {
        // the audio thread wants to update the parameter window
        mainw->vrfx_update = NULL;
        update_visual_params(xrfx, FALSE);
      }

      // the audio thread wants to update the parameter scroll(s)
      if (mainw->ce_thumbs) ce_thumbs_apply_rfx_changes();

      if (!mainw->do_ctx_update && all_updated && !mainw->refresh_model) {
        // redrawing  the embedded frame image and
        // events like fullscreen on / off are not acted on directly, instead these are stacked
        // for execution at this point. The callbacks are triggered and will pass requests to the main
        // thread.

        // TODO - some callbacks do not require a rebuild of the nodemodel, and these can be triggered
        // at any time. Others will do and can only be run when there are no active plan steps
        // type A - drawing updates, soft inits / deinits
        // type B - normal fx toggles, sepwin / fs, mode changes, clip switches

        if (sah->stack) {
          all_updated = FALSE;
          // here we trigger only "light" updates, e.g drawing updates
          lives_proc_thread_add_hook(mainw->player_proc, SYNC_ANNOUNCE_HOOK,
                                     HOOK_OPT_FG_LIGHT, updates_done, NULL);
          mainw->gui_much_events = TRUE;
          BG_THREADVAR(hook_hints) = HOOK_OPT_FG_LIGHT;
          lives_proc_thread_trigger_hooks(mainw->player_proc, SYNC_ANNOUNCE_HOOK);
          BG_THREADVAR(hook_hints) = 0;
        }
      }

      // need to do this every time or we do not capture keypresses
      mainw->do_ctx_update = TRUE;

      if (mainw->new_clip != mainw->playing_file || IS_VALID_CLIP(mainw->close_this_clip)
          || mainw->new_blend_file != mainw->blend_file) goto switch_point;

      // if any player window config changes happened, we need to rebuild the nodemodel
      // with new player target
      if (mainw->refresh_model) rebuild_nodemodel();
    }
    if (!CURRENT_CLIP_IS_VALID) mainw->cancelled = CANCEL_INTERNAL_ERROR;
  }

  if (mainw->cancelled == CANCEL_NONE) goto player_loop;

  retval = MILLIONS(2) + mainw->cancelled;

err_end:
  mainw->jack_can_stop = FALSE;
  g_print("ret A\n");
  return retval;
}


boolean begin_playback(void) {
  audio_start = mainw->play_start;
  mainw->cevent_tc = -1;
  mainw->force_show = TRUE;

  ready_player_one(cfile->next_event ? get_event_timecode(cfile->next_event) : 0);

  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;

  mainw->scratch = SCRATCH_NONE;

  if (mainw->record_starting) {
    if (!record_setup(lives_get_current_playback_ticks(mainw->origticks, NULL))) return FALSE;
  }

  if (mainw->event_list || !CLIP_HAS_VIDEO(mainw->playing_file)) mainw->video_seek_ready = TRUE;
  if (mainw->event_list || !CLIP_HAS_AUDIO(mainw->playing_file)) mainw->audio_seek_ready = TRUE;

  cfile->last_frameno = cfile->frameno = mainw->play_start;

  if (!mainw->playing_sel && (mainw->multitrack || !mainw->event_list)) mainw->play_start = 1;

  if (mainw->multitrack && !mainw->multitrack->is_rendering) {
    // playback start from middle of multitrack
    // calculate when we "would have started" at time 0
    mainw->offsetticks = get_event_timecode(mainw->multitrack->pb_start_event);
  }

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read) {
    mainw->pulsed_read->is_paused = FALSE;
  }
#endif
#ifdef ENABLE_JACK
  if (mainw->jackd_read) {
    mainw->jackd_read->is_paused = FALSE;
  }
#endif

  if (mainw->record) mainw->record_paused = FALSE;

  if (!mainw->proc_ptr && cfile->next_event) {
    /// reset dropped frame count etc
    process_events(NULL, FALSE, 0);
  }

  if (prefs->pbq_adaptive) reset_effort();

  if (mainw->multitrack && !mainw->multitrack->is_rendering) mainw->effort = EFFORT_RANGE_MAX;

  // must call reset_timebase first, since we set playback ticks
  if (!mainw->foreign && !mainw->multitrack) {
    avsync_force();
  } else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;

  process_one();

  //while (1) {
  /*   while (!mainw->internal_messaging && !lives_file_test(cfile->info_file, LIVES_FILE_TEST_EXISTS)) { */
  /*     // just pulse the progress bar, or play video */
  /*     // returns a code if pb stopped */
  /*     int ret = process_one(); */
  /*     if (ret) { */
  /* 	//g_print("pb stopped, reason %d\n", ret); */
  /* 	lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL); */
  /* 	return FALSE; */
  /*     } */

  /*     if ((mainw->disk_mon & MONITOR_QUOTA) && prefs->disk_quota) { */
  /* 	int64_t dsused = disk_monitor_check_result(prefs->workdir); */
  /* 	if (dsused >= 0) { */
  /* 	  capable->ds_used = dsused; */
  /* 	} */
  /* 	disk_monitor_start(prefs->workdir); */
  /* 	mainw->dsu_valid = FALSE; */
  /*     } */

  /*     if (LIVES_UNLIKELY(mainw->agen_needs_reinit)) { */
  /* 	// we are generating audio from a plugin and it needs reinit */
  /* 	// - we do it in this thread so as not to hold up the player thread */
  /* 	reinit_audio_gen(); */
  /*     } */

  /*     if (LIVES_IS_PLAYING && CURRENT_CLIP_IS_VALID && cfile->play_paused) */
  /* 	lives_usleep(prefs->sleep_time); */

  /*     // normal playback, with realtime audio player */
  /*     if (mainw->whentostop != STOP_ON_AUD_END) continue; */

  /*     if (LIVES_UNLIKELY(mainw->agen_needs_reinit)) { */
  /* 	// we are generating audio from a plugin and it needs reinit */
  /* 	// - we do it in this thread so as not to hold up the player thread */
  /* 	reinit_audio_gen(); */
  /*     } */
  /*   } */
  /* } */

  //finish:
  //play/operation ended

#ifdef DEBUG
  g_print("exiting progress dialog\n");
#endif
  return TRUE;
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
// attributes are obj status, source, rate, channs, sampsize, is float, audio status, endian, interleaved, data len, data
// TODO - will be part of OBJ_INTENTION_CREATE_INSTANCE (req == subtype)
lives_obj_instance_t *lives_player_inst_create(uint64_t subtype) {
  char *choices[2];
  weed_plant_t *gui;
  lives_obj_attr_t *attr;
  weed_plant_t  *inst = lives_obj_instance_create(OBJECT_TYPE_PLAYER, subtype);
  lives_object_include_states(inst, OBJECT_STATE_PREPARED);
  attr = lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_SOURCE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_SOURCE, _("Source"), WEED_PARAM_INTEGER);
  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  choices[0] = lives_strdup("Internal");
  choices[1] = lives_strdup("External");
  weed_set_string_array(gui, WEED_LEAF_CHOICES, 2, choices);
  lives_free(choices[0]); lives_free(choices[1]);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_RATE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_RATE, _("Rate Hz"), WEED_PARAM_INTEGER);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_CHANNELS, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_CHANNELS, _("Channels"), WEED_PARAM_INTEGER);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_SAMPSIZE, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_SAMPSIZE, _("Sample size (bits)"), WEED_PARAM_INTEGER);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_STATUS, WEED_SEED_INT64);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_SIGNED, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_SIGNED, _("Signed"), WEED_PARAM_SWITCH);
  attr = lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_ENDIAN, WEED_SEED_INT);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_ENDIAN, _("Endian"), WEED_PARAM_INTEGER);
  gui = weed_plant_new(WEED_PLANT_GUI);
  weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  choices[0] = lives_strdup("Little endian");
  choices[1] = lives_strdup("Big endian");
  weed_set_string_array(gui, WEED_LEAF_CHOICES, 2, choices);
  lives_free(choices[0]); lives_free(choices[1]);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_FLOAT, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_FLOAT, _("Is float"), WEED_PARAM_SWITCH);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_INTERLEAVED, WEED_SEED_BOOLEAN);
  lives_attribute_set_param_type(inst, ATTR_AUDIO_INTERLEAVED, _("Interleaved"), WEED_PARAM_SWITCH);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_DATA_LENGTH, WEED_SEED_INT64);
  lives_obj_instance_declare_attribute(inst, ATTR_AUDIO_DATA, WEED_SEED_VOIDPTR);
  return inst;
}

