// pulse.c
// LiVES (lives-exe)
// (c) G. Finch <salsaman+lives@gmail.com> 2005 - 2018
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO
//#define CACHE_TEST
#include "main.h"
#include "callbacks.h"
#include "support.h"
#include "effects.h"
#include "effects-weed.h"

#define afile mainw->files[pulsed->playing_file]

//#define DEBUG_PULSE

static pulse_driver_t pulsed;
static pulse_driver_t pulsed_reader;

static pa_threaded_mainloop *pa_mloop = NULL;
static pa_context *pcon = NULL;

static uint32_t pulse_server_rate = 0;

#define PULSE_READ_BYTES 48000

static uint8_t prbuf[PULSE_READ_BYTES * 2];

static size_t prb = 0;

static boolean seek_err;

///////////////////////////////////////////////////////////////////

LIVES_GLOBAL_INLINE void pa_mloop_lock(void) {
  if (!pa_threaded_mainloop_in_thread(pa_mloop)) {
    pa_threaded_mainloop_lock(pa_mloop);
  } else {
    LIVES_ERROR("tried to lock pa mainloop within audio thread");
  }
}

LIVES_GLOBAL_INLINE void pa_mloop_unlock(void) {
  if (!pa_threaded_mainloop_in_thread(pa_mloop))
    pa_threaded_mainloop_unlock(pa_mloop);
  else {
    LIVES_ERROR("tried to unlock pa mainloop within audio thread");
  }
}


static void pulse_server_cb(pa_context *c, const pa_server_info *info, void *userdata) {
  if (info == NULL) {
    pulse_server_rate = 0;
  } else {
    pulse_server_rate = info->sample_spec.rate;
  }
  pa_threaded_mainloop_signal(pa_mloop, 0);
}


static void pulse_success_cb(pa_stream *stream, int i, void *userdata) {
  pa_threaded_mainloop_signal(pa_mloop, 0);
}


static void stream_underflow_callback(pa_stream *s, void *userdata) {
  //pulse_driver_t *pulsed = (pulse_driver_t *)userdata;
  // we get isolated cases when the GUI is very busy, for example right after playback
  // we should ignore these isolated cases, except in DEBUG mode.
  // otherwise - increase tlen ?
  // e.g. pa_stream_set_buffer_attr(s, battr, success_cb, NULL);

  if (prefs->show_dev_opts) {
    fprintf(stderr, "PA Stream underrun.\n");
  }
}


