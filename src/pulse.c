// pulse.c
// LiVES (lives-exe)
// (c) G. Finch <salsaman+lives@gmail.com> 2005 - 2020
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO
#include "main.h"
#include "callbacks.h"
#include "effects.h"
#include "effects-weed.h"

#define afile mainw->files[pulsed->playing_file]

//#define DEBUG_PULSE

#define THRESH_BASE 10000.
#define THRESH_MAX 50000.

static pulse_driver_t pulsed;
static pulse_driver_t pulsed_reader;

static pa_threaded_mainloop *pa_mloop = NULL;
static pa_context *pcon = NULL;
static char pactxnm[512];

static uint32_t pulse_server_rate = 0;

#define PULSE_READ_BYTES 48000

static uint8_t prbuf[PULSE_READ_BYTES * 2];

static size_t prb = 0;

static boolean seek_err;
static boolean sync_ready = FALSE;

static volatile int lock_count = 0;

static off_t fwd_seek_pos = 0;

///////////////////////////////////////////////////////////////////


LIVES_GLOBAL_INLINE void pa_mloop_lock(void) {
  if (!pa_threaded_mainloop_in_thread(pa_mloop)) {
    pa_threaded_mainloop_lock(pa_mloop);
    ++lock_count;
  } else {
    LIVES_ERROR("tried to lock pa mainloop within audio thread");
  }
}

LIVES_GLOBAL_INLINE void pa_mloop_unlock(void) {
  if (!pa_threaded_mainloop_in_thread(pa_mloop)) {
    if (lock_count > 0) {
      --lock_count;
      pa_threaded_mainloop_unlock(pa_mloop);
    }
  } else {
    LIVES_ERROR("tried to unlock pa mainloop within audio thread");
  }
}


static void pulse_server_cb(pa_context *c, const pa_server_info *info, void *userdata) {
  if (!info) pulse_server_rate = 0;
  else pulse_server_rate = info->sample_spec.rate;
  pa_threaded_mainloop_signal(pa_mloop, 0);
}

static void pulse_success_cb(pa_stream *stream, int i, void *userdata) {pa_threaded_mainloop_signal(pa_mloop, 0);}

#include <sys/time.h>
#include <sys/resource.h>

static void stream_underflow_callback(pa_stream *s, void *userdata) {
  // we get isolated cases when the GUI is very busy, for example right after playback
  // we should ignore these isolated cases, except in DEBUG mode.
  // otherwise - increase tlen and possibly maxlen ?
  // e.g. pa_stream_set_buffer_attr(s, battr, success_cb, NULL);

  if (prefs->show_dev_opts) {
    fprintf(stderr, "PA Stream underrun.\n");
  }

  mainw->uflow_count++;
}


static void stream_overflow_callback(pa_stream *s, void *userdata) {
  pa_operation *paop;
  //pulse_driver_t *pulsed = (pulse_driver_t *)userdata;
  fprintf(stderr, "Stream overrun.\n");
  paop = pa_stream_flush(s, NULL, NULL);
  pa_operation_unref(paop);
  //break_me();
}


static void stream_moved_callback(pa_stream *s, void *userdata) {
  //pulse_driver_t *pulsed = (pulse_driver_t *)userdata;
  fprintf(stderr, "Stream moved. \n");
}


static void stream_buffer_attr_callback(pa_stream *s, void *userdata) {
  //fprintf(stderr, "Stream ba changed. \n");
}


boolean lives_pulse_init(short startup_phase) {
  // startup pulseaudio server
  char *msg;
  pa_context_state_t pa_state;
  ticks_t timeout;
  lives_alarm_t alarm_handle;
  LiVESResponseType resp;
  boolean retried = FALSE;

  if (pa_mloop) return TRUE;

retry:

  pa_mloop = pa_threaded_mainloop_new();
  lives_snprintf(pactxnm, 512, "LiVES-%"PRId64, lives_random());
  pcon = pa_context_new(pa_threaded_mainloop_get_api(pa_mloop), pactxnm);
  pa_context_connect(pcon, NULL, (pa_context_flags_t)0, NULL);
  pa_threaded_mainloop_start(pa_mloop);

  pa_state = pa_context_get_state(pcon);

  alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
  while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pa_state != PA_CONTEXT_READY) {
    sched_yield();
    lives_usleep(prefs->sleep_time);
    pa_state = pa_context_get_state(pcon);
  }
  lives_alarm_clear(alarm_handle);

  if (pa_context_get_state(pcon) == PA_CONTEXT_READY) timeout = 1;

  if (timeout == 0) {
    pa_context_unref(pcon);
    pcon = NULL;
    pulse_shutdown();
    if (!retried) {
      retried = TRUE;
      goto retry;
    }
    LIVES_WARN("Unable to connect to the pulseaudio server");

    if (!mainw->foreign) {
      if (startup_phase != 2) {
        resp = do_abort_retry_cancel_dialog(
                 _("\nUnable to connect to the pulseaudio server.\n"
                   "Click Abort to exit from LiVES, Retry to try again,\n"
                   "or Cancel to run LiVES without audio features.\n"
                   "Audio settings can be updated in Tools/Preferences/Playback.\n"));
        if (resp == LIVES_RESPONSE_RETRY) {
          fprintf(stderr, "Retrying...\n");
          goto retry;
        }
        fprintf(stderr, "Giving up.\n");
        switch_aud_to_none(TRUE);
      } else {
        msg = (_("\nUnable to connect to the pulseaudio server.\n"));
        if (startup_phase != 2) {
          do_error_dialog(msg);
          mainw->aplayer_broken = TRUE;
        } else {
          do_error_dialogf("%s%s", msg, _("LiVES will exit and you can choose another audio player.\n"));
        }
        lives_free(msg);
      }
    }
    return FALSE;
  }
  return TRUE;
}


void pulse_get_rec_avals(pulse_driver_t *pulsed) {
  mainw->rec_aclip = pulsed->playing_file;
  if (mainw->rec_aclip != -1) {
    mainw->rec_aseek = fabs((double)(fwd_seek_pos / (double)(afile->achans * afile->asampsize / 8)) / (double)afile->arps)
                       + (double)(mainw->startticks - mainw->currticks) / TICKS_PER_SECOND_DBL;
    mainw->rec_avel = fabs((double)pulsed->in_arate / (double)afile->arps) * (double)afile->adirection;
    //g_print("RECSEEK is %f %ld\n", mainw->rec_aseek, pulsed->real_seek_pos);
  }
}


static void pulse_set_rec_avals(pulse_driver_t *pulsed) {
  // record direction change (internal)
  mainw->rec_aclip = pulsed->playing_file;
  if (mainw->rec_aclip != -1) {
    pulse_get_rec_avals(pulsed);
  }
}


LIVES_GLOBAL_INLINE size_t pulse_get_buffsize(pulse_driver_t *pulsed) {return pulsed->chunk_size;}

#if !HAVE_PA_STREAM_BEGIN_WRITE
static void pulse_buff_free(void *ptr) {lives_free(ptr);}
#endif

static void sync_ready_ok(pulse_driver_t *pulsed, size_t nbytes) {
  if (nbytes >= 8192) {
    mainw->syncticks += ((double)nbytes / (double)(pulsed->out_arate) * 1000000.
                         / (double)(pulsed->out_achans * pulsed->out_asamps >> 3) + .5) * USEC_TO_TICKS;
  } else {
    mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
  }
  mainw->fps_mini_measure = 0;
  mainw->fps_mini_ticks = mainw->currticks;
  pthread_mutex_lock(&mainw->avseek_mutex);
  mainw->audio_seek_ready = TRUE;
  pthread_cond_signal(&mainw->avseek_cond);
  pthread_mutex_unlock(&mainw->avseek_mutex);
}


static void sample_silence_pulse(pulse_driver_t *pulsed, size_t nbytes, size_t xbytes) {
  uint8_t *buff;
  int nsamples;

  if (sync_ready) sync_ready_ok(pulsed, xbytes > 0 ? xbytes : 0);

  if (xbytes <= 0) return;
  if (mainw->aplayer_broken) return;
  while (nbytes > 0) {
    int ret = 0;
#if HAVE_PA_STREAM_BEGIN_WRITE
    xbytes = -1;
    // returns a buffer and size for us to write to
    pa_stream_begin_write(pulsed->pstream, (void **)&buff, &xbytes);
#endif
    if (nbytes < xbytes) xbytes = nbytes;
#if !HAVE_PA_STREAM_BEGIN_WRITE
    buff = (uint8_t *)lives_calloc(xbytes);
#endif
    if (!buff || ret != 0) return;

#if HAVE_PA_STREAM_BEGIN_WRITE
    lives_memset(buff, 0, xbytes);
#endif
    if (pulsed->astream_fd != -1) audio_stream(buff, xbytes, pulsed->astream_fd); // old streaming API

    nsamples = xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);

    // streaming API
    if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float && pulsed->playing_file != -1
        && pulsed->playing_file != mainw->ascrap_file) {
      sample_silence_stream(pulsed->out_achans, nsamples);
    }

    if (mainw->afbuffer && prefs->audio_src != AUDIO_SRC_EXT
        && (!mainw->event_list || mainw->record || mainw->record_paused))  {
      // buffer audio for any generators
      // interleaved, so we paste all to channel 0
      append_to_audio_buffer16(buff, nsamples * pulsed->out_achans, 2);
    }
#if !HAVE_PA_STREAM_BEGIN_WRITE
    pa_stream_write(pulsed->pstream, buff, xbytes, pulse_buff_free, 0, PA_SEEK_RELATIVE);
#else
    pa_stream_write(pulsed->pstream, buff, xbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
    nbytes -= xbytes;
    if (!pulsed->is_paused) pulsed->frames_written += nsamples;
    pulsed->extrausec += ((double)xbytes / (double)(pulsed->out_arate) * 1000000.
                          / (double)(pulsed->out_achans * pulsed->out_asamps >> 3) + .5);
  }
  pulsed->real_seek_pos = pulsed->seek_pos;
  if (IS_VALID_CLIP(pulsed->playing_file) && pulsed->seek_pos < afile->afilesize)
    afile->aseek_pos = pulsed->seek_pos;
}


#define NBYTES_LIMIT (65536 * 4)

static short *shortbuffer = NULL;

