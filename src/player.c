// player.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "callbacks.h"
#include "resample.h"

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

  if (*tsource == LIVES_TIME_SOURCE_NONE) {
#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop && (prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) &&
        (prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT) && !(mainw->record && !(prefs->rec_opts & REC_FRAMES))) {
      // calculate the time from jack transport
      *tsource = LIVES_TIME_SOURCE_EXTERNAL;
      current = jack_transport_get_current_ticks();
    }
#endif
  }

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
  /// it can be helpful to imagine a virtual clock which is at currrent time:
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
  /// hence we need mainw->syncticks --> a global adjustment which is independant of the clock source. This is similar to
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


void calc_aframeno(int fileno) {
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && ((mainw->jackd && mainw->jackd->playing_file == fileno) ||
      (mainw->jackd_read && mainw->jackd_read->playing_file == fileno))) {
    // get seek_pos from jack
    if (mainw->jackd_read) mainw->aframeno = lives_jack_get_pos(mainw->jackd_read) * cfile->fps + 1.;
    else mainw->aframeno = lives_jack_get_pos(mainw->jackd) * cfile->fps + 1.;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && ((mainw->pulsed && mainw->pulsed->playing_file == fileno) ||
      (mainw->pulsed_read && mainw->pulsed_read->playing_file == fileno))) {
    // get seek_pos from pulse
    if (mainw->pulsed_read) mainw->aframeno = lives_pulse_get_pos(mainw->pulsed_read) * cfile->fps + 1.;
    else mainw->aframeno = lives_pulse_get_pos(mainw->pulsed) * cfile->fps + 1.;
  }
#endif
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
  int aplay_file = fileno;

  if (!sfile) return 0;

  cframe = sfile->last_frameno;
  if (norecurse) return cframe;

  if (sfile->achans > 0) {
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE) {
      if (mainw->pulsed) aplay_file = mainw->pulsed->playing_file;
    }
#endif
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      if (mainw->jackd) aplay_file = mainw->jackd->playing_file;
    }
#endif
    if (!IS_VALID_CLIP(aplay_file)) aplay_file = -1;
    else {
      if (fileno != aplay_file) {
        off64_t aseek_pos_delta = (off64_t)((double)dtc / TICKS_PER_SECOND_DBL * (double)(sfile->arate))
                                  * sfile->achans * sfile->asampsize / 8;
        if (sfile->adirection == LIVES_DIRECTION_FORWARD) sfile->aseek_pos += aseek_pos_delta;
        else sfile->aseek_pos -= aseek_pos_delta;
        if (sfile->aseek_pos < 0 || sfile->aseek_pos > sfile->afilesize) {
          nloops = sfile->aseek_pos / sfile->afilesize;
          if (mainw->ping_pong && (sfile->adirection == LIVES_DIRECTION_REVERSE || clip_can_reverse(fileno))) {
            sfile->adirection += nloops;
            sfile->adirection &= 1;
            if (sfile->adirection == LIVES_DIRECTION_BACKWARD && !clip_can_reverse(fileno))
              sfile->adirection = LIVES_DIRECTION_REVERSE;
          }
          sfile->aseek_pos -= nloops * sfile->afilesize;
          if (sfile->adirection == LIVES_DIRECTION_REVERSE) sfile->aseek_pos = sfile->afilesize - sfile->aseek_pos;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

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
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps + .00001);
  else
    nframe = cframe + (frames_t)((double)dtc / TICKS_PER_SECOND_DBL * fps - .00001);

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
        /// the frame number changed, but we will recalulate the value using mainw->deltaticks
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
      if (!mainw->foreign && fileno == mainw->playing_file &&
          mainw->scratch == SCRATCH_JUMP && (!mainw->event_list || mainw->record || mainw->record_paused) &&
          (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
        resync_audio(nframe);
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
      if (mainw->whentostop == STOP_ON_VID_END) {
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

  if (fileno == mainw->playing_file && prefs->audio_src == AUDIO_SRC_INT && fileno == aplay_file && sfile->achans > 0
      && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
      && (mainw->scratch != SCRATCH_NONE && mainw->scratch != SCRATCH_JUMP_NORESYNC)) {
    if (mainw->whentostop == STOP_ON_AUD_END) {
      // check if audio stopped playback. The audio player will also be checking this, BUT: we have to check here too
      // before doing any resync, otherwise the video can loop and if the audio is then resynced it may never reach the end
      calc_aframeno(fileno);
      if (!check_for_audio_stop(fileno, first_frame + 1, last_frame - 1)) {
        mainw->cancelled = CANCEL_AUD_END;
        mainw->scratch = SCRATCH_NONE;
        return 0;
      }
      resync_audio(nframe);
      if (mainw->scratch == SCRATCH_JUMP) {
        mainw->video_seek_ready = TRUE;   /// ????
        mainw->scratch = SCRATCH_JUMP_NORESYNC;
      }
    }
  }
  if (fileno == mainw->playing_file) {
    if (mainw->scratch != SCRATCH_NONE) {
      sfile->last_frameno = nframe;
      mainw->scratch = SCRATCH_JUMP_NORESYNC;
    }
  }
  return nframe;
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