static void stream_overflow_callback(pa_stream *s, void *userdata) {
  fprintf(stderr, "Stream overrun.\n");
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

  if (pa_mloop != NULL) return TRUE;

  pa_mloop = pa_threaded_mainloop_new();
  pcon = pa_context_new(pa_threaded_mainloop_get_api(pa_mloop), "LiVES");
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

    LIVES_WARN("Unable to connect to the pulseaudio server");

    if (!mainw->foreign) {
      if (startup_phase == 0 && capable->has_sox_play) {
        do_error_dialog_with_check(
          _("\nUnable to connect to the pulseaudio server.\nFalling back to sox audio player.\nYou can change this in Preferences/Playback.\n"),
          WARN_MASK_NO_PULSE_CONNECT);
        switch_aud_to_sox(prefs->warning_mask & WARN_MASK_NO_PULSE_CONNECT);
      } else if (startup_phase == 0) {
        do_error_dialog_with_check(
          _("\nUnable to connect to the pulseaudio server.\nFalling back to none audio player.\nYou can change this in Preferences/Playback.\n"),
          WARN_MASK_NO_PULSE_CONNECT);
        switch_aud_to_none(TRUE);
      } else {
        msg = lives_strdup(_("\nUnable to connect to the pulseaudio server.\n"));
        if (startup_phase != 2) {
          do_blocking_error_dialog(msg);
          mainw->aplayer_broken = TRUE;
        } else {
          do_blocking_error_dialogf("%s%s", msg, _("LiVES will exit and you can choose another audio player.\n"));
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
    mainw->rec_aseek = (double)pulsed->real_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
    mainw->rec_avel = SIGNED_DIVIDE((double)pulsed->in_arate, (double)afile->arate);
  }
}


static void pulse_set_rec_avals(pulse_driver_t *pulsed) {
  // record direction change (internal)
  mainw->rec_aclip = pulsed->playing_file;
  if (mainw->rec_aclip != -1) {
    pulse_get_rec_avals(pulsed);
  }
}


#if !HAVE_PA_STREAM_BEGIN_WRITE
static void pulse_buff_free(void *ptr) {
  lives_free(ptr);
}
#endif


static void sample_silence_pulse(pulse_driver_t *pdriver, size_t nbytes, size_t xbytes) {
  uint8_t *buff;
  int nsamples;

  if (xbytes <= 0) return;
  if (mainw->aplayer_broken) return;
  while (nbytes > 0) {
    int ret = 0;
#if HAVE_PA_STREAM_BEGIN_WRITE
    xbytes = -1;
    // returns a buffer and size for us to write to
    pa_stream_begin_write(pdriver->pstream, (void **)&buff, &xbytes);
#endif
    if (nbytes < xbytes) xbytes = nbytes;
#if !HAVE_PA_STREAM_BEGIN_WRITE
    buff = (uint8_t *)lives_calloc(xbytes);
#endif
    if (!buff || ret != 0) {
      return;
    }
#if HAVE_PA_STREAM_BEGIN_WRITE
    lives_memset(buff, 0, xbytes);
#endif
    if (pdriver->astream_fd != -1) audio_stream(buff, xbytes, pdriver->astream_fd); // old streaming API

    nsamples = xbytes / pdriver->out_achans / (pdriver->out_asamps >> 3);

    // new streaming API
    if (mainw->ext_audio && mainw->vpp != NULL && mainw->vpp->render_audio_frame_float != NULL && pdriver->playing_file != -1
        && pdriver->playing_file != mainw->ascrap_file) {
      sample_silence_stream(pdriver->out_achans, nsamples);
    }

    pthread_mutex_lock(&mainw->abuf_frame_mutex);
    if (mainw->audio_frame_buffer != NULL && prefs->audio_src != AUDIO_SRC_EXT
        && (mainw->event_list == NULL || mainw->record || mainw->record_paused))  {
      // buffer audio for any generators
      // interlaced, so we paste all to channel 0
      append_to_audio_buffer16(buff, nsamples * pdriver->out_achans, 0);
      mainw->audio_frame_buffer->samples_filled += nsamples * pdriver->out_achans;
    }
    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
#if !HAVE_PA_STREAM_BEGIN_WRITE
    pa_stream_write(pdriver->pstream, buff, xbytes, pulse_buff_free, 0, PA_SEEK_RELATIVE);
#else
    pa_stream_write(pdriver->pstream, buff, xbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
    nbytes -= xbytes;
  }
}


#ifdef CACHE_TEST
static void push_cache_buffer(lives_audio_buf_t *cache_buffer, pulse_driver_t *pulsed,
                              size_t in_bytes, size_t nframes, double shrink_factor) {
  // push a cache_buffer for another thread to fill

  if (mainw->ascrap_file > -1 && pulsed->playing_file == mainw->ascrap_file) cache_buffer->sequential = TRUE;
  else cache_buffer->sequential = FALSE;

  cache_buffer->fileno = pulsed->playing_file;
  cache_buffer->seek = pulsed->seek_pos;
  cache_buffer->bytesize = in_bytes;

  cache_buffer->in_achans = pulsed->in_achans;
  cache_buffer->out_achans = pulsed->out_achans;

  cache_buffer->in_asamps = afile->asampsize;
  cache_buffer->out_asamps = 16;

  cache_buffer->shrink_factor = shrink_factor;

  cache_buffer->swap_sign = pulsed->usigned;
  cache_buffer->swap_endian = pulsed->reverse_endian ? SWAP_X_TO_L : 0;

  cache_buffer->samp_space = nframes;

  cache_buffer->in_interleaf = TRUE;
  cache_buffer->out_interleaf = TRUE;

  cache_buffer->operation = LIVES_READ_OPERATION;
  cache_buffer->is_ready = FALSE;
#ifdef TEST_COND
  wake_audio_thread();
#endif
}


LIVES_INLINE lives_audio_buf_t *pop_cache_buffer(void) {
  // get next available cache_buffer
  return audio_cache_get_buffer();
}
#endif

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

  uint64_t nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);

  aserver_message_t *msg;

  int64_t seek, xseek;
  int new_file;
  char *filename;
  boolean from_memory = FALSE;

  uint8_t *buffer;
  size_t xbytes = pa_stream_writable_size(pstream);

  boolean needs_free = FALSE;
#ifdef CACHE_TEST
  lives_audio_buf_t *cache_buffer = NULL;
  boolean wait_cache_buffer = FALSE;
#endif
  size_t offs = 0;
  boolean got_cmd = FALSE;
  pa_volume_t pavol;

  pulsed->real_seek_pos = pulsed->seek_pos;

  pulsed->pstream = pstream;

  if (xbytes > nbytes) xbytes = nbytes;

  if (!mainw->is_ready || pulsed == NULL || (!LIVES_IS_PLAYING && pulsed->msgq == NULL) || nbytes > NBYTES_LIMIT) {
    sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
    //g_print("pt a1 %ld %d %p %d %p %ld\n",nsamples, mainw->is_ready, pulsed, mainw->playing_file, pulsed->msgq, nbytes);
    return;
  }

  /// handle control commands from the main (video) thread
  if ((msg = (aserver_message_t *)pulsed->msgq) != NULL) {
    got_cmd = TRUE;
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file = atoi((char *)msg->data);
      if (pulsed->playing_file != new_file) {
        filename = lives_get_audio_file_name(new_file);
        pulsed->fd = lives_open2(filename, O_RDONLY);
        if (pulsed->fd == -1) {
          // dont show gui errors - we are running in realtime thread
          LIVES_ERROR("pulsed: error opening");
          LIVES_ERROR(filename);
          pulsed->playing_file = -1;
        } else {
#ifdef HAVE_POSIX_FADVISE
          if (new_file == mainw->ascrap_file) {
            posix_fadvise(pulsed->fd, 0, 0, POSIX_FADV_SEQUENTIAL);
          }
#endif
        }
        lives_free(filename);
      }
      pulsed->real_seek_pos = pulsed->seek_pos = 0;
      pulsed->playing_file = new_file;
      pa_stream_trigger(pulsed->pstream, NULL, NULL);
      break;
    case ASERVER_CMD_FILE_CLOSE:
      if (pulsed->fd >= 0) close(pulsed->fd);
      if (pulsed->sound_buffer == pulsed->aPlayPtr->data) pulsed->sound_buffer = NULL;
      if (pulsed->aPlayPtr->data != NULL) {
        lives_free((void *)(pulsed->aPlayPtr->data));
        pulsed->aPlayPtr->data = NULL;
      }
      pulsed->aPlayPtr->max_size = 0;
      pulsed->aPlayPtr->size = 0;
      pulsed->fd = -1;
      pulsed->playing_file = -1;
      pulsed->in_use = FALSE;
      break;
    case ASERVER_CMD_FILE_SEEK:
      if (pulsed->fd < 0) break;
      xseek = seek = atol((char *)msg->data);
      if (seek < 0.) xseek = 0.;
      if (mainw->preview || (mainw->agen_key == 0 && !mainw->agen_needs_reinit)) {
        xseek = ALIGN_CEILNG(xseek, afile->achans * (afile->asampsize >> 3));
        lseek(pulsed->fd, xseek, SEEK_SET);
      }
      pulsed->real_seek_pos = pulsed->seek_pos = seek;
      pa_stream_trigger(pulsed->pstream, NULL, NULL);
      pulsed->in_use = TRUE;
      break;
    default:
      pulsed->msgq = NULL;
      msg->data = NULL;
    }
    if (msg->data != NULL && msg->next != msg) {
      lives_free((char *)(msg->data));
      msg->data = NULL;
    }
    msg->command = ASERVER_CMD_PROCESSED;
    pulsed->msgq = msg->next;
    if (pulsed->msgq != NULL && pulsed->msgq->next == pulsed->msgq) pulsed->msgq->next = NULL;
  }
  if (got_cmd) {
    sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
    return;
  }

  if (pulsed->chunk_size != nbytes) pulsed->chunk_size = nbytes;

  pulsed->state = pa_stream_get_state(pulsed->pstream);

  if (pulsed->state == PA_STREAM_READY) {
    uint64_t pulseFramesAvailable = nsamples;
    uint64_t inputFramesAvailable = 0;
    uint64_t numFramesToWrite = 0;
    int64_t in_frames = 0;
    uint64_t in_bytes = 0, xin_bytes = 0;
    float shrink_factor = 1.f;
    int swap_sign;

#ifdef DEBUG_PULSE
    lives_printerr("playing... pulseFramesAvailable = %ld\n", pulseFramesAvailable);
#endif

    pulsed->num_calls++;

    /// not in use, just send silence
    if (!pulsed->in_use || (pulsed->read_abuf > -1 && !LIVES_IS_PLAYING)
        || ((((pulsed->fd < 0 || pulsed->seek_pos < 0.) &&
              pulsed->read_abuf < 0) &&
             ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack != NULL))
            || pulsed->is_paused || (mainw->pulsed_read != NULL && mainw->pulsed_read->playing_file != -1))) {
      sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);

      if (!pulsed->is_paused) pulsed->frames_written += nsamples;
      if (pulsed->seek_pos < 0. && pulsed->playing_file > -1 && afile != NULL) {
        pulsed->seek_pos += nsamples * afile->achans * afile->asampsize / 8;
        if (pulsed->seek_pos >= 0) pulse_audio_seek_bytes(pulsed, pulsed->seek_pos, afile);
      }
#ifdef DEBUG_PULSE
      g_print("pt a3 %d\n", pulsed->in_use);
#endif
      return;
    }

    // time interpolation
    pulsed->extrausec += (float)nbytes / (float)(pulsed->out_arate * pulsed->out_achans * pulsed->out_asamps) * 1000000000.;

    if (LIVES_LIKELY(pulseFramesAvailable > 0 && (pulsed->read_abuf > -1 ||
                     (pulsed->aPlayPtr != NULL
                      && pulsed->in_achans > 0) ||
                     (((mainw->agen_key != 0 || mainw->agen_needs_reinit) && !mainw->preview)
                      && mainw->multitrack == NULL)))) {

      if (LIVES_IS_PLAYING && pulsed->read_abuf > -1) {
        // playing back from memory buffers instead of from file
        // this is used in multitrack
        from_memory = TRUE;
        numFramesToWrite = pulseFramesAvailable;
        pulsed->frames_written += numFramesToWrite;
        pulseFramesAvailable -= numFramesToWrite;
      } else {
        // from file or audio generator
        if (LIVES_LIKELY(pulsed->fd >= 0)) {
          int playfile = mainw->playing_file;
          pulsed->seek_end = 0;

          if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && IS_VALID_CLIP(pulsed->playing_file))
            pulsed->seek_end = afile->afilesize;

          if (pulsed->seek_end == 0 || ((pulsed->playing_file == mainw->ascrap_file && !mainw->preview) && playfile >= -1
                                        && mainw->files[playfile] != NULL && mainw->files[playfile]->achans > 0)) {
            pulsed->seek_end = INT64_MAX;
          }
#ifdef CACHE_TEST
          cache_buffer = pop_cache_buffer();
          if (cache_buffer != NULL && cache_buffer->fileno == -1) pulsed->playing_file = -1;
          if (cache_buffer != NULL && cache_buffer->in_achans > 0 && !cache_buffer->is_ready) wait_cache_buffer = TRUE;
#endif
          /// calculate how much to read
          pulsed->aPlayPtr->size = 0;
          in_bytes = ABS((in_frames = ((double)pulsed->in_arate / (double)pulsed->out_arate *
                                       (double)pulseFramesAvailable + ((double)fastrand() / (double)LIVES_MAXUINT64))))
                     * pulsed->in_achans * (pulsed->in_asamps >> 3);
#ifdef DEBUG_PULSE
          g_print("in bytes=%ld %d %d %lu %lu %lu\n", in_bytes, pulsed->in_arate, pulsed->out_arate, pulseFramesAvailable,
                  pulsed->in_achans, pulsed->in_asamps);
#endif
          shrink_factor = (float)in_frames / (float)pulseFramesAvailable;

          /// expand the buffer if necessary
          if (LIVES_UNLIKELY((in_bytes > pulsed->aPlayPtr->max_size && !(*pulsed->cancelled) && ABS(shrink_factor) <= 100.f))) {
            boolean update_sbuffer = FALSE;
            if (pulsed->sound_buffer == pulsed->aPlayPtr->data) update_sbuffer = TRUE;
            if (pulsed->aPlayPtr->data != NULL) lives_free((void *)(pulsed->aPlayPtr->data));
            pulsed->aPlayPtr->data = lives_calloc_safety(in_bytes / 4 + 1, 4);
            if (update_sbuffer) pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data); // ASSIG to p.sb
            if (pulsed->aPlayPtr->data != NULL) {
              pulsed->aPlayPtr->max_size = in_bytes;
            } else {
              pulsed->aPlayPtr->max_size = 0;
            }
          }

          // update looping mode
          if (mainw->whentostop == NEVER_STOP) {
            if (mainw->ping_pong && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
                && ((prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) || mainw->current_file == pulsed->playing_file)
                && (mainw->event_list == NULL || mainw->record || mainw->record_paused)
                && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
              pulsed->loop = AUDIO_LOOP_PINGPONG;
            else pulsed->loop = AUDIO_LOOP_FORWARD;
          } else {
            pulsed->loop = AUDIO_LOOP_NONE;
            //in_bytes = 0;
          }

          if (shrink_factor  > 0.) {
            // forward playback
            if ((mainw->agen_key == 0 || mainw->multitrack != NULL || mainw->preview) && in_bytes > 0) {
              pulsed->aPlayPtr->size = read(pulsed->fd, (void *)(pulsed->aPlayPtr->data), in_bytes);
            } else pulsed->aPlayPtr->size = in_bytes;
            pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data); // ASSIG to p.sb
            pulsed->seek_pos += in_bytes;
            if (pulsed->seek_pos >= pulsed->seek_end && pulsed->playing_file != mainw->ascrap_file && !afile->opening) {
              if (pulsed->loop == AUDIO_LOOP_NONE) {
                if (*pulsed->whentostop == STOP_ON_AUD_END) {
                  *pulsed->cancelled = CANCEL_AUD_END;
                }
                in_bytes = 0;
              } else {
                if (pulsed->loop == AUDIO_LOOP_PINGPONG && ((pulsed->playing_file != mainw->playing_file)
                    || clip_can_reverse(mainw->playing_file))) {
                  pulsed->in_arate = -pulsed->in_arate;
                  pulsed->seek_pos -= in_bytes;
                } else {
                  pulsed->seek_pos = pulsed->real_seek_pos = 0;
                }
                if (mainw->record && !mainw->record_paused)
                  pulse_set_rec_avals(pulsed);
		// *INDENT-OFF*
              }}}
	  // *INDENT-ON*

          else if (pulsed->playing_file != mainw->ascrap_file && shrink_factor  < 0.f) {
            /// reversed playback
            if ((pulsed->seek_pos -= in_bytes) < 0) {
              if (pulsed->loop == AUDIO_LOOP_NONE) {
                if (*pulsed->whentostop == STOP_ON_AUD_END) {
                  *pulsed->cancelled = CANCEL_AUD_END;
                }
                in_bytes = 0;
              } else {
                if (pulsed->loop == AUDIO_LOOP_PINGPONG && ((pulsed->playing_file != mainw->playing_file)
                    || clip_can_reverse(mainw->playing_file))) {
                  pulsed->in_arate = -pulsed->in_arate;
                  shrink_factor = -shrink_factor;
                  pulsed->seek_pos = -pulsed->seek_pos;
                } else pulsed->seek_pos += pulsed->seek_end;
              }
              pulsed->real_seek_pos = pulsed->seek_pos;
              pulse_set_rec_avals(pulsed);
            }
          }

          if (((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->preview ||
               mainw->multitrack != NULL) && in_bytes > 0) {
            int qnt = afile->achans * (afile->asampsize >> 3);
            pulsed->seek_pos = ALIGN_CEILNG(pulsed->seek_pos, qnt);
            lseek(pulsed->fd, pulsed->seek_pos, SEEK_SET);
          }

          if (shrink_factor < 0 && (mainw->agen_key == 0 || mainw->multitrack != NULL || mainw->preview) && in_bytes > 0) {
            pulsed->aPlayPtr->size = read(pulsed->fd, (void *)(pulsed->aPlayPtr->data), in_bytes);
            pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data); // ASSIG to p.sb
          }

          /// put silence if anything changed
          if (pulsed->mute || in_bytes == 0 || pulsed->aPlayPtr->size == 0 || !IS_VALID_CLIP(pulsed->playing_file)
              || (pulsed->aPlayPtr->data == NULL && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) ||
                  mainw->multitrack != NULL || mainw->preview))) {
            sample_silence_pulse(pulsed, nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3), xbytes);
            if (!pulsed->is_paused) pulsed->frames_written += nsamples;