/**
   @brief write audio to pulse

   PULSE AUDIO calls this periodically to get the next audio buffer
   note the buffer size can, and does, change on each call, making it inefficient to use ringbuffers

   there are three main modes of operation (plus silence)
   - playing back from a file; in this case we keep track of the seek position so we know when we hit a boundary
   -- would probably better to have another thread cache the audio like the jack player does, currently we just read from
   the audio file (using buffered reads would be not really help since we also play in reverse) and in addition the buffer size
   is not constant

   - playing from a memory buffer; this mode is used in multitrack or when reproducing a recording (event_list);
   since the audio composition is known in advance it is possible to prepare audio blocks, apply effects and volume mixer levels
   in advance

   - playing from an audio generator; we just run the plugin and request the correct number of samples.

   we also resample / change formats as needed.

   currently we don't support end to end float, but that is planned for the very near future.

   During playback we always send audio, even if it is silence, since the video timings may be derived from the
   audio sample count.
*/
static void pulse_audio_write_process(pa_stream *pstream, size_t nbytes, void *arg) {
  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  pa_operation *paop;
  aserver_message_t *msg;
  ssize_t pad_bytes = 0;
  uint8_t *buffer;
  uint64_t nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
  off_t offs = 0;
  size_t xbytes = pa_stream_writable_size(pstream);
  off_t seek, xseek;
  pa_volume_t pavol;
  char *filename;

  boolean got_cmd = FALSE;
  boolean from_memory = FALSE;
  boolean needs_free = FALSE;

  int new_file;

  sync_ready = FALSE;

#ifdef USE_RPMALLOC
  if (!rpmalloc_is_thread_initialized()) {
    rpmalloc_thread_initialize();
  }
#endif
  //pa_thread_make_realtime(50);
  //g_print("PA\n");
  pulsed->real_seek_pos = pulsed->seek_pos;
  pulsed->pstream = pstream;

  if (xbytes > nbytes) xbytes = nbytes;

  if (!mainw->is_ready || !pulsed || (!LIVES_IS_PLAYING && !pulsed->msgq) || nbytes > NBYTES_LIMIT) {
    sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
    //g_print("pt a1 %ld %d %p %d %p %ld\n",nsamples, mainw->is_ready, pulsed, mainw->playing_file, pulsed->msgq, nbytes);
    return;
  }

  /// handle control commands from the main (video) thread
  if ((msg = (aserver_message_t *)pulsed->msgq) != NULL) {
    got_cmd = TRUE;
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      pulsed->in_use = TRUE;
      paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
      pa_operation_unref(paop);
      new_file = atoi((char *)msg->data);
      if (pulsed->playing_file != new_file) {
        filename = lives_get_audio_file_name(new_file);
        pulsed->fd = lives_open_buffered_rdonly(filename);
        if (pulsed->fd == -1) {
          // dont show gui errors - we are running in realtime thread
          LIVES_ERROR("pulsed: error opening");
          LIVES_ERROR(filename);
          pulsed->playing_file = -1;
        }
        lives_free(filename);
      }
      fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = 0;
      pulsed->playing_file = new_file;
      //pa_stream_trigger(pulsed->pstream, NULL, NULL); // only needed for prebuffer
      break;
    case ASERVER_CMD_FILE_CLOSE:
      pulsed->in_use = TRUE;
      paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
      pa_operation_unref(paop);
      if (pulsed->fd >= 0) lives_close_buffered(pulsed->fd);
      if (pulsed->sound_buffer == pulsed->aPlayPtr->data) pulsed->sound_buffer = NULL;
      if (pulsed->aPlayPtr->data) {
        lives_free((void *)(pulsed->aPlayPtr->data));
        pulsed->aPlayPtr->data = NULL;
      }
      pulsed->aPlayPtr->max_size = pulsed->aPlayPtr->size = 0;
      pulsed->fd = pulsed->playing_file = -1;
      pulsed->in_use = FALSE;
      pulsed->seek_pos = pulsed->real_seek_pos = fwd_seek_pos = 0;
      break;
    case ASERVER_CMD_FILE_SEEK:
      if (pulsed->fd < 0) break;
      pulsed->in_use = TRUE;
      paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
      pa_operation_unref(paop);
      xseek = seek = atol((char *)msg->data);
      /// when we get a seek request, here is what we will do:
      /// - any time a video seek is requested, video_seek_ready and audio_seek_ready are both set to FALSE
      /// - once the video player is about to show a frame, it will set video_seek_ready, and cond_mutex wait
      /// - if (volatile) video_seek_ready is FALSE:
      /// -- seek to the point
      /// -- play silence until video_seek_ready is TRUE
      /// - once video_seek_ready is TRUE, adjust by delta time, prefill the buffer
      ///   and cond_mutex_signal so video can continue

      //g_print("xseek is %ld\n", xseek);
      if (seek < 0.) xseek = 0.;
      xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3));
      lives_lseek_buffered_rdonly_absolute(pulsed->fd, xseek);
      fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = afile->aseek_pos = xseek;
      if (pulsed->playing_file == mainw->ascrap_file || afile->adirection == LIVES_DIRECTION_FORWARD) {
        lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
      } else {
        lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
      }
      break;
    default:
      pulsed->msgq = NULL;
      msg->data = NULL;
    }
    if (msg->next != msg) lives_freep((void **) & (msg->data));
    msg->command = ASERVER_CMD_PROCESSED;
    pulsed->msgq = msg->next;
    if (pulsed->msgq && pulsed->msgq->next == pulsed->msgq) pulsed->msgq->next = NULL;
  }
  if (got_cmd) {
    sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
    return;
  }

  /// this is the value we will return from pulse_get_rec_avals
  fwd_seek_pos = pulsed->real_seek_pos;

  if (pulsed->chunk_size != nbytes) pulsed->chunk_size = nbytes;

  pulsed->state = pa_stream_get_state(pulsed->pstream);

  if (pulsed->state == PA_STREAM_READY) {
    uint64_t pulseFramesAvailable = nsamples;
    uint64_t inputFramesAvailable = 0;
    uint64_t numFramesToWrite = 0;
    double in_framesd = 0.;
    float clip_vol = 1.;
#ifdef DEBUG_PULSE
    int64_t in_frames = 0;
#endif
    size_t in_bytes = 0, xin_bytes = 0;
    /** the ratio of samples in : samples out - may be negative. This is NOT the same as the velocity as it incluides a resampling factor */
    float shrink_factor = 1.f;
    int swap_sign;
    int qnt = 1;

    if (IS_VALID_CLIP(pulsed->playing_file)) qnt = afile->achans * (afile->asampsize >> 3);

#ifdef DEBUG_PULSE
    lives_printerr("playing... pulseFramesAvailable = %ld\n", pulseFramesAvailable);
#endif

    pulsed->num_calls++;

    /// not in use, just send silence
    if (!pulsed->in_use || (pulsed->read_abuf > -1 && !LIVES_IS_PLAYING)
        || ((((pulsed->fd < 0 || pulsed->seek_pos < 0 ||
               (!mainw->multitrack && IS_VALID_CLIP(pulsed->playing_file) && pulsed->seek_pos > afile->afilesize))
              && pulsed->read_abuf < 0) && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack))
            || pulsed->is_paused || (mainw->pulsed_read && mainw->pulsed_read->playing_file != -1))) {

      if (pulsed->seek_pos < 0 && IS_VALID_CLIP(pulsed->playing_file) && pulsed->in_arate > 0) {
        pulsed->seek_pos += (double)(pulsed->in_arate / pulsed->out_arate) * nsamples * qnt;
        pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
        if (pulsed->seek_pos > 0) {
          pad_bytes = -pulsed->real_seek_pos;
          pulsed->real_seek_pos = pulsed->seek_pos = 0;
        }
      }

      if (IS_VALID_CLIP(pulsed->playing_file) && !mainw->multitrack
          && pulsed->seek_pos > afile->afilesize && pulsed->in_arate < 0) {
        pulsed->seek_pos += (pulsed->in_arate / pulsed->out_arate) * nsamples * pulsed->in_achans * pulsed->in_asamps / 8;
        pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos - qnt, qnt);
        if (pulsed->seek_pos < afile->afilesize) {
          pad_bytes = (afile->afilesize - pulsed->real_seek_pos); // -ve val
          pulsed->real_seek_pos = pulsed->seek_pos = afile->afilesize;
        }
      }
#ifdef DEBUG_PULSE
      g_print("pt a3 %d\n", pulsed->in_use);
#endif

      if (!pad_bytes) {
        sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * pulsed->out_asamps >> 3, xbytes);
        return;
      }
    }

    if (!mainw->audio_seek_ready && pulsed->playing_file != mainw->playing_file)
      mainw->audio_seek_ready = TRUE;

    if (!mainw->audio_seek_ready) {
      double dqnt;
      size_t qnt;
      int64_t rnd_samp;
      frames_t rnd_frame;
      if (!mainw->video_seek_ready) {
        int64_t xusec = pulsed->extrausec;
        sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * pulsed->out_asamps >> 3, xbytes);
        //pulsed->seek_pos += xbytes;
        fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;
        pulsed->usec_start += (pulsed->extrausec - xusec);
        pulsed->extrausec = xusec;
        //mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
        return;
        /// adjustment is .5 (rounding factor) + (if we switched clips) 1 frame (because video advances 1) + 1 (????)
      }
      dqnt = (double)afile->achans * afile->asampsize / 8.;
      qnt = afile->achans * (afile->asampsize >> 3);
      /* g_print("@ SYNCxx %d seek pos %ld = %f  ct %ld   st %ld\n", mainw->actual_frame, pulsed->seek_pos, */
      /*         ((double)pulsed->seek_pos / (double)afile->arps / 4. * afile->fps + 1.), mainw->currticks, mainw->startticks); */
      /* pulsed->seek_pos += afile->adirection * (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL */
      /*                     * (double)(afile->arps  * qnt); */
      rnd_frame = (frames_t)((double)pulsed->seek_pos / (double)afile->arate / dqnt * afile->fps
                             + (afile->last_play_sequence != mainw->play_sequence ? .000001 : .5));
      //g_print("RND frame is %d\n", rnd_frame + 1);
      //g_print("VALXXX %d %d %d\n", mainw->play_sequence, afile->last_play_sequence, mainw->switch_during_pb);
      rnd_frame += afile->adirection * (mainw->switch_during_pb && afile->last_play_sequence == mainw->play_sequence ? 1 : 0);
      mainw->switch_during_pb = FALSE;
      afile->last_play_sequence = mainw->play_sequence;
      rnd_samp = (int64_t)((double)(rnd_frame + .00001) / afile->fps * (double)afile->arate + .5);
      pulsed->seek_pos = (ssize_t)(rnd_samp * qnt);
      //g_print("rndfr = %d rnt = %ld skpo = %ld\n", rnd_frame + 1, rnd_samp, pulsed->seek_pos);
      pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
      lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
      //g_print("seek to %ld %ld\n", pulsed->seek_pos, (int64_t)((double)pulsed->seek_pos / 48000. / 4. * afile->fps) + 1);
      fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;

      if (pulsed->playing_file == mainw->ascrap_file || afile->adirection == LIVES_DIRECTION_FORWARD) {
        lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
      } else {
        lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
      }

      shrink_factor = (float)pulsed->in_arate / (float)pulsed->out_arate / mainw->audio_stretch;
      in_framesd = fabs((double)shrink_factor * (double)pulseFramesAvailable);

      // preload the buffer for first read
      in_bytes = (size_t)(in_framesd * pulsed->in_achans * (pulsed->in_asamps >> 3));
      lives_read_buffered(pulsed->fd, NULL, in_bytes * 8, TRUE);
      lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
      //mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);

      /* g_print("@ SYNC %d seek pos %ld = %f  ct %ld   st %ld\n", mainw->actual_frame, pulsed->seek_pos, */
      /*         ((double)pulsed->seek_pos / (double)afile->arps / 4. * afile->fps + 1.), mainw->currticks, mainw->startticks); */
      sync_ready = TRUE;
    }

    if (LIVES_LIKELY(pulseFramesAvailable > 0 && (pulsed->read_abuf > -1
                     || (pulsed->aPlayPtr && pulsed->in_achans > 0) ||
                     (((mainw->agen_key != 0 || mainw->agen_needs_reinit)
                       && !mainw->preview) && !mainw->multitrack)))) {
      if (LIVES_IS_PLAYING && pulsed->read_abuf > -1) {
        // playing back from memory buffers instead of from file
        // this is used in multitrack
        from_memory = TRUE;
        numFramesToWrite = pulseFramesAvailable;
      } else {
        // from file or audio generator
        if (LIVES_LIKELY(pulsed->fd >= 0)) {
          int playfile = mainw->playing_file;
          pulsed->seek_end = 0;
          if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && IS_VALID_CLIP(pulsed->playing_file)) {
            if (mainw->playing_sel) {
              pulsed->seek_end = (int64_t)((double)(afile->end - 1.) / afile->fps * afile->arps) * afile->achans
                                 * (afile->asampsize / 8);
              if (pulsed->seek_end > afile->afilesize) pulsed->seek_end = afile->afilesize;
            } else {
              if (!mainw->loop_video) pulsed->seek_end = (int64_t)((double)(mainw->play_end - 1.) / afile->fps * afile->arps)
                    * afile->achans * (afile->asampsize / 8);
              else pulsed->seek_end = afile->afilesize;
            }
            if (pulsed->seek_end > afile->afilesize) pulsed->seek_end = afile->afilesize;
          }
          if (pulsed->seek_end == 0 || ((pulsed->playing_file == mainw->ascrap_file && !mainw->preview) && IS_VALID_CLIP(playfile)
                                        && mainw->files[playfile]->achans > 0)) pulsed->seek_end = INT64_MAX;

          /// calculate how much to read
          pulsed->aPlayPtr->size = 0;

          shrink_factor = (float)pulsed->in_arate / (float)pulsed->out_arate / mainw->audio_stretch;
          in_framesd = fabs((double)shrink_factor * (double)pulseFramesAvailable);

          // add in a small random factor so on longer timescales we aren't losing or gaining samples
          in_bytes = (int)(in_framesd + ((double)fastrand() / (double)LIVES_MAXUINT64))
                     * pulsed->in_achans * (pulsed->in_asamps >> 3);

#ifdef DEBUG_PULSE
          in_frames = in_bytes / pulsed->in_achans * (pulsed->in_asamps >> 3);
          g_print("in bytes=%ld %d %d %lu %lu %lu\n", in_bytes, pulsed->in_arate, pulsed->out_arate, pulseFramesAvailable,
                  pulsed->in_achans, pulsed->in_asamps);
#endif

          /// expand the buffer if necessary
          if (LIVES_UNLIKELY((in_bytes > pulsed->aPlayPtr->max_size && !(*pulsed->cancelled) && fabsf(shrink_factor) <= 100.f))) {
            boolean update_sbuffer = FALSE;
            if (pulsed->sound_buffer == pulsed->aPlayPtr->data) update_sbuffer = TRUE;
            if (pulsed->aPlayPtr->data) lives_free((void *)(pulsed->aPlayPtr->data));
            pulsed->aPlayPtr->data = lives_calloc_safety(in_bytes >> 2, 4);
            if (update_sbuffer) pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data);
            if (pulsed->aPlayPtr->data) pulsed->aPlayPtr->max_size = in_bytes;
            else pulsed->aPlayPtr->max_size = 0;
          }

          // update looping mode
          if (mainw->whentostop == NEVER_STOP || mainw->loop_cont) {
            if (mainw->ping_pong && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
                && ((prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) || mainw->current_file == pulsed->playing_file)
                && (!mainw->event_list || mainw->record || mainw->record_paused)
                && mainw->agen_key == 0 && !mainw->agen_needs_reinit
                && (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
                    || ((prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG))))
              pulsed->loop = AUDIO_LOOP_PINGPONG;
            else pulsed->loop = AUDIO_LOOP_FORWARD;
          } else {
            pulsed->loop = AUDIO_LOOP_NONE;
            //in_bytes = 0;
          }

          pulsed->aPlayPtr->size = 0;

          if (shrink_factor  > 0.) {
            // forward playback
            if ((mainw->agen_key == 0 || mainw->multitrack || mainw->preview) && in_bytes > 0) {
              lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
              if (pad_bytes < 0) pad_bytes = 0;
              else {
                pad_bytes *= shrink_factor;
                pad_bytes = ALIGN_CEIL64(pad_bytes - qnt, qnt);
              }
              if (pad_bytes) lives_memset((void *)pulsed->aPlayPtr->data, 0, pad_bytes);
              pulsed->aPlayPtr->size = lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data + pad_bytes),
                                       in_bytes - pad_bytes, TRUE) + pad_bytes;
            } else pulsed->aPlayPtr->size = in_bytes;
            pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data);
            pulsed->seek_pos += in_bytes - pad_bytes;
            if (pulsed->seek_pos >= pulsed->seek_end && !afile->opening) {
              ssize_t rem = pulsed->seek_end - pulsed->real_seek_pos;
              if (pulsed->aPlayPtr->size + rem > in_bytes) rem = in_bytes - pulsed->aPlayPtr->size;
              if (rem > 0)
                pulsed->aPlayPtr->size += lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data)
                                          + pulsed->aPlayPtr->size,
                                          pulsed->seek_end - pulsed->real_seek_pos, TRUE);

              if (pulsed->loop == AUDIO_LOOP_NONE) {
                if (*pulsed->whentostop == STOP_ON_AUD_END) *pulsed->cancelled = CANCEL_AUD_END;
                in_bytes = 0;
                pulsed->in_use = FALSE;
              } else {
                if (pulsed->loop == AUDIO_LOOP_PINGPONG && (afile->pb_fps < 0. || clip_can_reverse(pulsed->playing_file))) {
                  pulsed->in_arate = -pulsed->in_arate;
                  afile->adirection = -afile->adirection;
                  /// TODO - we should really read the first few bytes, however we dont yet support partial buffer reversals

                  pulsed->seek_pos = pulsed->seek_end;
                  pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos - qnt, qnt);
                  pulsed->real_seek_pos = pulsed->seek_pos;
                } else {
                  do {
                    if (mainw->playing_sel) {
                      pulsed->seek_pos = (int64_t)((double)(afile->start - 1.) / afile->fps * afile->arps)
                                         * afile->achans * (afile->asampsize / 8);
                      pulsed->real_seek_pos = pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
                    } else pulsed->seek_pos = 0;
                    if (pulsed->seek_pos == pulsed->seek_end) break;
                    lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
                    if (pulsed->aPlayPtr->size < in_bytes) {
                      pulsed->aPlayPtr->size += lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data)
                                                + pulsed->aPlayPtr->size, in_bytes - pulsed->aPlayPtr->size, TRUE);
                      pulsed->real_seek_pos = pulsed->seek_pos = lives_buffered_offset(pulsed->fd);
                    }
                  } while (pulsed->aPlayPtr->size < in_bytes && !lives_read_buffered_eof(pulsed->fd));
                  if (pulsed->aPlayPtr->size < in_bytes) {
                    pad_bytes = in_bytes - pulsed->aPlayPtr->size;
                    /// TODO: use pad_with_silence() for all padding
                    lives_memset((void *)pulsed->aPlayPtr->data + in_bytes - pad_bytes, 0, pad_bytes);
                    pulsed->aPlayPtr->size = in_bytes;
                  }
                }
              }
              pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos - qnt, qnt);
              fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;
              if (mainw->record && !mainw->record_paused) pulse_set_rec_avals(pulsed);
            }
          }

          else if (pulsed->playing_file != mainw->ascrap_file && shrink_factor  < 0.f) {
            /// reversed playback
            off_t seek_start = (mainw->playing_sel ?
                                (int64_t)((double)(afile->start - 1.) / afile->fps * afile->arps)
                                * afile->achans * (afile->asampsize / 8) : 0);
            seek_start = ALIGN_CEIL64(seek_start - qnt, qnt);
            if (pad_bytes > 0) pad_bytes = 0;
            else {
              if (pad_bytes < 0) {
                pad_bytes *= shrink_factor;
                pad_bytes = ALIGN_CEIL64(pad_bytes, qnt);
                /// pre-pad any silence (at end)
                lives_memset((void *)pulsed->aPlayPtr->data + in_bytes - pad_bytes, 0, pad_bytes);
              }
            }

            if ((pulsed->seek_pos -= (in_bytes - pad_bytes)) < seek_start) {
              /// hit the (lower) bound
              if (pulsed->loop == AUDIO_LOOP_NONE) {
                if (*pulsed->whentostop == STOP_ON_AUD_END) *pulsed->cancelled = CANCEL_AUD_END;
                in_bytes = 0;
              } else {
                /// read remaining bytes
                lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
                pulsed->seek_pos = ALIGN_CEIL64(seek_start, qnt);
                if (((mainw->agen_key == 0 && !mainw->agen_needs_reinit))) {
                  lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
                  pulsed->aPlayPtr->size = lives_read_buffered(pulsed->fd,
                                           (void *)(pulsed->aPlayPtr->data) + in_bytes - pad_bytes -
                                           (pulsed->real_seek_pos - pulsed->seek_pos),
                                           pulsed->real_seek_pos - pulsed->seek_pos, TRUE);
                  if (pulsed->aPlayPtr->size < pulsed->real_seek_pos - seek_start) {
                    /// short read, shift them up
                    lives_memmove((void *)pulsed->aPlayPtr->data + in_bytes - pad_bytes - pulsed->aPlayPtr->size,
                                  (void *)(pulsed->aPlayPtr->data) + in_bytes - pad_bytes -
                                  (pulsed->real_seek_pos - seek_start), pulsed->aPlayPtr->size);
                  }
                }

                pulsed->aPlayPtr->size += pad_bytes;

                /// bounce or loop round
                if (pulsed->loop == AUDIO_LOOP_PINGPONG) {
                  /// TODO - we should really read the first few bytes, however we dont yet support partial buffer reversals
                  pulsed->in_arate = -pulsed->in_arate;
                  afile->adirection = -afile->adirection;
                  pulsed->seek_pos = seek_start;
                } else {
                  pulsed->seek_pos = pulsed->seek_end - pulsed->aPlayPtr->size;
                }
              }
              pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos - qnt, qnt);
              fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;
              if (mainw->record && !mainw->record_paused) pulse_set_rec_avals(pulsed);
            }

            if (((mainw->agen_key == 0 && !mainw->agen_needs_reinit)) && in_bytes - pulsed->aPlayPtr->size > 0) {
              /// seek / read
              pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
              lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
              if (pulsed->playing_file == mainw->ascrap_file || pulsed->in_arate > 0) {
                lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
              } else {
                lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
                if (pulsed->aPlayPtr->size < in_bytes) {
                  pulsed->real_seek_pos = pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
                  lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
                  pulsed->aPlayPtr->size
                  += lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data)
                                         + pulsed->aPlayPtr->size,
                                         in_bytes - pulsed->aPlayPtr->size, TRUE);
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*

          if (pulsed->aPlayPtr->size < in_bytes) {
            /// if we are a few bytes short, pad with silence. If playing fwd we pad at the end, backwards we pad the beginning
            /// (since the buffer will be reversed)
            if (pulsed->in_arate > 0) {
              pad_with_silence(-1, (void *)pulsed->aPlayPtr->data, pulsed->aPlayPtr->size, in_bytes,
                               afile->asampsize >> 3, afile->signed_endian & AFORM_UNSIGNED,
                               afile->signed_endian & AFORM_BIG_ENDIAN);
            } else {
              lives_memmove((void *)pulsed->aPlayPtr->data + (in_bytes - pulsed->aPlayPtr->size), (void *)pulsed->aPlayPtr->data,
                            pulsed->aPlayPtr->size);
              pad_with_silence(-1, (void *)pulsed->aPlayPtr->data, 0, in_bytes - pulsed->aPlayPtr->size,
                               afile->asampsize >> 3, afile->signed_endian & AFORM_UNSIGNED,
                               afile->signed_endian & AFORM_BIG_ENDIAN);
            }
          }
        }

        /// put silence if anything changed
        if (pulsed->mute || in_bytes == 0 || pulsed->aPlayPtr->size == 0 || !IS_VALID_CLIP(pulsed->playing_file)
            || (!pulsed->aPlayPtr->data && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) ||
                                            mainw->multitrack || mainw->preview))) {
          sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
#ifdef DEBUG_PULSE
          g_print("pt a4\n");
#endif
          return;
        }

        if (mainw->agen_key != 0 && !mainw->multitrack && !mainw->preview) {
          in_bytes = pulseFramesAvailable * pulsed->out_achans * 2;
          if (xin_bytes == 0) xin_bytes = in_bytes;
        }

        if ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack || (mainw->preview &&
            !mainw->preview_rendering)) {
          /// read from file
          swap_sign = afile->signed_endian & AFORM_UNSIGNED;

          inputFramesAvailable = pulsed->aPlayPtr->size / (pulsed->in_achans * (pulsed->in_asamps >> 3));
#ifdef DEBUG_PULSE
          lives_printerr("%ld inputFramesAvailable == %ld, %ld, %d %d,pulseFramesAvailable == %lu\n", pulsed->aPlayPtr->size,
                         inputFramesAvailable,
                         in_frames, pulsed->in_arate, pulsed->out_arate, pulseFramesAvailable);
#endif

          //// buffer is just a ref to pulsed->aPlayPtr->data
          buffer = (uint8_t *)pulsed->aPlayPtr->data;
          ////
          numFramesToWrite = MIN(pulseFramesAvailable, (inputFramesAvailable / fabsf(shrink_factor) + .001)); // VALGRIND

#ifdef DEBUG_PULSE
          lives_printerr("inputFramesAvailable after conversion %ld\n", (uint64_t)((double)inputFramesAvailable
                         / shrink_factor + .001));
          lives_printerr("nsamples == %ld, pulseFramesAvailable == %ld,\n\tpulsed->num_input_channels == %ld, "
                         "pulsed->out_achans == %ld\n",
                         nsamples,
                         pulseFramesAvailable, pulsed->in_achans, pulsed->out_achans);
#endif

          // pulsed->sound_buffer will either point to pulsed->aPlayPtr->data or will hold transformed audio
          if (pulsed->in_asamps == pulsed->out_asamps && shrink_factor == 1. && pulsed->in_achans == pulsed->out_achans &&
              !pulsed->reverse_endian && !swap_sign) {
            // no transformation needed
            pulsed->sound_buffer = buffer;
          } else {
            if (pulsed->sound_buffer != pulsed->aPlayPtr->data) lives_freep((void **)&pulsed->sound_buffer);

            pulsed->sound_buffer = (uint8_t *)lives_calloc_safety(pulsed->chunk_size >> 2, 4);

            if (!pulsed->sound_buffer) {
              sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
#ifdef DEBUG_PULSE
              g_print("pt X2\n");
#endif
              return;
            }

            if (!from_memory) {
              /// remove random factor if doing so gives a power of 2
              if (in_bytes > in_framesd * pulsed->in_achans * (pulsed->in_asamps >> 3) && !((uint64_t)in_framesd & 1))
                in_bytes -= pulsed->in_achans * (pulsed->in_asamps >> 3);
            }

            /// convert 8 bit to 16 if applicable, and resample to out rate
            if (pulsed->in_asamps == 8) {
              sample_move_d8_d16((short *)(pulsed->sound_buffer), (uint8_t *)buffer, nsamples, in_bytes,
                                 shrink_factor, pulsed->out_achans, pulsed->in_achans, swap_sign ? SWAP_U_TO_S : 0);
            } else {
              sample_move_d16_d16((short *)pulsed->sound_buffer, (short *)buffer, nsamples, in_bytes, shrink_factor,
                                  pulsed->out_achans, pulsed->in_achans, pulsed->reverse_endian ? SWAP_X_TO_L : 0,
                                  swap_sign ? SWAP_U_TO_S : 0);
            }
          }

          if ((has_audio_filters(AF_TYPE_ANY) || mainw->ext_audio) && (pulsed->playing_file != mainw->ascrap_file)) {
            boolean memok = TRUE;
            float **fltbuf = (float **)lives_calloc(pulsed->out_achans, sizeof(float *));
            int i;
            /// we have audio filters... convert to float, pass through any audio filters, then back to s16
            for (i = 0; i < pulsed->out_achans; i++) {
              // convert s16 to non-interleaved float
              fltbuf[i] = (float *)lives_calloc_safety(nsamples, sizeof(float));
              if (!fltbuf[i]) {
                memok = FALSE;
                for (--i; i >= 0; i--) {
                  lives_freep((void **)&fltbuf[i]);
                }
                break;
              }
              /// convert to float, and take the opportunity to find the max volume
              /// (currently this is used to trigger recording start optionally)
              pulsed->abs_maxvol_heard = sample_move_d16_float(fltbuf[i], (short *)pulsed->sound_buffer + i,
                                         nsamples, pulsed->out_achans, FALSE, FALSE, 1.0);
            }

            if (memok) {
              ticks_t tc = mainw->currticks;
              // apply any audio effects with in_channels

              if (has_audio_filters(AF_TYPE_ANY)) {
                /** we create an Audio Layer and then call weed_apply_audio_effects_rt. The layer data is copied by ref
                  to the in channel of the filter and then from the out channel back to the layer.
                  IF the filter supports inplace then
                  we get the same buffers back, otherwise we will get newly allocated ones, we copy by ref back to our audio buf
                  and feed the result to the player as usual */
                weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                weed_layer_set_audio_data(layer, fltbuf, pulsed->out_arate, pulsed->out_achans, nsamples);
                weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                lives_free(fltbuf);
                fltbuf = weed_layer_get_audio_data(layer, NULL);
                weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                weed_layer_free(layer);
              }

              pthread_mutex_lock(&mainw->vpp_stream_mutex);
              if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
                (*mainw->vpp->render_audio_frame_float)(fltbuf, numFramesToWrite);
              }
              pthread_mutex_unlock(&mainw->vpp_stream_mutex);

              // convert float audio back to s16 in pulsed->sound_buffer
              sample_move_float_int(pulsed->sound_buffer, fltbuf, nsamples, 1.0, pulsed->out_achans, PA_SAMPSIZE, 0,
                                    (capable->hw.byte_order == LIVES_LITTLE_ENDIAN), FALSE, 1.0);

              for (i = 0; i < pulsed->out_achans; i++) {
                lives_free(fltbuf[i]);
              }
            }
            lives_free(fltbuf);
          }
        } else {
          // PULLING AUDIO FROM AN AUDIO GENERATOR
          // get float audio from gen, convert it to S16
          float **fltbuf = NULL;
          boolean pl_error = FALSE;
          xbytes = nbytes;
          numFramesToWrite = pulseFramesAvailable;

          if (mainw->agen_needs_reinit) pl_error = TRUE;
          else {
            fltbuf = (float **)lives_malloc(pulsed->out_achans * sizeof(float *));
            for (int i = 0; i < pulsed->out_achans; i++) fltbuf[i] =
                (float *)lives_calloc_safety(numFramesToWrite, sizeof(float));
            if (!get_audio_from_plugin(fltbuf, pulsed->out_achans, pulsed->out_arate, numFramesToWrite, TRUE)) {
              pl_error = TRUE;
            }
          }

          if (!pl_error) {
            if (LIVES_UNLIKELY(nbytes > pulsed->aPlayPtr->max_size)) {
              boolean update_sbuffer = FALSE;
              if (pulsed->sound_buffer == pulsed->aPlayPtr->data) update_sbuffer = TRUE;
              if (pulsed->aPlayPtr->data) lives_free((void *)(pulsed->aPlayPtr->data));
              pulsed->aPlayPtr->data = lives_calloc_safety(nbytes / 4 + 1, 4);
              //g_print("realloc 2\n");
              if (update_sbuffer) pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data);
              if (pulsed->aPlayPtr->data) {
                pulsed->aPlayPtr->max_size = nbytes;
              } else {
                pulsed->aPlayPtr->size = pulsed->aPlayPtr->max_size = 0;
                pl_error = TRUE;
              }
            }
            if (!pl_error) pulsed->aPlayPtr->size = nbytes;
          }

          // get back non-interleaved float fbuffer; rate and channels should match
          if (pl_error) nbytes = 0;
          else {
            boolean memok = FALSE;

            if (has_audio_filters(AF_TYPE_ANY) || mainw->ext_audio) {
              memok = TRUE;
              if (memok) {
                // apply any audio effects with in_channels
                ticks_t tc = mainw->currticks;
                if (has_audio_filters(AF_TYPE_ANY)) {
                  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                  weed_layer_set_audio_data(layer, fltbuf, pulsed->out_arate, pulsed->out_achans, numFramesToWrite);
                  weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                  lives_free(fltbuf);
                  fltbuf = weed_layer_get_audio_data(layer, NULL);
                  weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                  weed_layer_free(layer);
                }
              }
            }

            // streaming - we can push float audio to the playback plugin
            pthread_mutex_lock(&mainw->vpp_stream_mutex);
            if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
              (*mainw->vpp->render_audio_frame_float)(fltbuf, numFramesToWrite);
            }
            pthread_mutex_unlock(&mainw->vpp_stream_mutex);

            // copy effected audio back into pulsed->aPlayPtr->data
            pulsed->sound_buffer = (uint8_t *)pulsed->aPlayPtr->data;

            sample_move_float_int(pulsed->sound_buffer, fltbuf, numFramesToWrite, 1.0,
                                  pulsed->out_achans, PA_SAMPSIZE, 0, (capable->hw.byte_order == LIVES_LITTLE_ENDIAN), FALSE, 1.0);

            if (fltbuf) {
              for (register int i = 0; i < pulsed->out_achans; i++) lives_freep((void **)&fltbuf[i]);
            }

            lives_free(fltbuf);
            /// pl_error
          }

          if (mainw->record && !mainw->record_paused && mainw->ascrap_file != -1 && mainw->playing_file > 0) {
            /// if we are recording then write generated audio to ascrap_file
            /// we may need to resample again to the file rate.
            /// TODO: use markers to indicate when the rate changes, eliminating the necessity
            size_t rbytes = numFramesToWrite * mainw->files[mainw->ascrap_file]->achans *
                            mainw->files[mainw->ascrap_file]->asampsize >> 3;
            pulse_flush_read_data(pulsed, mainw->ascrap_file,
                                  nbytes, mainw->files[mainw->ascrap_file]->signed_endian & AFORM_BIG_ENDIAN,
                                  pulsed->sound_buffer);
            mainw->files[mainw->ascrap_file]->aseek_pos += rbytes;
          }
          /* // end from gen */
        }

        pulseFramesAvailable -= numFramesToWrite;