#ifdef DEBUG_PULSE
            g_print("pt a4\n");
#endif
            return;
          }
        }

        if (mainw->agen_key != 0 && mainw->multitrack == NULL && !mainw->preview) {
          in_bytes = pulseFramesAvailable * pulsed->out_achans * 2;
          if (xin_bytes == 0) xin_bytes = in_bytes;
        }

        if ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack != NULL || (mainw->preview &&
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
          buffer = (uint8_t *)pulsed->aPlayPtr->data; // ASSIG to buffer
          ////
          numFramesToWrite = MIN(pulseFramesAvailable, (inputFramesAvailable / ABS(shrink_factor) + .001)); // VALGRIND

#ifdef DEBUG_PULSE
          lives_printerr("inputFramesAvailable after conversion %ld\n", (uint64_t)((double)inputFramesAvailable / shrink_factor + .001));
          lives_printerr("nsamples == %ld, pulseFramesAvailable == %ld,\n\tpulsed->num_input_channels == %ld, pulsed->out_achans == %ld\n",
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
            pulsed->sound_buffer = (uint8_t *)lives_calloc_safety(pulsed->chunk_size, 1);
            if (!pulsed->sound_buffer) {
              sample_silence_pulse(pulsed, nsamples * pulsed->out_achans *
                                   (pulsed->out_asamps >> 3), xbytes);
              if (!pulsed->is_paused) pulsed->frames_written += nsamples;
#ifdef DEBUG_PULSE
              g_print("pt X2\n");
#endif
              return;
            }

            /// convert 8 bit to 16 if applicable, and resample to out rate
            if (pulsed->in_asamps == 8) {
              sample_move_d8_d16((short *)(pulsed->sound_buffer), (uint8_t *)buffer, numFramesToWrite, in_bytes,
                                 shrink_factor, pulsed->out_achans, pulsed->in_achans, swap_sign ? SWAP_U_TO_S : 0);
            } else {
              sample_move_d16_d16((short *)pulsed->sound_buffer, (short *)buffer, numFramesToWrite, in_bytes, shrink_factor,
                                  pulsed->out_achans, pulsed->in_achans, pulsed->reverse_endian ? SWAP_X_TO_L : 0,
                                  swap_sign ? SWAP_U_TO_S : 0);
            }
          }

          if ((has_audio_filters(AF_TYPE_ANY) || mainw->ext_audio) && (pulsed->playing_file != mainw->ascrap_file)) {
            boolean memok = TRUE;
            float **fltbuf = (float **)lives_calloc(pulsed->out_achans, sizeof(float *));
            register int i;

            /// we have audio filters... convert to float, pass through any audio filters, then back to s16
            for (i = 0; i < pulsed->out_achans; i++) {
              // convert s16 to non-interleaved float
              fltbuf[i] = (float *)lives_calloc_safety(numFramesToWrite, sizeof(float));
              if (fltbuf[i] == NULL) {
                memok = FALSE;
                for (--i; i >= 0; i--) {
                  lives_freep((void **)&fltbuf[i]);
                }
                break;
              }
              /// convert to float, and take the opportunity to find the max volume
              /// (currently this is used to trigger recording start optionally)
              pulsed->abs_maxvol_heard = sample_move_d16_float(fltbuf[i], (short *)pulsed->sound_buffer + i,
                                         numFramesToWrite, pulsed->out_achans, FALSE, FALSE, 1.0);
            }

            if (memok) {
              ticks_t tc = mainw->currticks;
              // apply any audio effects with in_channels

              if (has_audio_filters(AF_TYPE_ANY)) {
                /** we create an Audio Layer and then call weed_apply_audio_effects_rt. The layer data is copied by ref
                  to the in channel of the filter and then from the out channel back to the layer. IF the filter supports inplace then
                  we get the same buffers back, otherwise we will get newly allocated ones, we copy by ref back to our audio buf
                  and feed the result to the player as usual */
                weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                weed_layer_set_audio_data(layer, fltbuf, pulsed->out_arate, pulsed->out_achans, numFramesToWrite);
                weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                lives_free(fltbuf);
                fltbuf = weed_layer_get_audio_data(layer, NULL);
                weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                weed_layer_free(layer);
              }

              pthread_mutex_lock(&mainw->vpp_stream_mutex);
              if (mainw->ext_audio && mainw->vpp != NULL && mainw->vpp->render_audio_frame_float != NULL) {
                (*mainw->vpp->render_audio_frame_float)(fltbuf, numFramesToWrite);
              }
              pthread_mutex_unlock(&mainw->vpp_stream_mutex);

              // convert float audio back to s16 in pulsed->sound_buffer
              sample_move_float_int(pulsed->sound_buffer, fltbuf, numFramesToWrite, 1.0, pulsed->out_achans, PA_SAMPSIZE, 0,
                                    (capable->byte_order == LIVES_LITTLE_ENDIAN), FALSE, 1.0);

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
            for (int i = 0; i < pulsed->out_achans; i++) fltbuf[i] = (float *)lives_calloc_safety(numFramesToWrite, sizeof(float));
            if (!get_audio_from_plugin(fltbuf, pulsed->out_achans, pulsed->out_arate, numFramesToWrite, TRUE)) {
              pl_error = TRUE;
            }
          }

          if (!pl_error) {
            if (LIVES_UNLIKELY(nbytes > pulsed->aPlayPtr->max_size)) {
              boolean update_sbuffer = FALSE;
              if (pulsed->sound_buffer == pulsed->aPlayPtr->data) update_sbuffer = TRUE;
              if (pulsed->aPlayPtr->data != NULL) lives_free((void *)(pulsed->aPlayPtr->data));
              pulsed->aPlayPtr->data = lives_calloc_safety(nbytes / 4 + 1, 4);
              //g_print("realloc 2\n");
              if (update_sbuffer) pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data); // ASSIG to p.sb
              if (pulsed->aPlayPtr->data != NULL) {
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
            if (mainw->ext_audio && mainw->vpp != NULL && mainw->vpp->render_audio_frame_float != NULL) {
              (*mainw->vpp->render_audio_frame_float)(fltbuf, numFramesToWrite);
            }
            pthread_mutex_unlock(&mainw->vpp_stream_mutex);

            // copy effected audio bac k into pulsed->aPlayPtr->data
            pulsed->sound_buffer = (uint8_t *)pulsed->aPlayPtr->data; // ASSIG to p.sb

            sample_move_float_int(pulsed->sound_buffer, fltbuf, numFramesToWrite, 1.0,
                                  pulsed->out_achans, PA_SAMPSIZE, 0, (capable->byte_order == LIVES_LITTLE_ENDIAN), FALSE, 1.0);

            if (fltbuf != NULL) {
              register int i;
              for (i = 0; i < pulsed->out_achans; i++) {
                lives_freep((void **)&fltbuf[i]);
              }
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
                                  nbytes, mainw->files[mainw->ascrap_file]->signed_endian & AFORM_BIG_ENDIAN, pulsed->sound_buffer);
            mainw->files[mainw->ascrap_file]->aseek_pos += rbytes;
          }
          /* // end from gen */
        }

        pulsed->frames_written += numFramesToWrite;
        pulseFramesAvailable -= numFramesToWrite;

#ifdef DEBUG_PULSE
        lives_printerr("pulseFramesAvailable == %ld\n", pulseFramesAvailable);
#endif

        // playback from memory or file

        if (future_prefs->volume != pulsed->volume_linear) {
          // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +) ??
          pa_operation *paop;
          pavol = pa_sw_volume_from_linear(future_prefs->volume);
          pa_cvolume_set(&pulsed->volume, pulsed->out_achans, pavol);
          paop = pa_context_set_sink_input_volume(pulsed->con, pa_stream_get_index(pulsed->pstream), &pulsed->volume, NULL, NULL);
          pa_operation_unref(paop);
          pulsed->volume_linear = future_prefs->volume;
        }

#ifdef CACHE_TEST
        if (wait_cache_buffer) {
          while (!cache_buffer->is_ready && !cache_buffer->die) {
            lives_usleep(prefs->sleep_time);
          }
          wait_cache_buffer = FALSE;
        }
#endif
      }
    }

    // buffer is reused here, it's what we'll actually push to pulse

    while (nbytes > 0) {
      if (nbytes < xbytes) xbytes = nbytes;

      if (!from_memory) {
#if !HAVE_PA_STREAM_BEGIN_WRITE
        if (xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3) <= numFramesToWrite && offs == 0) {
          buffer = pulsed->sound_buffer; // XASSIG to buffer
#if 0
        }
      }