#ifdef DEBUG_PULSE
        lives_printerr("pulseFramesAvailable == %ld\n", pulseFramesAvailable);
#endif
      }

      // playback from memory or file
      if (pulsed->playing_file > -1 && !mainw->multitrack) clip_vol = lives_vol_from_linear(afile->vol);
      if (future_prefs->volume * clip_vol != pulsed->volume_linear) {
        // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +) ??
        pa_operation *paop;
        pavol = pa_sw_volume_from_linear(future_prefs->volume * clip_vol);
        pa_cvolume_set(&pulsed->volume, pulsed->out_achans, pavol);
        paop = pa_context_set_sink_input_volume(pulsed->con,
                                                pa_stream_get_index(pulsed->pstream), &pulsed->volume, NULL, NULL);
        pa_operation_unref(paop);
        pulsed->volume_linear = future_prefs->volume * clip_vol;
      }
    }

    // buffer is reused here, it's what we'll actually push to pulse

    if (sync_ready) sync_ready_ok(pulsed, nbytes);

    while (nbytes > 0) {
      if (nbytes < xbytes) xbytes = nbytes;

      if (!from_memory) {
#if !HAVE_PA_STREAM_BEGIN_WRITE
        if (xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3) <= numFramesToWrite && offs == 0) {
          buffer = pulsed->sound_buffer;
#if 0
        }
      }
#endif
#else
        if (0) {
          // do nothing
        }
#endif
      else {
        int ret = 0;
        if (pulsed->sound_buffer) {
#if HAVE_PA_STREAM_BEGIN_WRITE
          xbytes = -1;
          // returns a buffer and a max size fo us to write to
          ret = pa_stream_begin_write(pulsed->pstream, (void **)&buffer, &xbytes);
          if (nbytes < xbytes) xbytes = nbytes;
#else
          buffer = (uint8_t *)lives_calloc(nbytes, 1);
#endif
        }
        if (!pulsed->sound_buffer || ret != 0 || !buffer) {
          sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), nbytes);
#ifdef DEBUG_PULSE
          g_print("pt X3\n");
#endif
          return;
        }
        lives_memcpy(buffer, pulsed->sound_buffer + offs, xbytes);
        offs += xbytes;
        needs_free = TRUE;
      }

      /// we may also stream to a fifo, as well as possibly caching the audio for any video filters to utilize
      if (pulsed->astream_fd != -1) audio_stream(buffer, xbytes, pulsed->astream_fd);
      if (mainw->afbuffer && prefs->audio_src != AUDIO_SRC_EXT) {
        append_to_audio_buffer16(buffer, xbytes / 2, 2);
      }

      /// Finally... we actually write to pulse buffers
#if !HAVE_PA_STREAM_BEGIN_WRITE
      pa_stream_write(pulsed->pstream, buffer, xbytes, buffer == pulsed->aPlayPtr->data ? NULL :
                      pulse_buff_free, 0, PA_SEEK_RELATIVE);
#else
      pa_stream_write(pulsed->pstream, buffer, xbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
      if (!sync_ready)
        pulsed->extrausec += ((double)xbytes / (double)(pulsed->out_arate) * 1000000.
                              / (double)(pulsed->out_achans * pulsed->out_asamps >> 3) + .5);
      pulsed->frames_written += xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
    } else {
      // from memory (e,g multitrack)
      if (pulsed->read_abuf > -1 && !pulsed->mute) {
        int ret = 0;
#if HAVE_PA_STREAM_BEGIN_WRITE
        xbytes = -1;
        ret = pa_stream_begin_write(pulsed->pstream, (void **)&shortbuffer, &xbytes);
#endif
        if (nbytes < xbytes) xbytes = nbytes;
#if !HAVE_PA_STREAM_BEGIN_WRITE
        shortbuffer = (short *)lives_calloc(xbytes);
#endif
        if (!shortbuffer || ret != 0) {
          sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), nbytes);
#ifdef DEBUG_PULSE
          g_print("pt X4\n");
#endif
          return;
        }
        sample_move_abuf_int16(shortbuffer, pulsed->out_achans, (xbytes >> 1) / pulsed->out_achans, pulsed->out_arate);
        if (pulsed->astream_fd != -1) audio_stream(shortbuffer, xbytes, pulsed->astream_fd);
#if !HAVE_PA_STREAM_BEGIN_WRITE
        pa_stream_write(pulsed->pstream, shortbuffer, xbytes, pulse_buff_free, 0, PA_SEEK_RELATIVE);
#else
        pa_stream_write(pulsed->pstream, shortbuffer, xbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
        if (!sync_ready)
          pulsed->extrausec += ((double)xbytes / (double)(pulsed->out_arate) * 1000000.
                                / (double)(pulsed->out_achans * pulsed->out_asamps >> 3) + .5);
        pulsed->frames_written += xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
      } else {
        sample_silence_pulse(pulsed, xbytes, xbytes);
      }
    }
    nbytes -= xbytes;
  }

  if (needs_free && pulsed->sound_buffer != pulsed->aPlayPtr->data && pulsed->sound_buffer) {
    lives_freep((void **)&pulsed->sound_buffer);
  }

  fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;

  if (pulseFramesAvailable) {
#ifdef DEBUG_PULSE
    lives_printerr("buffer underrun of %ld frames\n", pulseFramesAvailable);
#endif
    xbytes = pa_stream_writable_size(pstream);
    sample_silence_pulse(pulsed, pulseFramesAvailable * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
    if (!pulsed->is_paused) pulsed->frames_written += xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
  }
} else {
#ifdef DEBUG_PULSE
  if (pulsed->state == PA_STREAM_UNCONNECTED || pulsed->state == PA_STREAM_CREATING)
    LIVES_INFO("pulseaudio stream UNCONNECTED or CREATING");
  else
    LIVES_WARN("pulseaudio stream FAILED or TERMINATED");
#endif
}
#ifdef DEBUG_PULSE
lives_printerr("done\n");
#endif
}


size_t pulse_flush_read_data(pulse_driver_t *pulsed, int fileno, size_t rbytes, boolean rev_endian, void *data) {
  // prb is how many bytes to write, with rbytes as the latest addition

  short *gbuf;
  size_t bytes_out, frames_out, bytes = 0;
  void *holding_buff;

  float out_scale;
  int swap_sign;

  lives_clip_t *ofile;

  if (!data) data = prbuf;

  if (mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
    if (prb == 0 || mainw->rec_samples == 0) return 0;
    if (prb <= PULSE_READ_BYTES * 2) {
      gbuf = (short *)data;
    } else {
      gbuf = (short *)lives_malloc(prb);
      if (!gbuf) return 0;
      if (prb > rbytes) lives_memcpy((void *)gbuf, prbuf, prb - rbytes);
      lives_memcpy((void *)gbuf + (prb - rbytes > 0 ? prb - rbytes : 0), data, rbytes);
    }
    ofile = afile;
  } else {
    if (rbytes == 0) return 0;
    if (fileno == -1) return 0;
    gbuf = (short *)data;
    prb = rbytes;
    ofile = mainw->files[fileno];
  }

  if (mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
    out_scale = (float)pulsed->in_arate / (float)ofile->arate;
  } else out_scale = 1.;

  swap_sign = ofile->signed_endian & AFORM_UNSIGNED;

  frames_out = (size_t)((double)((prb / (ofile->asampsize >> 3) / ofile->achans)) / out_scale);

  if (mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
    if (frames_out != pulsed->chunk_size) pulsed->chunk_size = frames_out;
  }

  bytes_out = frames_out * ofile->achans * (ofile->asampsize >> 3);

  holding_buff = lives_malloc(bytes_out);

  if (!holding_buff) {
    if (gbuf != (short *)data) lives_free(gbuf);
    prb = 0;
    return 0;
  }

  if (ofile->asampsize == 16) {
    sample_move_d16_d16((short *)holding_buff, gbuf, frames_out, prb, out_scale, ofile->achans, pulsed->in_achans,
                        pulsed->reverse_endian ? SWAP_L_TO_X : 0, swap_sign ? SWAP_S_TO_U : 0);
  } else {
    sample_move_d16_d8((uint8_t *)holding_buff, gbuf, frames_out, prb, out_scale, ofile->achans, pulsed->in_achans,
                       swap_sign ? SWAP_S_TO_U : 0);
  }

  if (gbuf != (short *)data) lives_free(gbuf);

  prb = 0;

  if (mainw->rec_samples > 0) {
    if (frames_out > mainw->rec_samples) frames_out = mainw->rec_samples;
    mainw->rec_samples -= frames_out;
  }

  if (!THREADVAR(bad_aud_file)) {
    size_t target = frames_out * (ofile->asampsize / 8) * ofile->achans;
    ssize_t bytes;
    bytes = lives_write_buffered(mainw->aud_rec_fd, holding_buff, target, TRUE);
    if (bytes > 0) {
      uint64_t chk = (mainw->aud_data_written & AUD_WRITE_CHECK);
      mainw->aud_data_written += bytes;
      if (IS_VALID_CLIP(mainw->ascrap_file) && mainw->aud_rec_fd == mainw->files[mainw->ascrap_file]->cb_src)
        add_to_ascrap_mb(bytes);
      check_for_disk_space((mainw->aud_data_written & AUD_WRITE_CHECK) != chk);
    }
    if (bytes < target) THREADVAR(bad_aud_file) = filename_from_fd(NULL, mainw->aud_rec_fd);
  }

  lives_free(holding_buff);

  return bytes;
}