#endif
#else
        if (0) {
          // do nothing
#endif
    } else {
      int ret = 0;
      if (pulsed->sound_buffer) {
#if HAVE_PA_STREAM_BEGIN_WRITE
        xbytes = -1;
        // returns a buffer and a max size fo us to write to
        ret = pa_stream_begin_write(pulsed->pstream, (void **)&buffer, &xbytes);
        if (nbytes < xbytes) xbytes = nbytes;
#else
        buffer = (uint8_t *)lives_malloc(nbytes);
#endif
      }
      if (!pulsed->sound_buffer || ret != 0 || !buffer) {
        sample_silence_pulse(pulsed, nsamples * pulsed->out_achans *
                             (pulsed->out_asamps >> 3), nbytes);
        if (!pulsed->is_paused) pulsed->frames_written += nsamples;
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
    pthread_mutex_lock(&mainw->abuf_frame_mutex);
    if (mainw->audio_frame_buffer != NULL && prefs->audio_src != AUDIO_SRC_EXT) {
      append_to_audio_buffer16(buffer, xbytes / 2, 0);
      mainw->audio_frame_buffer->samples_filled += xbytes / 2;
    }
    pthread_mutex_unlock(&mainw->abuf_frame_mutex);

    /// Finally... we actually write to pulse buffers
#if !HAVE_PA_STREAM_BEGIN_WRITE
    pa_stream_write(pulsed->pstream, buffer, xbytes, buffer == pulsed->aPlayPtr->data ? NULL :
                    pulse_buff_free, 0, PA_SEEK_RELATIVE);

    //if (cache_buffer != NULL && cache_buffer->buffer16 != NULL)
    //  pa_stream_write(pulsed->pstream, cache_buffer->buffer16[0], xbytes, NULL, 0, PA_SEEK_RELATIVE);
    //else
#else
    pa_stream_write(pulsed->pstream, buffer, xbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
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
        sample_silence_pulse(pulsed, nsamples * pulsed->out_achans *
                             (pulsed->out_asamps >> 3), nbytes);
        if (!pulsed->is_paused) pulsed->frames_written += nsamples;
#ifdef DEBUG_PULSE
        g_print("pt X4\n");
#endif
        return;
      }
      sample_move_abuf_int16(shortbuffer, pulsed->out_achans, (xbytes >> 1) / pulsed->out_achans, pulsed->out_arate);
      if (pulsed->astream_fd != -1) audio_stream(shortbuffer, xbytes, pulsed->astream_fd);
      /* pthread_mutex_lock(&mainw->abuf_frame_mutex); */
      /* if (mainw->audio_frame_buffer != NULL && prefs->audio_src != AUDIO_SRC_EXT) { */
      /*   append_to_audio_buffer16(shortbuffer, xbytes / 2, 0); */
      /*   mainw->audio_frame_buffer->samples_filled += xbytes / 2; */
      /* } */
      /* pthread_mutex_unlock(&mainw->abuf_frame_mutex); */
#if !HAVE_PA_STREAM_BEGIN_WRITE
      pa_stream_write(pulsed->pstream, shortbuffer, xbytes, pulse_buff_free, 0, PA_SEEK_RELATIVE);
#else
      pa_stream_write(pulsed->pstream, shortbuffer, xbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
    } else {
      sample_silence_pulse(pulsed, xbytes, xbytes);
      if (!pulsed->is_paused) pulsed->frames_written += xbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
    }
  }
  nbytes -= xbytes;
}