static void pulse_audio_read_process(pa_stream * pstream, size_t nbytes, void *arg) {
  // read nsamples from pulse buffer, and then possibly write to mainw->aud_rec_fd

  // this is the callback from pulse when we are recording or playing external audio

  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  float out_scale;
  size_t frames_out, nsamples;
  void *data;
  size_t rbytes = nbytes, zbytes;
  int nch = pulsed->in_achans;

  pulsed->pstream = pstream;

  if (pulsed->is_corked) return;

  if (!pulsed->in_use || (mainw->playing_file < 0 && prefs->audio_src == AUDIO_SRC_EXT) || mainw->effects_paused) {
    pa_stream_peek(pulsed->pstream, (const void **)&data, &rbytes);
    if (rbytes > 0) {
      //g_print("PVAL %d\n", (*(uint8_t *)data & 0x80) >> 7);
      pa_stream_drop(pulsed->pstream);
    }
    prb = 0;
    if (pulsed->in_use)
      pulsed->extrausec += ((double)nbytes / (double)(pulsed->out_arate) * 1000000.
                            / (double)(pulsed->out_achans * pulsed->out_asamps >> 3) + .5);
    return;
  }

  zbytes = pa_stream_readable_size(pulsed->pstream);

  if (zbytes == 0) {
    //g_print("nothing to read from PA\n");
    return;
  }

  if (pa_stream_peek(pulsed->pstream, (const void **)&data, &rbytes)) {
    return;
  }

  if (!data) {
    if (rbytes > 0) {
      pa_stream_drop(pulsed->pstream);
    }
    return;
  }

  if (!mainw->fs && !mainw->faded && !mainw->multitrack && mainw->ext_audio_mon)
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_mon), (*(uint8_t *)data & 0x80) >> 7);

  // time interpolation
  pulsed->extrausec += ((double)rbytes / (double)(pulsed->out_arate) * 1000000.
                        / (double)(pulsed->out_achans * pulsed->out_asamps >> 3) + .5);

  pthread_mutex_lock(&mainw->audio_filewriteend_mutex);

  if (pulsed->playing_file == -1) {
    out_scale = 1.0; // just listening, no recording
  } else {
    out_scale = (float)afile->arate / (float)pulsed->in_arate; // recording to ascrap_file
  }

  if (mainw->record && mainw->record_paused && prb > 0) {
    // flush audio when recording is paused
    if (prb <= PULSE_READ_BYTES * 2) {
      lives_memcpy(&prbuf[prb - rbytes], data, rbytes);
      pulse_flush_read_data(pulsed, pulsed->playing_file, prb, pulsed->reverse_endian, prbuf);
    } else {
      pulse_flush_read_data(pulsed, pulsed->playing_file, rbytes, pulsed->reverse_endian, data);
    }
  }

  if (pulsed->playing_file == -1 || (mainw->record && mainw->record_paused)) prb = 0;
  else prb += rbytes;

  frames_out = (size_t)((double)((prb / (pulsed->in_asamps >> 3) / nch)) / out_scale + .5);

  nsamples = (size_t)((double)((rbytes / (pulsed->in_asamps >> 3) / nch)) / out_scale + .5);

  // should really be frames_read here
  if (!pulsed->is_paused) {
    pulsed->frames_written += nsamples;
  }

  if (prefs->audio_src == AUDIO_SRC_EXT && (pulsed->playing_file == -1 || pulsed->playing_file == mainw->ascrap_file)) {
    // - (do not call this when recording ext window or voiceover)

    // in this case we read external audio, but maybe not record it
    // we may wish to analyse the audio for example, or push it to a video generator
    // or stream it to the video playback plugin

    if ((!mainw->video_seek_ready && prefs->ahold_threshold > pulsed->abs_maxvol_heard)
        || has_audio_filters(AF_TYPE_A) || mainw->ext_audio) {
      // convert to float, apply any analysers
      boolean memok = TRUE;
      float **fltbuf = (float **)lives_calloc(nch, sizeof(float *));
      register int i;

      size_t xnsamples = (size_t)(rbytes / (pulsed->in_asamps >> 3) / nch);

      if (!fltbuf) {
        pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
        pa_stream_drop(pulsed->pstream);
        return;
      }

      for (i = 0; i < nch; i++) {
        // convert s16 to non-interleaved float
        fltbuf[i] = (float *)lives_calloc(xnsamples, sizeof(float));
        if (!fltbuf[i]) {
          memok = FALSE;
          for (--i; i >= 0; i--) lives_free(fltbuf[i]);
          break;
        }

        pulsed->abs_maxvol_heard
          = sample_move_d16_float(fltbuf[i], (short *)(data) + i, xnsamples, nch, FALSE, FALSE, 1.0);

        if (mainw->afbuffer && prefs->audio_src == AUDIO_SRC_EXT) {
          // if we have audio triggered gens., push audio to it
          append_to_audio_bufferf(fltbuf[i], xnsamples, i == nch - 1 ? -i - 1 : i + 1);
        }
      }

      if (memok) {
        ticks_t tc = mainw->currticks;
        // apply any audio effects with in channels but no out channels (analysers)

        if (has_audio_filters(AF_TYPE_A)) {
          weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
          weed_layer_set_audio_data(layer, fltbuf, pulsed->in_arate, nch, xnsamples);
          weed_apply_audio_effects_rt(layer, tc, TRUE, TRUE);
          lives_free(fltbuf);
          fltbuf = weed_layer_get_audio_data(layer, NULL);
          weed_layer_free(layer);
        }
        // stream audio to video playback plugin if appropriate (probably needs retesting...)
        pthread_mutex_lock(&mainw->vpp_stream_mutex);
        if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
          (*mainw->vpp->render_audio_frame_float)(fltbuf, xnsamples);
        }
        pthread_mutex_unlock(&mainw->vpp_stream_mutex);
        for (i = 0; i < nch; i++) {
          lives_free(fltbuf[i]);
        }
      }

      lives_freep((void **)&fltbuf);
    }
  }

  if (pulsed->playing_file == -1 || (mainw->record && mainw->record_paused) || pulsed->is_paused) {
    pa_stream_drop(pulsed->pstream);
    if (pulsed->is_paused) {
      // This is NECESSARY to reduce / eliminate huge latencies.

      // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +)
      pa_operation *paop = pa_stream_flush(pulsed->pstream, NULL,
                                           NULL); // if not recording, flush the rest of audio (to reduce latency)
      pa_operation_unref(paop);
    }
    pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
    return;
  }

  if (mainw->playing_file != mainw->ascrap_file && IS_VALID_CLIP(mainw->playing_file))
    mainw->files[mainw->playing_file]->aseek_pos += rbytes;
  if (mainw->ascrap_file != -1 && !mainw->record_paused) mainw->files[mainw->ascrap_file]->aseek_pos += rbytes;

  pulsed->seek_pos += rbytes;

  if (prb < PULSE_READ_BYTES && (mainw->rec_samples == -1 || frames_out < mainw->rec_samples)) {
    // buffer until we have enough
    lives_memcpy(&prbuf[prb - rbytes], data, rbytes);
  } else {
    if (prb <= PULSE_READ_BYTES * 2) {
      lives_memcpy(&prbuf[prb - rbytes], data, rbytes);
      pulse_flush_read_data(pulsed, pulsed->playing_file, prb, pulsed->reverse_endian, prbuf);
    } else {
      pulse_flush_read_data(pulsed, pulsed->playing_file, rbytes, pulsed->reverse_endian, data);
    }
  }

  pa_stream_drop(pulsed->pstream);
  pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);

  if (mainw->rec_samples == 0 && mainw->cancelled == CANCEL_NONE) {
    mainw->cancelled = CANCEL_KEEP; // we wrote the required #
  }
}


void pulse_shutdown(void) {
  //g_print("pa shutdown\n");
  if (pcon) {
    //g_print("pa shutdown2\n");
    pa_context_disconnect(pcon);
    pa_context_unref(pcon);
  }
  if (pa_mloop) {
    pa_threaded_mainloop_stop(pa_mloop);
    pa_threaded_mainloop_free(pa_mloop);
  }
  pcon = NULL;
  pa_mloop = NULL;
}


void pulse_close_client(pulse_driver_t *pdriver) {
  if (pdriver->pstream) {
    pa_mloop_lock();
    pa_stream_disconnect(pdriver->pstream);
    pa_stream_set_write_callback(pdriver->pstream, NULL, NULL);
    pa_stream_set_read_callback(pdriver->pstream, NULL, NULL);
    pa_stream_set_underflow_callback(pdriver->pstream, NULL, NULL);
    pa_stream_set_overflow_callback(pdriver->pstream, NULL, NULL);
    pa_stream_unref(pdriver->pstream);
    pa_mloop_unlock();
  }
  if (pdriver->pa_props) pa_proplist_free(pdriver->pa_props);
  pdriver->pa_props = NULL;
  pdriver->pstream = NULL;
}


int pulse_audio_init(void) {
  // initialise variables
  pulsed.in_use = FALSE;
  pulsed.mloop = pa_mloop;
  pulsed.con = pcon;

  //for (int j = 0; j < PULSE_MAX_OUTPUT_CHANS; j++) pulsed.volume.values[j] = pa_sw_volume_from_linear(future_prefs->volume);
  pulsed.volume_linear = future_prefs->volume;
  pulsed.state = (pa_stream_state_t)PA_STREAM_UNCONNECTED;
  pulsed.in_arate = 44100;
  pulsed.fd = -1;
  pulsed.seek_pos = pulsed.seek_end = pulsed.real_seek_pos = 0;
  pulsed.msgq = NULL;
  pulsed.num_calls = 0;
  pulsed.chunk_size = 0;
  pulsed.astream_fd = -1;
  pulsed.abs_maxvol_heard = 0.;
  pulsed.pulsed_died = FALSE;
  pulsed.aPlayPtr = (audio_buffer_t *)lives_malloc(sizeof(audio_buffer_t));
  pulsed.aPlayPtr->data = NULL;
  pulsed.aPlayPtr->size = 0;
  pulsed.aPlayPtr->max_size = 0;
  pulsed.in_achans = PA_ACHANS;
  pulsed.out_achans = PA_ACHANS;
  pulsed.out_asamps = PA_SAMPSIZE;
  pulsed.mute = FALSE;
  pulsed.out_chans_available = PULSE_MAX_OUTPUT_CHANS;
  pulsed.is_output = TRUE;
  pulsed.read_abuf = -1;
  pulsed.is_paused = FALSE;
  //pulsed.pstream = NULL;
  pulsed.pa_props = NULL;
  pulsed.playing_file = -1;
  pulsed.sound_buffer = NULL;
  pulsed.extrausec = 0;
  return 0;
}


int pulse_audio_read_init(void) {
  // initialise variables
#if PA_SW_CONNECTION
  int j;
#endif

  pulsed_reader.in_use = FALSE;
  pulsed_reader.mloop = pa_mloop;
  pulsed_reader.con = pcon;

  //for (j = 0; j < PULSE_MAX_OUTPUT_CHANS; j++) pulsed_reader.volume.values[j] = pa_sw_volume_from_linear(future_prefs->volume);
  pulsed_reader.state = (pa_stream_state_t)PA_STREAM_UNCONNECTED;
  pulsed_reader.fd = -1;
  pulsed_reader.seek_pos = pulsed_reader.seek_end = 0;
  pulsed_reader.msgq = NULL;
  pulsed_reader.num_calls = 0;
  pulsed_reader.chunk_size = 0;
  pulsed_reader.astream_fd = -1;
  pulsed_reader.abs_maxvol_heard = 0.;
  pulsed_reader.pulsed_died = FALSE;
  pulsed_reader.in_achans = PA_ACHANS;
  pulsed_reader.in_asamps = PA_SAMPSIZE;
  pulsed_reader.mute = FALSE;
  pulsed_reader.is_output = FALSE;
  pulsed_reader.is_paused = FALSE;
  pulsed_reader.pstream = NULL;
  pulsed_reader.pa_props = NULL;
  pulsed_reader.sound_buffer = NULL;
  pulsed_reader.extrausec = 0;
  return 0;
}


#if PA_SW_CONNECTION
static void info_cb(pa_context * c, const pa_sink_input_info * i, int eol, void *userdata) {
  // would be great if this worked, but apparently it always returns NULL in i
  // for a hardware connection

  // TODO: get volume_writeable (pa 1.0+)
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
  if (!i) return;

  pdrive->volume = i->volume;
  pdriver->volume_linear = pa_sw_volume_to_linear(i->volume.values[0]);
  pref_factory_float(PREF_MASTER_VOLUME, pdriver->volume_linear, TRUE);
  if (i->mute != mainw->mute) on_mute_activate(NULL, NULL);
}
#endif


int pulse_driver_activate(pulse_driver_t *pdriver) {
  // create a new client and connect it to pulse server
  char *pa_clientname;
  char *mypid;

  pa_sample_spec pa_spec;
  pa_channel_map pa_map;
  pa_buffer_attr pa_battr;

  pa_operation *pa_op;

  if (pdriver->pstream) return 0;

  if (mainw->aplayer_broken) return 2;

  if (pdriver->is_output) {
    pa_clientname = "LiVES_audio_out";
  } else {
    pa_clientname = "LiVES_audio_in";
  }

  mypid = lives_strdup_printf("%d", capable->mainpid);

  pdriver->pa_props = pa_proplist_new();

  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_ICON_NAME, lives_get_application_name());
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_ID, lives_get_application_name());
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_NAME, lives_get_application_name());

  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_PROCESS_BINARY, capable->myname);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_PROCESS_ID, mypid);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_VERSION, LiVES_VERSION);

  lives_free(mypid);

#ifdef GUI_GTK
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_LANGUAGE, pango_language_to_string(gtk_get_default_language()));
#endif

#ifdef GUI_QT
  QLocale ql;
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_LANGUAGE,
                   (QLocale::languageToString(ql.language())).toLocal8Bit().constData());
#endif

  pa_channel_map_init_stereo(&pa_map);

  pa_spec.format = PA_SAMPLE_S16NE;

  pa_spec.channels = pdriver->out_achans = pdriver->in_achans;

  pdriver->in_asamps = pdriver->out_asamps = PA_SAMPSIZE;
  pdriver->out_signed = AFORM_SIGNED;

  if (capable->hw.byte_order == LIVES_BIG_ENDIAN) {
    pdriver->out_endian = AFORM_BIG_ENDIAN;
    pa_spec.format = PA_SAMPLE_S16BE;
  } else {
    pdriver->out_endian = AFORM_LITTLE_ENDIAN;
    pa_spec.format = PA_SAMPLE_S16LE;
  }

  if (pdriver->is_output) {
    pa_battr.maxlength = LIVES_PA_BUFF_MAXLEN;
    pa_battr.tlength = LIVES_PA_BUFF_TARGET;
    pa_battr.minreq = LIVES_PA_BUFF_MINREQ * 2;

    /// TODO: kick off a thread to call the audio loop peridically (to receive command messages), on audio_seek == FALSE,
    // seek and fill the prefbuffer, then kill the thread loop and let pa take over. Must do the same on underflow though
    pa_battr.prebuf = 0;  /// must set this to zero else we hang, since pa is waiting for the buffer to be filled first
  } else {
    pa_battr.maxlength = LIVES_PA_BUFF_MAXLEN * 2;
    pa_battr.fragsize = LIVES_PA_BUFF_FRAGSIZE * 4;
    pa_battr.minreq = (uint32_t) - 1;
    pa_battr.prebuf = -1;
  }

  pa_mloop_lock();
  if (pulse_server_rate == 0) {
    pa_op = pa_context_get_server_info(pdriver->con, pulse_server_cb, pa_mloop);
    while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
      pa_threaded_mainloop_wait(pa_mloop);
    }
    pa_operation_unref(pa_op);
  }

  if (pulse_server_rate == 0) {
    pa_mloop_unlock();
    LIVES_WARN("Problem getting pulseaudio rate...expect more problems ahead.");
    return 1;
  }

  pa_spec.rate = pdriver->out_arate = pdriver->in_arate = pulse_server_rate;

  pdriver->pstream = pa_stream_new_with_proplist(pdriver->con, pa_clientname, &pa_spec, &pa_map, pdriver->pa_props);

  /// TODO: try to set volume and mute state from sever rather then the other way round

  if (pdriver->is_output) {
    pa_volume_t pavol;
    pdriver->is_corked = TRUE;

    // set write callback
    pa_stream_set_write_callback(pdriver->pstream, pulse_audio_write_process, pdriver);
    pa_stream_set_underflow_callback(pdriver->pstream, stream_underflow_callback, pdriver);
    pa_stream_set_overflow_callback(pdriver->pstream, stream_overflow_callback, pdriver);
    pa_stream_set_moved_callback(pdriver->pstream, stream_moved_callback, pdriver);
    pa_stream_set_buffer_attr_callback(pdriver->pstream, stream_buffer_attr_callback, pdriver);

#if PA_SW_CONNECTION
    pa_stream_connect_playback(pdriver->pstream, NULL, &pa_battr, (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY |
                               PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_START_CORKED |
                               PA_STREAM_AUTO_TIMING_UPDATE), NULL, NULL);
#else
    pdriver->volume_linear = future_prefs->volume;
    pavol = pa_sw_volume_from_linear(pdriver->volume_linear);
    pa_cvolume_set(&pdriver->volume, pdriver->out_achans, pavol);

    // calling this may cause other streams to be interrupted temporarily
    // it seems impossible to avoid this
    pa_stream_connect_playback(pdriver->pstream, NULL, &pa_battr, (pa_stream_flags_t)(0
                               | PA_STREAM_RELATIVE_VOLUME
                               | PA_STREAM_INTERPOLATE_TIMING
                               | PA_STREAM_START_CORKED
                               | PA_STREAM_START_UNMUTED
                               | PA_STREAM_NOT_MONOTONIC
                               | PA_STREAM_AUTO_TIMING_UPDATE),
                               &pdriver->volume, NULL);
#endif
    pa_mloop_unlock();

    while (pa_stream_get_state(pdriver->pstream) != PA_STREAM_READY) {
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }

    pdriver->volume_linear = -1;

#if PA_SW_CONNECTION
    // get the volume from the server
    pa_op = pa_context_get_sink_info(pdriver->con, info_cb, &pdriver);

    while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }
    pa_operation_unref(pa_op);
#endif
  } else {
    // set read callback
    pdriver->frames_written = 0;
    pdriver->usec_start = 0;
    pdriver->in_use = FALSE;
    pdriver->abs_maxvol_heard = 0.;
    pdriver->is_corked = TRUE;
    prb = 0;

    pa_stream_set_underflow_callback(pdriver->pstream, stream_underflow_callback, pdriver);
    pa_stream_set_overflow_callback(pdriver->pstream, stream_overflow_callback, pdriver);

    pa_stream_set_moved_callback(pdriver->pstream, stream_moved_callback, pdriver);
    pa_stream_set_buffer_attr_callback(pdriver->pstream, stream_buffer_attr_callback, pdriver);
    pa_stream_set_read_callback(pdriver->pstream, pulse_audio_read_process, pdriver);

    pa_stream_connect_record(pdriver->pstream, NULL, &pa_battr,
                             (pa_stream_flags_t)(PA_STREAM_START_CORKED
                                 | PA_STREAM_ADJUST_LATENCY
                                 | PA_STREAM_INTERPOLATE_TIMING
                                 | PA_STREAM_AUTO_TIMING_UPDATE
                                 | PA_STREAM_NOT_MONOTONIC));

    pa_mloop_unlock();
    while (pa_stream_get_state(pdriver->pstream) != PA_STREAM_READY) {
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }
  }

  return 0;
}


//#define DEBUG_PULSE_CORK
static void uncorked_cb(pa_stream * s, int success, void *userdata) {
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
#ifdef DEBUG_PULSE_CORK
  g_print("uncorked %p\n", pdriver);
#endif
  pdriver->is_corked = FALSE;
  prefs->force_system_clock = FALSE;
}


static void corked_cb(pa_stream * s, int success, void *userdata) {
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
#ifdef DEBUG_PULSE_CORK
  g_print("corked %p\n", pdriver);
#endif
  pdriver->is_corked = TRUE;
  prefs->force_system_clock = TRUE;
  pa_threaded_mainloop_signal(pa_mloop, 0);
}


void pulse_driver_uncork(pulse_driver_t *pdriver) {
  pa_operation *paop;
  pdriver->abs_maxvol_heard = 0.;
  if (!pdriver->is_corked) return;
  pa_mloop_lock();
  paop = pa_stream_cork(pdriver->pstream, 0, uncorked_cb, pdriver);
  pa_mloop_unlock();
  if (pdriver->is_output) {
    pa_operation_unref(paop);
    return; // let it uncork in its own time...
  }
  pa_operation_unref(paop);
}