#ifdef CACHE_TEST
// push the cache_buffer to be filled
if (!wait_cache_buffer && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->preview)) {
  push_cache_buffer(cache_buffer, pulsed, in_bytes, nsamples, shrink_factor);
}
#endif
if (needs_free && pulsed->sound_buffer != pulsed->aPlayPtr->data && pulsed->sound_buffer != NULL) {
  lives_freep((void **)&pulsed->sound_buffer);
}

if (pulseFramesAvailable) {
#ifdef DEBUG_PULSE
  lives_printerr("buffer underrun of %ld frames\n", pulseFramesAvailable);
#endif
  xbytes = pa_stream_writable_size(pstream);
  sample_silence_pulse(pulsed, pulseFramesAvailable * pulsed->out_achans
                       * (pulsed->out_asamps >> 3), xbytes);
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

  if (data == NULL) data = prbuf;

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

  if (mainw->bad_aud_file == NULL) {
    size_t target = frames_out * (ofile->asampsize / 8) * ofile->achans, bytes;
    // use write not lives_write - because of potential threading issues
    bytes = write(mainw->aud_rec_fd, holding_buff, target);
    if (bytes > 0) {
      mainw->aud_data_written += bytes;
      if (mainw->ascrap_file != -1 && mainw->files[mainw->ascrap_file] != NULL &&
          mainw->aud_rec_fd == mainw->files[mainw->ascrap_file]->cb_src)
        add_to_ascrap_mb(bytes);
      if (mainw->aud_data_written > AUD_WRITTEN_CHECK) {
        mainw->aud_data_written = 0;
        check_for_disk_space();
      }
    }
    if (bytes < target) mainw->bad_aud_file = filename_from_fd(NULL, mainw->aud_rec_fd);
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

  pulsed->pstream = pstream;

  if (pulsed->is_corked) return;

  if (!pulsed->in_use || (mainw->playing_file < 0 && prefs->audio_src == AUDIO_SRC_EXT) || mainw->effects_paused) {
    pa_stream_peek(pulsed->pstream, (const void **)&data, &rbytes);
    if (rbytes > 0) pa_stream_drop(pulsed->pstream);
    prb = 0;
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

  if (data == NULL) {
    if (rbytes > 0) {
      pa_stream_drop(pulsed->pstream);
    }
    return;
  }

  // time interpolation
  pulsed->extrausec += (float)rbytes / (float)(pulsed->in_arate * pulsed->in_achans * pulsed->in_asamps) * 1000000000.;

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

  frames_out = (size_t)((double)((prb / (pulsed->in_asamps >> 3) / pulsed->in_achans)) / out_scale + .5);

  nsamples = (size_t)((double)((rbytes / (pulsed->in_asamps >> 3) / pulsed->in_achans)) / out_scale + .5);

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
      float **fltbuf = (float **)lives_malloc(pulsed->in_achans * sizeof(float *));
      register int i;

      size_t xnsamples = (size_t)(rbytes / (pulsed->in_asamps >> 3) / pulsed->in_achans);

      if (fltbuf == NULL) {
        pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
        pa_stream_drop(pulsed->pstream);
        return;
      }

      for (i = 0; i < pulsed->in_achans; i++) {
        // convert s16 to non-interleaved float
        fltbuf[i] = (float *)lives_malloc(xnsamples * sizeof(float));
        if (fltbuf[i] == NULL) {
          memok = FALSE;
          for (--i; i >= 0; i--) {
            lives_free(fltbuf[i]);
          }
          break;
        }

        pulsed->abs_maxvol_heard
          = sample_move_d16_float(fltbuf[i], (short *)(data) + i, xnsamples, pulsed->in_achans, FALSE, FALSE, 1.0);

        pthread_mutex_lock(&mainw->abuf_frame_mutex);
        if (mainw->audio_frame_buffer != NULL && prefs->audio_src == AUDIO_SRC_EXT) {
          // if we have audio triggered gens., push audio to it
          append_to_audio_bufferf(fltbuf[i], xnsamples, i);
          if (i == pulsed->in_achans - 1) mainw->audio_frame_buffer->samples_filled += xnsamples;
        }
        pthread_mutex_unlock(&mainw->abuf_frame_mutex);
      }

      if (memok) {
        ticks_t tc = mainw->currticks;
        // apply any audio effects with in channels but no out channels (analysers)

        if (has_audio_filters(AF_TYPE_A)) {
          weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
          weed_layer_set_audio_data(layer, fltbuf, pulsed->in_arate, pulsed->in_achans, xnsamples);
          weed_apply_audio_effects_rt(layer, tc, TRUE, TRUE);
          lives_free(fltbuf);
          fltbuf = weed_layer_get_audio_data(layer, NULL);
          weed_layer_free(layer);
        }
        // stream audio to video playback plugin if appropriate (probably needs retesting...)
        pthread_mutex_lock(&mainw->vpp_stream_mutex);
        if (mainw->ext_audio && mainw->vpp != NULL && mainw->vpp->render_audio_frame_float != NULL) {
          (*mainw->vpp->render_audio_frame_float)(fltbuf, xnsamples);
        }
        pthread_mutex_unlock(&mainw->vpp_stream_mutex);
        for (i = 0; i < pulsed->in_achans; i++) {
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
  if (pcon != NULL) {
    //g_print("pa shutdown2\n");
    pa_context_disconnect(pcon);
    pa_context_unref(pcon);
  }
  if (pa_mloop != NULL) {
    pa_threaded_mainloop_stop(pa_mloop);
    pa_threaded_mainloop_free(pa_mloop);
  }
  pcon = NULL;
  pa_mloop = NULL;
}


void pulse_close_client(pulse_driver_t *pdriver) {
  if (pdriver->pstream != NULL) {
    pa_mloop_lock();
    pa_stream_disconnect(pdriver->pstream);
    pa_stream_set_write_callback(pdriver->pstream, NULL, NULL);
    pa_stream_set_read_callback(pdriver->pstream, NULL, NULL);
    pa_stream_set_underflow_callback(pdriver->pstream, NULL, NULL);
    pa_stream_set_overflow_callback(pdriver->pstream, NULL, NULL);
    pa_stream_unref(pdriver->pstream);
    pa_mloop_unlock();
  }
  if (pdriver->pa_props != NULL) pa_proplist_free(pdriver->pa_props);
  pdriver->pa_props = NULL;
  pdriver->pstream = NULL;
}


int pulse_audio_init(void) {
  // initialise variables
#if PA_SW_CONNECTION
  int j;
#endif

  pulsed.in_use = FALSE;
  pulsed.mloop = pa_mloop;
  pulsed.con = pcon;

  //for (j = 0; j < PULSE_MAX_OUTPUT_CHANS; j++) pulsed.volume.values[j] = pa_sw_volume_from_linear(future_prefs->volume);
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
  pulsed.pstream = NULL;
  pulsed.pa_props = NULL;
  pulsed.playing_file = -1;
  pulsed.sound_buffer = NULL;
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
  return 0;
}


#if PA_SW_CONNECTION
static void info_cb(pa_context * c, const pa_sink_input_info * i, int eol, void *userdata) {
  // would be great if this worked, but apparently it always returns NULL in i
  // for a hardware connection

  // TODO: get volume_writeable (pa 1.0+)
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
  if (i == NULL) return;

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

  if (pdriver->pstream != NULL) return 0;

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

  if (capable->byte_order == LIVES_BIG_ENDIAN) {
    pdriver->out_endian = AFORM_BIG_ENDIAN;
    pa_spec.format = PA_SAMPLE_S16BE;
  } else {
    pdriver->out_endian = AFORM_LITTLE_ENDIAN;
    pa_spec.format = PA_SAMPLE_S16LE;
  }

  if (pdriver->is_output) {
    pa_battr.maxlength = LIVES_PA_BUFF_MAXLEN;
    pa_battr.tlength = LIVES_PA_BUFF_TARGET;
  } else {
    pa_battr.maxlength = LIVES_PA_BUFF_MAXLEN * 2;
    pa_battr.fragsize = LIVES_PA_BUFF_FRAGSIZE * 4;
  }

  pa_battr.minreq = (uint32_t) - 1;
  pa_battr.prebuf = -1;

  pa_mloop_lock();
  if (pulse_server_rate == 0) {
    pa_op = pa_context_get_server_info(pdriver->con, pulse_server_cb, pa_mloop);
    while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
      pa_threaded_mainloop_wait(pa_mloop);
    }
    pa_operation_unref(pa_op);
  }

  if (pulse_server_rate == 0) {
    LIVES_WARN("Problem getting pulseaudio rate...expect more problems ahead.");
    return 1;
  }

  pa_spec.rate = pdriver->out_arate = pdriver->in_arate = pulse_server_rate;

  pdriver->pstream = pa_stream_new_with_proplist(pdriver->con, pa_clientname, &pa_spec, &pa_map, pdriver->pa_props);
  pa_mloop_unlock();

  if (pdriver->is_output) {
    pa_volume_t pavol;
    pdriver->is_corked = TRUE;

#if PA_SW_CONNECTION
    pa_stream_connect_playback(pdriver->pstream, NULL, &pa_battr, (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY |
                               PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_START_CORKED |
                               PA_STREAM_AUTO_TIMING_UPDATE),
                               NULL, NULL);
#else
    pdriver->volume_linear = future_prefs->volume;
    pavol = pa_sw_volume_from_linear(pdriver->volume_linear);
    pa_cvolume_set(&pdriver->volume, pdriver->out_achans, pavol);

    pa_stream_connect_playback(pdriver->pstream, NULL, &pa_battr, (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY |
                               PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_START_CORKED | PA_STREAM_NOT_MONOTONIC |
                               PA_STREAM_AUTO_TIMING_UPDATE),
                               &pdriver->volume, NULL);
#endif

    while (pa_stream_get_state(pdriver->pstream) != PA_STREAM_READY) {
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }

    pa_stream_set_underflow_callback(pdriver->pstream, stream_underflow_callback, pdriver);
    pa_stream_set_overflow_callback(pdriver->pstream, stream_overflow_callback, pdriver);
    pa_stream_set_moved_callback(pdriver->pstream, stream_moved_callback, pdriver);
    pa_stream_set_buffer_attr_callback(pdriver->pstream, stream_buffer_attr_callback, pdriver);

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
    // set write callback
    pa_stream_set_write_callback(pdriver->pstream, pulse_audio_write_process, pdriver);
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

    pa_stream_connect_record(pdriver->pstream, NULL, &pa_battr,
                             (pa_stream_flags_t)(PA_STREAM_START_CORKED | PA_STREAM_ADJUST_LATENCY | PA_STREAM_INTERPOLATE_TIMING |
                                 PA_STREAM_AUTO_TIMING_UPDATE |
                                 PA_STREAM_NOT_MONOTONIC));

    while (pa_stream_get_state(pdriver->pstream) != PA_STREAM_READY) {
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }
    pa_stream_set_read_callback(pdriver->pstream, pulse_audio_read_process, pdriver);
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

  if (pdriver->is_corked) {
    //g_print("IS CORKED\n");
    return;
  }

  pa_mloop_lock();
  paop = pa_stream_cork(pdriver->pstream, 1, corked_cb, pdriver);
  while (pa_operation_get_state(paop) == PA_OPERATION_RUNNING)
    pa_threaded_mainloop_wait(pa_mloop);
  pa_operation_unref(paop);
  pa_mloop_unlock();

  pa_mloop_lock();
  paop = pa_stream_flush(pdriver->pstream, NULL, NULL);
  pa_operation_unref(paop);
  pa_mloop_unlock();
}


///////////////////////////////////////////////////////////////

pulse_driver_t *pulse_get_driver(boolean is_output) {
  if (is_output) return &pulsed;
  return &pulsed_reader;
}


volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *pulsed) {
  if (pulsed->pulsed_died || mainw->aplayer_broken) return NULL;
  return pulsed->msgq;
}


void pa_time_reset(pulse_driver_t *pulsed, int64_t offset) {
  pa_usec_t usec;
  pa_operation *pa_op;

  pa_mloop_lock();
  pa_op = pa_stream_update_timing_info(pulsed->pstream, pulse_success_cb, pa_mloop);

  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(pa_mloop);
  }
  pa_operation_unref(pa_op);

  pa_stream_get_time(pulsed->pstream, &usec);
  pa_mloop_unlock();
  pulsed->usec_start = usec + offset / USEC_TO_TICKS;
  pulsed->frames_written = 0;
  mainw->currticks = offset;
  mainw->deltaticks = mainw->startticks = 0;
  pulsed->extrausec = 0;
}


ticks_t lives_pulse_get_time(pulse_driver_t *pulsed) {
  // get the time in ticks since either playback started
  volatile aserver_message_t *msg = pulsed->msgq;
  pa_usec_t usec, retusec;
  static pa_usec_t last_usec = 0;
  ticks_t timeout;
  lives_alarm_t alarm_handle;
  int err;
  if (msg != NULL && (msg->command == ASERVER_CMD_FILE_SEEK || msg->command == ASERVER_CMD_FILE_OPEN)) {
    alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
    while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(pulsed) != NULL) {
      sched_yield(); // wait for seek
      lives_usleep(prefs->sleep_time);
    }
    lives_alarm_clear(alarm_handle);
    if (timeout == 0) return -1;
  }

  alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
  do {
    pa_mloop_lock();
    err = pa_stream_get_time(pulsed->pstream, &usec);
    pa_mloop_unlock();
    sched_yield();
    lives_usleep(prefs->sleep_time);
  } while ((timeout = lives_alarm_check(alarm_handle)) > 0 && usec == 0 && err == 0);
#ifdef DEBUG_PA_TIME
  g_print("gettime3 %d %ld %ld %ld %f\n", err, usec, pulsed->usec_start, (usec - pulsed->usec_start) * USEC_TO_TICKS,
          (usec - pulsed->usec_start) * USEC_TO_TICKS / 100000000.);
#endif
  lives_alarm_clear(alarm_handle);
  if (timeout == 0) return -1;

  retusec = usec;
  if (last_usec > 0 && usec <= last_usec) {
    retusec += pulsed->extrausec;
  } else pulsed->extrausec = 0;
  last_usec = usec;

  return (ticks_t)((retusec - pulsed->usec_start) * USEC_TO_TICKS);
}


double lives_pulse_get_pos(pulse_driver_t *pulsed) {
  // get current time position (seconds) in audio file
  return pulsed->real_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
}


boolean pulse_audio_seek_frame(pulse_driver_t *pulsed, int frame) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample
  int64_t seekstart;
  volatile aserver_message_t *pmsg;
  ticks_t timeout;
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);

  if (frame < 1) frame = 1;

  do {
    pmsg = pulse_get_msgq(pulsed);
  } while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pmsg != NULL && pmsg->command != ASERVER_CMD_FILE_SEEK);
  lives_alarm_clear(alarm_handle);
  if (timeout == 0 || pulsed->playing_file == -1) {
    if (timeout == 0) LIVES_WARN("PA connect timed out");
    return FALSE;
  }
  if (frame > afile->frames && afile->frames > 0) frame = afile->frames;
  seekstart = (int64_t)((double)(frame - 1.) / afile->fps * afile->arps) * afile->achans * (afile->asampsize / 8);
  pulse_audio_seek_bytes(pulsed, seekstart, afile);
  return TRUE;
}


int64_t pulse_audio_seek_bytes(pulse_driver_t *pulsed, int64_t bytes, lives_clip_t *sfile) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file
  volatile aserver_message_t *pmsg;
  int64_t seekstart;

  if (!pulsed->is_corked) {
    ticks_t timeout;
    lives_alarm_t alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);

    do {
      pmsg = pulse_get_msgq(pulsed);
    } while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pmsg != NULL && pmsg->command != ASERVER_CMD_FILE_SEEK);
    lives_alarm_clear(alarm_handle);

    if (timeout == 0 || pulsed->playing_file == -1) {
      if (timeout == 0) LIVES_WARN("PA connect timed out");
      return 0;
    }
  }

  seekstart = ((int64_t)(bytes / sfile->achans / (sfile->asampsize / 8))) * sfile->achans * (sfile->asampsize / 8);

  if (seekstart < 0) seekstart = 0;
  if (seekstart > sfile->afilesize) seekstart = sfile->afilesize;

  pulse_message2.command = ASERVER_CMD_FILE_SEEK;
  pulse_message2.next = NULL;
  pulse_message2.data = lives_strdup_printf("%"PRId64, seekstart);
  if (pulsed->msgq == NULL) pulsed->msgq = &pulse_message2;
  else pulsed->msgq->next = &pulse_message2;
  return seekstart;
}