void pulse_driver_cork(pulse_driver_t *pdriver) {
  pa_operation *paop;
  ticks_t timeout;
  lives_alarm_t alarm_handle;
  if (pdriver->is_corked) {
    //g_print("IS CORKED\n");
    return;
  }

  do {
    alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
    pa_mloop_lock();
    paop = pa_stream_cork(pdriver->pstream, 1, corked_cb, pdriver);
    while (pa_operation_get_state(paop) == PA_OPERATION_RUNNING && (timeout = lives_alarm_check(alarm_handle)) > 0) {
      pa_threaded_mainloop_wait(pa_mloop);
    }
    pa_operation_unref(paop);
    pa_mloop_unlock();
    lives_alarm_clear(alarm_handle);
  } while (!timeout);

  pa_mloop_lock();
  paop = pa_stream_flush(pdriver->pstream, NULL, NULL);
  pa_operation_unref(paop);
  pa_mloop_unlock();
}


///////////////////////////////////////////////////////////////

LIVES_GLOBAL_INLINE pulse_driver_t *pulse_get_driver(boolean is_output) {
  if (is_output) return &pulsed;
  return &pulsed_reader;
}


LIVES_GLOBAL_INLINE volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *pulsed) {
  if (pulsed->pulsed_died || mainw->aplayer_broken) return NULL;
  return pulsed->msgq;
}


boolean pa_time_reset(pulse_driver_t *pulsed, ticks_t offset) {
  int64_t usec;
  pa_operation *pa_op;

  if (!pulsed->pstream) return FALSE;

  pa_mloop_lock();
  pa_op = pa_stream_update_timing_info(pulsed->pstream, pulse_success_cb, pa_mloop);

  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(pa_mloop);
  }
  pa_operation_unref(pa_op);
  pa_mloop_unlock();

  while (pa_stream_get_time(pulsed->pstream, (pa_usec_t *)&usec) < 0) {
    lives_nanosleep(10000);
  }
  pulsed->usec_start = usec - offset  / USEC_TO_TICKS;
  pulsed->frames_written = 0;
  return TRUE;
}


/**
   @brief calculate the playback time based on samples sent to the soundcard
*/
ticks_t lives_pulse_get_time(pulse_driver_t *pulsed) {
  // get the time in ticks since either playback started
  volatile aserver_message_t *msg;
  int64_t usec;
  ticks_t timeout, retval;
  lives_alarm_t alarm_handle;

  msg = pulsed->msgq;
  if (msg && (msg->command == ASERVER_CMD_FILE_SEEK || msg->command == ASERVER_CMD_FILE_OPEN)) {
    alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
    while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(pulsed)) {
      lives_nanosleep(1000);
    }
    lives_alarm_clear(alarm_handle);
    if (timeout == 0) return -1;
  }
  /* if (!CLIP_HAS_AUDIO(pulsed->playing_file) && !(LIVES_IS_PLAYING && pulsed->read_abuf > -1)) { */
  /*   return -1; */
  /* } */
  while (pa_stream_get_time(pulsed->pstream, (pa_usec_t *)&usec) < 0) {
    lives_nanosleep(10000);
  }
  retval = (ticks_t)((usec - pulsed->usec_start) * USEC_TO_TICKS);
  if (retval == -1) retval = 0;
  return retval;
}


double lives_pulse_get_pos(pulse_driver_t *pulsed) {
  // get current time position (seconds) in audio file
  //return (double)pulsed->real_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
  if (pulsed->playing_file > -1) {
    return (double)(fwd_seek_pos)
           / (double)(afile->arps * afile->achans * afile->asampsize / 8);
  }
  // from memory
  return (double)pulsed->frames_written / (double)pulsed->out_arate;
}


boolean pulse_audio_seek_frame(pulse_driver_t *pulsed, double frame) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample
  int64_t seekstart;
  double fps;

  if (frame > afile->frames && afile->frames > 0) frame = afile->frames;
  if (LIVES_IS_PLAYING) fps = fabs(afile->pb_fps);
  else fps = afile->fps;

  seekstart = (int64_t)(((frame - 1.) / fps
                         + (LIVES_IS_PLAYING ? (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL
                            * (pulsed->in_arate >= 0. ? 1.0 : -1.0) : 0.)) * (double)afile->arate)
              * afile->achans * afile->asampsize / 8;

  /* g_print("vals %ld and %ld %d\n", mainw->currticks, mainw->startticks, afile->arate); */
  /* g_print("bytes %f     %f       %d        %ld          %f\n", frame, afile->fps, LIVES_IS_PLAYING, seekstart, */
  /*         (double)seekstart / (double)afile->arate / 4.); */
  pulse_audio_seek_bytes(pulsed, seekstart, afile);
  return TRUE;
}


int64_t pulse_audio_seek_bytes(pulse_driver_t *pulsed, int64_t bytes, lives_clip_t *sfile) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file
  volatile aserver_message_t *pmsg;
  int qnt;

  // set this here so so that pulse_get_rev_avals returns the forward seek position
  fwd_seek_pos = bytes;

  if (!pulsed->is_corked) {
    ticks_t timeout;
    lives_alarm_t alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
    pmsg = pulse_get_msgq(pulsed);
    while (pmsg && pmsg->command != ASERVER_CMD_FILE_SEEK && (timeout = lives_alarm_check(alarm_handle)) > 0) {
      lives_nanosleep(1000);
      pmsg = pulse_get_msgq(pulsed);
    }
    lives_alarm_clear(alarm_handle);

    if (timeout == 0 || pulsed->playing_file == -1) {
      if (timeout == 0) LIVES_WARN("PA connect timed out");
      return 0;
    }
  }

  if (bytes < 0) bytes = 0;
  if (bytes > sfile->afilesize) bytes = sfile->afilesize;

  qnt = sfile->achans * (sfile->asampsize >> 3);
  bytes = ALIGN_CEIL64((int64_t)bytes, qnt);

  pulse_message2.command = ASERVER_CMD_FILE_SEEK;
  pulse_message2.next = NULL;
  pulse_message2.data = lives_strdup_printf("%"PRId64, bytes);
  pulse_message2.tc = 0;
  if (!pulsed->msgq) pulsed->msgq = &pulse_message2;
  else pulsed->msgq->next = &pulse_message2;
  return bytes;
}


boolean pulse_try_reconnect(void) {
  lives_alarm_t alarm_handle;
  do_threaded_dialog(_("Resetting pulseaudio connection..."), FALSE);

  pulse_shutdown();
  mainw->pulsed = NULL;
  if (prefs->pa_restart && !prefs->vj_mode) {
    char *com = lives_strdup_printf("%s %s", EXEC_PULSEAUDIO, prefs->pa_start_opts);
    lives_system(com, TRUE);
    lives_free(com);
  } else lives_system("pulseaudio -k", TRUE);
  alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
  while (lives_alarm_check(alarm_handle) > 0) {
    sched_yield();
    lives_usleep(prefs->sleep_time);
    threaded_dialog_spin(0.);
  }
  lives_alarm_clear(alarm_handle);
  if (!lives_pulse_init(9999)) {
    end_threaded_dialog();
    goto err123; // init server failed
  }
  pulse_audio_init(); // reset vars
  pulse_audio_read_init(); // reset vars
  mainw->pulsed = pulse_get_driver(TRUE);
  if (pulse_driver_activate(mainw->pulsed)) { // activate driver
    goto err123;
  }
  pulse_rec_audio_to_clip(-1, -1, RECA_MONITOR);
  end_threaded_dialog();
  d_print(_("\nConnection to pulseaudio was reset.\n"));
  return TRUE;

err123:
  mainw->aplayer_broken = TRUE;
  mainw->pulsed = NULL;
  do_pulse_lost_conn_error();
  return FALSE;
}


/**
  @brief prepare to play file fileno
  - set loop mode
  - check if we need to reconnect
  - set vals
*/
void pulse_aud_pb_ready(int fileno) {
  char *tmpfilename = NULL;
  lives_clip_t *sfile = mainw->files[fileno];
  int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
  int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

  lives_freep((void **) & (mainw->pulsed->aPlayPtr->data));
  mainw->pulsed->aPlayPtr->size = mainw->pulsed->aPlayPtr->max_size = 0;
  if (mainw->pulsed) pulse_driver_uncork(mainw->pulsed);

  // called at pb start and rec stop (after rec_ext_audio)
  if (mainw->pulsed && mainw->aud_rec_fd == -1) {
    mainw->pulsed->is_paused = FALSE;
    mainw->pulsed->mute = mainw->mute;
    if ((mainw->loop_cont || mainw->whentostop != STOP_ON_AUD_END) && !mainw->preview) {
      if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && !mainw->event_list
          && (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
              || ((prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG))))
        mainw->pulsed->loop = AUDIO_LOOP_PINGPONG;
      else mainw->pulsed->loop = AUDIO_LOOP_FORWARD;
    } else mainw->pulsed->loop = AUDIO_LOOP_NONE;
    if (sfile->achans > 0 && (!mainw->preview || (mainw->preview && mainw->is_processing)) &&
        (sfile->laudio_time > 0. || sfile->opening ||
         (mainw->multitrack && mainw->multitrack->is_rendering &&
          lives_file_test((tmpfilename = lives_get_audio_file_name(fileno)), LIVES_FILE_TEST_EXISTS)))) {
      ticks_t timeout;
      lives_alarm_t alarm_handle;

      lives_freep((void **)&tmpfilename);
      mainw->pulsed->in_achans = sfile->achans;
      mainw->pulsed->in_asamps = sfile->asampsize;
      mainw->pulsed->in_arate = sfile->arate;
      mainw->pulsed->usigned = !asigned;
      mainw->pulsed->seek_end = sfile->afilesize;
      mainw->pulsed->seek_pos = 0;

      if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN)) || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
        mainw->pulsed->reverse_endian = TRUE;
      else mainw->pulsed->reverse_endian = FALSE;

      alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
      while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed)) {
        sched_yield(); // wait for seek
        lives_usleep(prefs->sleep_time);
      }
      lives_alarm_clear(alarm_handle);

      if (timeout == 0) pulse_try_reconnect();

      if ((!mainw->multitrack || mainw->multitrack->is_rendering ||
           sfile->opening) && (!mainw->event_list || mainw->record || (mainw->preview && mainw->is_processing))) {
        // tell pulse server to open audio file and start playing it
        pulse_message.command = ASERVER_CMD_FILE_OPEN;
        pulse_message.data = lives_strdup_printf("%d", fileno);
        pulse_message.next = NULL;
        mainw->pulsed->msgq = &pulse_message;
        pulse_audio_seek_bytes(mainw->pulsed, sfile->aseek_pos, sfile);
        if (seek_err) {
          if (pulse_try_reconnect()) pulse_audio_seek_bytes(mainw->pulsed, sfile->aseek_pos, sfile);
        }
        mainw->rec_aclip = fileno;
        mainw->rec_avel = sfile->pb_fps / sfile->fps;
        mainw->rec_aseek = (double)sfile->aseek_pos / (double)(sfile->arps * sfile->achans * (sfile->asampsize / 8));
      }
    }
    if (mainw->agen_key != 0 && !mainw->multitrack) mainw->pulsed->in_use = TRUE; // audio generator is active
  }
}

#undef afile

#endif