boolean pulse_try_reconnect(void) {
  lives_alarm_t alarm_handle;
  do_threaded_dialog(_("Resetting pulseaudio connection..."), FALSE);

  pulse_shutdown();
  mainw->pulsed = NULL;

  lives_system("pulseaudio -k", TRUE);
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
  if (prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT) {
    // create reader connection now, if permanent
    pulse_rec_audio_to_clip(-1, -1, RECA_EXTERNAL);
  }
  end_threaded_dialog();
  d_print(_("\nConnection to pulseaudio was reset.\n"));
  return TRUE;

err123:
  mainw->aplayer_broken = TRUE;
  mainw->pulsed = NULL;
  do_pulse_lost_conn_error();
  return FALSE;
}


void pulse_aud_pb_ready(int fileno) {
  // TODO - can we merge with switch_audio_clip() ?

  // prepare to play file fileno
  // - set loop mode
  // - check if we need to reconnect
  // - set vals
  char *tmpfilename = NULL;
  lives_clip_t *sfile = mainw->files[fileno];
  int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
  int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

  lives_freep((void **) & (mainw->pulsed->aPlayPtr->data));
  mainw->pulsed->aPlayPtr->size = mainw->pulsed->aPlayPtr->max_size = 0;
  if (mainw->pulsed != NULL) pulse_driver_uncork(mainw->pulsed);

  // called at pb start and rec stop (after rec_ext_audio)
  if (mainw->pulsed != NULL && mainw->aud_rec_fd == -1) {
    mainw->pulsed->is_paused = FALSE;
    mainw->pulsed->mute = mainw->mute;
    if ((mainw->whentostop == NEVER_STOP) && !mainw->preview) {
      if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && mainw->event_list == NULL)
        mainw->pulsed->loop = AUDIO_LOOP_PINGPONG;
      else mainw->pulsed->loop = AUDIO_LOOP_FORWARD;
    } else mainw->pulsed->loop = AUDIO_LOOP_NONE;
    if (sfile->achans > 0 && (!mainw->preview || (mainw->preview && mainw->is_processing)) &&
        (sfile->laudio_time > 0. || sfile->opening ||
         (mainw->multitrack != NULL && mainw->multitrack->is_rendering &&
          lives_file_test((tmpfilename = lives_get_audio_file_name(fileno)), LIVES_FILE_TEST_EXISTS)))) {
      ticks_t timeout;
      lives_alarm_t alarm_handle;

      lives_freep((void **)&tmpfilename);
      mainw->pulsed->in_achans = sfile->achans;
      mainw->pulsed->in_asamps = sfile->asampsize;
      mainw->pulsed->in_arate = sfile->arate;
      mainw->pulsed->usigned = !asigned;
      mainw->pulsed->seek_end = sfile->afilesize;

      if ((aendian && (capable->byte_order == LIVES_BIG_ENDIAN)) || (!aendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
        mainw->pulsed->reverse_endian = TRUE;
      else mainw->pulsed->reverse_endian = FALSE;

      alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
      while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed) != NULL) {
        sched_yield(); // wait for seek
        lives_usleep(prefs->sleep_time);
      }
      lives_alarm_clear(alarm_handle);

      if (timeout == 0) pulse_try_reconnect();

      if ((mainw->multitrack == NULL || mainw->multitrack->is_rendering ||
           sfile->opening) && (mainw->event_list == NULL || mainw->record || (mainw->preview && mainw->is_processing))) {
        // tell pulse server to open audio file and start playing it
        pulse_message.command = ASERVER_CMD_FILE_OPEN;
        pulse_message.data = lives_strdup_printf("%d", fileno);
        pulse_message.next = NULL;
        mainw->pulsed->msgq = &pulse_message;
        pulse_audio_seek_bytes(mainw->pulsed, sfile->aseek_pos, sfile);
        if (seek_err) {
          if (pulse_try_reconnect()) pulse_audio_seek_bytes(mainw->pulsed, sfile->aseek_pos, sfile);
        }
        //mainw->pulsed->in_use = TRUE;
        mainw->rec_aclip = fileno;
        mainw->rec_avel = sfile->pb_fps / sfile->fps;
        mainw->rec_aseek = (double)sfile->aseek_pos / (double)(sfile->arps * sfile->achans * (sfile->asampsize / 8));
      }
    }
    if (mainw->agen_key != 0 && mainw->multitrack == NULL) mainw->pulsed->in_use = TRUE; // audio generator is active
  }
}

#undef afile

#endif

