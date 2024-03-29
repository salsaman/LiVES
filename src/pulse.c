// Pulse.c
// LiVES (Lives-exe)
// (c) G. Finch <salsaman+lives@gmail.com> 2005 - 2023
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO
#include "main.h"
#include "callbacks.h"
#include "effects.h"
#include "effects-weed.h"
#include "alarms.h"

#define afile mainw->files[pulsed->playing_file]

//#define DEBUG_PULSE

#define THRESH_BASE 10000.
#define THRESH_MAX 50000.

static pthread_mutex_t xtra_mutex = PTHREAD_MUTEX_INITIALIZER;

static pulse_driver_t pulsed;
static pulse_driver_t pulsed_reader;

static pa_threaded_mainloop *pa_mloop = NULL;
static pa_context *pcon = NULL;
static char pactxnm[512];

static uint32_t pulse_server_rate = 0;

static boolean seek_err;

static volatile int lock_count = 0;

static off_t fwd_seek_pos = 0;

///////////////////////////////////////////////////////////////////

static void pa_mloop_lock(void) {
  if (!pa_threaded_mainloop_in_thread(pa_mloop)) {
    if (lock_count) BREAK_ME("paloop locked");
    pa_threaded_mainloop_lock(pa_mloop);
    ++lock_count;
  } else {
    LIVES_ERROR("tried to lock pa mainloop within audio thread");
  }
}

static void pa_mloop_unlock(void) {
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
  if (info) pulse_server_rate = info->sample_spec.rate;
  pa_threaded_mainloop_signal(pa_mloop, 0);
}

static void pulse_success_cb(pa_stream *stream, int i, void *userdata) {pa_threaded_mainloop_signal(pa_mloop, 0);}

#include <sys/time.h>
#include <sys/resource.h>

static void stream_underflow_callback(pa_stream *s, void *userdata) {
  // we get isolated cases when the GUI is very busy, for example right after playback
  // we should ignore these isolated cases, except in DEBUGy mode.
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
  //BREAK_ME();
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
  LiVESResponseType resp;
  boolean retried = FALSE, is_ok = FALSE;

  if (pa_mloop) return TRUE;

retry:

  pa_mloop = pa_threaded_mainloop_new();
  lives_snprintf(pactxnm, 512, "LiVES-%"PRId64, lives_random());
  pcon = pa_context_new(pa_threaded_mainloop_get_api(pa_mloop), pactxnm);

  pa_context_connect(pcon, NULL, (pa_context_flags_t)0, NULL);
  pa_threaded_mainloop_start(pa_mloop);

  pa_mloop_lock();
  pa_state = pa_context_get_state(pcon);
  pa_mloop_unlock();

  lives_alarm_set_timeout(BILLIONS(50));
  while (!lives_alarm_triggered() && pa_state != PA_CONTEXT_READY) {
    lives_microsleep;
    pa_mloop_lock();
    pa_state = pa_context_get_state(pcon);
    pa_mloop_unlock();
  }
  lives_alarm_disarm();

  pa_mloop_lock();
  if (pa_context_get_state(pcon) == PA_CONTEXT_READY) is_ok = TRUE;
  pa_mloop_unlock();

  if (!is_ok) {
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


void pulse_set_avel(pulse_driver_t *pulsed, int clipno, double ratio) {
  if (pulsed->playing_file == mainw->ascrap_file) return;
  if (!CLIP_HAS_AUDIO(clipno)) {
    pulsed->in_arate = myround(pulsed->out_arate * ratio);
  } else {
    lives_clip_t *sfile = mainw->files[clipno];
    sfile->adirection = LIVES_DIRECTION_SIG(ratio);
    if (sfile->adirection == LIVES_DIRECTION_REVERSE) {
      pulsed->in_arate = sfile->arps * ratio - .5;
      lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
    } else {
      pulsed->in_arate = sfile->arps * ratio + .5;
      lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
    }
  }
  if (AV_CLIPS_EQUAL) pulse_get_rec_avals(pulsed);
}


void pulse_get_rec_avals(pulse_driver_t *pulsed) {
  if (RECORD_PAUSED || !LIVES_IS_RECORDING) return;
  if ((prefs->audio_opts & AUDIO_OPTS_IS_LOCKED) && mainw->ascrap_file != -1) {
    mainw->rec_aclip = mainw->ascrap_file;
    mainw->rec_aseek = mainw->files[mainw->ascrap_file]->aseek_pos;
    mainw->rec_avel = 1.;
    return;
  }
  mainw->rec_aclip = pulsed->playing_file;
  if (CLIP_HAS_AUDIO(mainw->rec_aclip)) {
    if (pulsed->is_output) {
      mainw->rec_aseek = (double)fwd_seek_pos / (double)(afile->achans * afile->asampsize / 8) / (double)afile->arps;
      mainw->rec_avel = fabs((double)pulsed->in_arate / (double)afile->arps) * (double)afile->adirection;
    } else {
      mainw->rec_aseek = (double)(afile->aseek_pos / (double)(afile->achans * afile->asampsize / 8)) / (double)afile->arps;
      mainw->rec_avel = 1.;
    }
    //g_print("RECSEEK is %f %ld\n", mainw->rec_aseek, pulsed->real_seek_pos);
  } else mainw->rec_avel = 0.;
}


static void pulse_set_rec_avals(pulse_driver_t *pulsed) {
  // record direction change (internal)
  mainw->rec_aclip = pulsed->playing_file;
  if (mainw->rec_aclip != -1) {
    pulse_get_rec_avals(pulsed);
  }
}

#if !HAVE_PA_STREAM_BEGIN_WRITE
static void pulse_buff_free(void *ptr) {lives_free(ptr);}
#endif

static uint8_t *silbuff = NULL;
static size_t silbufsz = 0;

static void sample_silence_pulse(pulse_driver_t *pulsed, ssize_t nbytes) {
#if HAVE_PA_STREAM_BEGIN_WRITE
  size_t xbytes;
  uint8_t *pa_buff;
#endif
  size_t nsamples;
  int ret = 0;
  boolean no_add = !mainw->audio_seek_ready;

  if (!nbytes) return;
  if (mainw->aplayer_broken) return;

  if (nbytes < 0) {
    no_add = TRUE;
    nbytes = -nbytes;
  }

#if HAVE_PA_STREAM_BEGIN_WRITE
  if (!pulsed->is_corked) {
    xbytes = -1;
    // returns a buffer and size for us to write to
    ret = pa_stream_begin_write(pulsed->pstream, (void **)&pa_buff, &xbytes);
    if (xbytes < nbytes) nbytes = xbytes;
  }
  if (ret != 0) goto done;
#endif

  if (nbytes > silbufsz) {
    if (silbufsz) lives_free(silbuff);
    silbufsz = 0;
    silbuff = (uint8_t *)lives_calloc(1, nbytes);
    if (!silbuff) goto done;
    silbufsz = nbytes;
  }

  if (!pulsed->is_corked) {
#if !HAVE_PA_STREAM_BEGIN_WRITE
    pa_stream_write(pulsed->pstream, silbuff, nbytes, pulse_buff_free, 0, PA_SEEK_RELATIVE);
    do_free = FALSE;
#else
    lives_memset(pa_buff, 0, nbytes);
    pa_stream_write(pulsed->pstream, pa_buff, nbytes, NULL, 0, PA_SEEK_RELATIVE);
#endif
  }

  if (pulsed->astream_fd != -1) audio_stream(silbuff, nbytes, pulsed->astream_fd); // old streaming API

  nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);

  // streaming API
  if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float && pulsed->playing_file != -1
      && pulsed->playing_file != mainw->ascrap_file) {
    sample_silence_stream(pulsed->out_achans, nsamples);
  }

  if (no_add) goto done;

  if (mainw->afbuffer && prefs->audio_src != AUDIO_SRC_EXT
      && (!mainw->event_list || mainw->record || mainw->record_paused))  {
    // silbuffer audio for any generators
    // interleaved, so we paste all to channel 0
    append_to_audio_buffer16(silbuff, nsamples, 2);
  }

  if (!pulsed->is_paused) pulsed->frames_written += nsamples;

  pulsed->real_seek_pos = pulsed->seek_pos;

  /* if (LIVES_IS_PLAYING) { */
  /*   if (IS_VALID_CLIP(pulsed->playing_file) && pulsed->seek_pos < afile->afilesize) */
  /*     afile->aseek_pos = pulsed->seek_pos; */

done:
  return;
}


#define NBYTES_LIMIT (65536 * 4)

static short *shortbuffer = NULL;

static volatile boolean in_ap = FALSE;

static void lives_pulse_set_client_attributes(pulse_driver_t *pulsed, int fileno,
    boolean activate, boolean running) {
  if (IS_VALID_CLIP(fileno)) {
    lives_clip_t *sfile = mainw->files[fileno];
    int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
    int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

    lives_freep((void **)&mainw->pulsed->aPlayPtr->data);
    mainw->pulsed->aPlayPtr->size = mainw->pulsed->aPlayPtr->max_size = 0;

    if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN))
        || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
      pulsed->reverse_endian = TRUE;
    else pulsed->reverse_endian = FALSE;

    // called from CMD_FILE_OPEN and also prepare...
    if (!running || (pulsed && mainw->aud_rec_fd == -1)) {
      int64_t astat = lives_aplayer_get_status(pulsed->inst);
      if (!running) pulsed->is_paused = FALSE;
      else {
        pulsed->is_paused = afile->play_paused;
        if (pulsed->is_paused)
          pulsed->mute = mainw->mute;
      }
      if (pulsed->is_paused) astat |= APLAYER_STATUS_PAUSED;
      else astat &= ~APLAYER_STATUS_PAUSED;
      if (pulsed->mute) astat |= APLAYER_STATUS_MUTED;
      else astat &= ~APLAYER_STATUS_MUTED;
      lives_aplayer_set_status(pulsed->inst, astat);

      if ((mainw->loop_cont || mainw->whentostop != STOP_ON_AUD_END) && !mainw->preview) {
        if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && !mainw->multitrack
            && (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
                || ((prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG))))
          pulsed->loop = AUDIO_LOOP_PINGPONG;
        else pulsed->loop = AUDIO_LOOP_FORWARD;
      } else pulsed->loop = AUDIO_LOOP_NONE;


      if ((activate || running) && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
        if (!sfile->play_paused)
          pulsed->in_arate = myround(sfile->arate * sfile->pb_fps / sfile->fps);
        else pulsed->in_arate = myround(sfile->arate * sfile->freeze_fps / sfile->fps);
      } else pulsed->in_arate = sfile->arate;

      sfile->adirection = LIVES_DIRECTION_SIG(pulsed->in_arate);
      if (sfile->adirection == LIVES_DIRECTION_REVERSE)
        pulsed->in_arate = -abs(pulsed->in_arate);
      else
        pulsed->in_arate = abs(pulsed->in_arate);

      pulsed->in_achans = sfile->achans;
      pulsed->in_asamps = sfile->asampsize;
      pulsed->usigned = !asigned;
      pulsed->seek_end = sfile->afilesize;
      pulsed->seek_pos = pulsed->real_seek_pos = fwd_seek_pos = 0;

      if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN))
          || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
        pulsed->reverse_endian = TRUE;
      else pulsed->reverse_endian = FALSE;
    }
  }
}


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
   (actual) audio sample count.

   NEW: we have two new modes:
   - "pogo" mode" in this mode we have a "locked" audiop track, which is then mixed with the audio from the current clip
   thus we need to read from two file sources, and combine the result

   - in / out mode - in this mode, the audio input is read, then mixed with the normal outuput
   the input audio may be for eaxmple from a voiceover. We add ourselves like a consumer of the audio arena,
   and pull the latest audio from the arena. Any excess data is saved for the next cycle and appended to.
   if we dont have enought we write what we have, which may cause an underflow.

   - we could use the channel mixer from multitrack to set relative volume levels

   recording:
   for pogo mode, we need to store clip/track/velocity/offsets fro both audio, as well as the relaitve volume

   - for in / out mode, we can treat the input audio as coming from a generator and use the same recording logic

   - audio effects could be applied separately to the mix channels, or to the overal mix

*/
static void pulse_audio_write_process(pa_stream *pstream, size_t nbytes, void *arg) {
  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  pa_operation *paop;
  aserver_message_t *msg;
  ssize_t pad_bytes = 0;
  uint8_t *buffer;
  uint64_t nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
#if HAVE_PA_STREAM_BEGIN_WRITE
  size_t xbytes;
#endif
  off_t seek, xseek;
  pa_volume_t pavol;
  char *filename;

  static lives_thread_data_t *tdata = NULL;
  static int async_writer_count = 0;
  static lives_proc_thread_t rec_lpt = NULL;
  static arec_details *dets = NULL;

  boolean got_cmd = FALSE;
  boolean from_memory = FALSE;
  boolean needs_free = FALSE;
  boolean cancel_rec_lpt = FALSE;

  int new_file;
  int ret = 0;

  lives_proc_thread_t self = pulsed->inst;
  //g_print("pa ping\n");

  if (!tdata) {
    tdata = get_thread_data();
    lives_thread_set_active(self);
    lives_snprintf(tdata->vars.var_origin, 128, "%s", "Pulseaudio Reader Thread");
    lives_proc_thread_include_states(self, THRD_STATE_EXTERN);
    tdata->vars.var_thrd_type = tdata->thrd_type = THRD_TYPE_AUDIO_READER;

  }

  lives_proc_thread_include_states(self, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(self, THRD_STATE_IDLING);

  in_ap = TRUE;

  //pa_thread_make_realtime(50);
  //g_print("PA\n");
  pulsed->real_seek_pos = pulsed->seek_pos;
  pulsed->pstream = pstream;


  if (rec_lpt && (!mainw->record || IS_VALID_CLIP(mainw->ascrap_file) || LIVES_IS_PLAYING)) {
    lives_proc_thread_request_cancel(rec_lpt, FALSE);
    cancel_rec_lpt = TRUE;
  }

  if (async_writer_count) {
    // here we make sure that the DATA_READY hook callbacks have all completed
    // we must do this before we can free the data from the previous cycle
    lives_hooks_async_join(NULL, DATA_READY_HOOK);
    async_writer_count = 0;
  }

  lives_aplayer_set_data_len(self, 0);
  lives_aplayer_set_data(self, NULL);





  if (cancel_rec_lpt) {
    // since we cancelled rec_lpt, then called async_join, it should have been removed from the hook_stack
    // we can now unref it, and it should be freed
    lives_proc_thread_unref(rec_lpt);
    rec_lpt = NULL;
    lives_free(dets);
    dets = NULL;
  }

  if (!mainw->is_ready || !pulsed || (!LIVES_IS_PLAYING && !pulsed->msgq)) {
    sample_silence_pulse(pulsed, -nbytes);
    //g_print("pt a1 %ld %d %p %d %p %ld\n",nsamples, mainw->is_ready, pulsed, mainw->playing_file, pulsed->msgq, nbytes);
    in_ap = FALSE;
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return;
  }

  pthread_mutex_lock(&xtra_mutex);
  pulsed->extrausec += ((double)nbytes / (double)(pulsed->out_arate) * (double)ONE_MILLION
                        / ((double)(pulsed->out_achans * (pulsed->out_asamps >> 3))) + .5);
  pthread_mutex_unlock(&xtra_mutex);

  /// handle control commands from the main (video) thread
  if ((msg = (aserver_message_t *)pulsed->msgq) != NULL) {
    int cmd = (int)msg->command;
    //got_cmd = TRUE;
    while (1) {
      if (cmd == ASERVER_CMD_PROCESSED) cmd = (int)msg->command;
      switch (cmd) {
      case ASERVER_CMD_FILE_OPEN:
        new_file = atoi((char *)msg->data);

        if (pulsed->playing_file == new_file) break;

        if (pulsed->playing_file != -1 && IS_VALID_CLIP(new_file)) {
          cmd = ASERVER_CMD_FILE_CLOSE;
          break;
        }

        got_cmd = TRUE;

        paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
        pa_operation_unref(paop);

        if (IS_VALID_CLIP(new_file)) {
          pulsed->in_use = TRUE;
          if (pulsed->playing_file != new_file) {
            pulsed->playing_file = new_file;
            lives_pulse_set_client_attributes(pulsed, new_file, FALSE, TRUE);
            if (IS_VALID_CLIP(new_file) && mainw->files[new_file]->aplay_fd > -1) {
              pulsed->fd = mainw->files[new_file]->aplay_fd;
            } else {
              filename = lives_get_audio_file_name(new_file);
              pulsed->fd = lives_open_buffered_rdonly(filename);
              lives_buffered_rdonly_slurp(pulsed->fd, 0);
              if (pulsed->fd == -1) {
                // dont show gui errors - we are running in realtime thread
                LIVES_ERROR("pulsed: error opening");
                LIVES_ERROR(filename);
                pulsed->playing_file = -1;
              }
              lives_free(filename);
            }
          }
          fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = 0;
          pulsed->playing_file = new_file;
          //pa_stream_trigger(pulsed->pstream, NULL, NULL); // only needed for prebuffer

          /* pulsed->in_use = TRUE; */
          /* paop = pa_stream_flush(pulsed->pstream, NULL, NULL); */
          /* pa_operation_unref(paop); */

          /* xseek = ALIGN_CEIL64(afile->aseek_pos, afile->achans * (afile->asampsize >> 3)); */
          /* lives_lseek_buffered_rdonly_absolute(pulsed->fd, xseek); */
          /* fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = afile->aseek_pos = xseek; */
          /* if (msg->extra) { */
          /*   double ratio = lives_strtod(msg->extra); */
          /*   pulse_set_avel(pulsed, pulsed->playing_file, ratio); */
          /* } */
          /* if (pulsed->playing_file == mainw->ascrap_file || afile->adirection == LIVES_DIRECTION_FORWARD) { */
          /*   lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE); */
          /* } else { */
          /*   lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE); */
          /* } */
          /* pulsed->playing_file = new_file; */
          /* fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = afile->aseek_pos; */
          /* //pa_stream_trigger(pulsed->pstream, NULL, NULL); // only needed for prebuffer */
          break;
        }
      case ASERVER_CMD_FILE_CLOSE:
        got_cmd = TRUE;
        paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
        pa_operation_unref(paop);
        if (pulsed->fd >= 0) {
          if (LIVES_IS_PLAYING && IS_VALID_CLIP(pulsed->playing_file)) {
            afile->aplay_fd = pulsed->fd;
            //if (!mainw->audio_seek_ready) afile->sync_delta = mainw->startticks - mainw->currticks;
          } else lives_close_buffered(pulsed->fd);
        }
        if (pulsed->sound_buffer == pulsed->aPlayPtr->data) pulsed->sound_buffer = NULL;
        lives_freep((void **)&pulsed->aPlayPtr->data);
        pulsed->aPlayPtr->max_size = pulsed->aPlayPtr->size = 0;
        pulsed->fd = pulsed->playing_file = -1;
        pulsed->in_use = FALSE;
        if (cmd != (int)msg->command) cmd = ASERVER_CMD_PROCESSED;
        break;
      case ASERVER_CMD_FILE_SEEK:
      case ASERVER_CMD_FILE_SEEK_ADJUST:
        if (!IS_VALID_CLIP(pulsed->playing_file) || pulsed->fd < 0) break;
        got_cmd = TRUE;
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

        if (msg->command == ASERVER_CMD_FILE_SEEK_ADJUST) {
          ticks_t delta = lives_get_current_ticks() - msg->tc;
          xseek += (double)delta / TICKS_PER_SECOND_DBL  *
                   (double)(afile->adirection * afile->arate * afile->achans * (afile->asampsize >> 3));
        }
        if (seek < 0.) xseek = 0.;
        xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3));
        lives_lseek_buffered_rdonly_absolute(pulsed->fd, xseek);
        fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = afile->aseek_pos = xseek;
        if (msg->extra) {
          double ratio = lives_strtod(msg->extra);
          pulse_set_avel(pulsed, pulsed->playing_file, ratio);
        }
        if (pulsed->playing_file == mainw->ascrap_file || afile->adirection == LIVES_DIRECTION_FORWARD)
          lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
        else  lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
        break;
      default:
        pulsed->msgq = NULL;
        msg->data = NULL;
      }
      if (cmd == (int)msg->command) break;
    }
    if (msg->next != msg) {
      lives_freep((void **)&msg->data);
      lives_freep((void **)&msg->extra);
    }
    msg->command = ASERVER_CMD_PROCESSED;
    pulsed->msgq = msg->next;
    if (pulsed->msgq && pulsed->msgq->next == pulsed->msgq) pulsed->msgq->next = NULL;
  }
  if (got_cmd) {
    sample_silence_pulse(pulsed, nbytes);
    in_ap = FALSE;
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return;
  }

  /// this is the value we will return from pulse_get_rec_avals
  fwd_seek_pos = pulsed->real_seek_pos;

  if (1) {
    int16_t *fbdata = NULL;
    uint64_t pulseFramesAvailable = nsamples;
    uint64_t inputFramesAvailable = 0;
    uint64_t numFramesToWrite = 0;
    double in_framesd = 0.;
    float clip_vol = 1.;
    size_t in_bytes = 0, xin_bytes = 0;
    /** the ratio of samples in : samples out - may be negative. This is NOT the same as the velocity as it incluides a resampling factor */
    float shrink_factor = 1.f;
    int swap_sign;
    int qnt = 1;
    boolean alock_mixer = FALSE;

    if (!mainw->audio_seek_ready) {
      audio_sync_ready();
    }

    if (!mainw->video_seek_ready) {
      // while waiting for video seek, we play silence, and the clock is advancing
      // however, the video player will add this extra time to its sync_delta
      sample_silence_pulse(pulsed, -nbytes);
      in_ap = FALSE;
      lives_proc_thread_include_states(self, THRD_STATE_IDLING);
      lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
      return;
    }

    if (IS_VALID_CLIP(pulsed->playing_file)) qnt = afile->achans * (afile->asampsize >> 3);

#ifdef DEBUG_PULSE
    lives_printerr("playing... pulseFramesAvailable = %ld\n", pulseFramesAvailable);
#endif

    pulsed->num_calls++;

    if (future_prefs->volume * clip_vol != pulsed->volume_linear) {
      // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +) ??
      pa_operation *paop;
      pulsed->volume_linear = future_prefs->volume * clip_vol;
      pavol = pa_sw_volume_from_linear(pulsed->volume_linear);
      pa_cvolume_set(&pulsed->volume, pulsed->out_achans, pavol);
      paop = pa_context_set_sink_input_volume(pulsed->con,
                                              pa_stream_get_index(pulsed->pstream), &pulsed->volume, NULL, NULL);
      pa_operation_unref(paop);
    }

    if (mainw->alock_abuf && mainw->alock_abuf->is_ready) {
      if (mainw->alock_abuf->_fd != -1) {
        if (!lives_buffered_rdonly_is_slurping(mainw->alock_abuf->_fd)) {
          lives_close_buffered(mainw->alock_abuf->_fd);
          mainw->alock_abuf->_fd = -1;
        }
      }
      if (mainw->alock_abuf->_fd == -1 && mainw->alock_abuf->fileno != pulsed->playing_file)
        alock_mixer = TRUE;
    }

    /// not in use, just send silence
    if (!pulsed->in_use || (pulsed->read_abuf > -1 && !LIVES_IS_PLAYING)
        || ((((pulsed->fd < 0 || pulsed->seek_pos < 0 ||
               (!mainw->multitrack && IS_VALID_CLIP(pulsed->playing_file) && pulsed->seek_pos > afile->afilesize))
              && pulsed->read_abuf < 0) && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack))
            || pulsed->is_paused || (mainw->pulsed_read && mainw->pulsed_read->playing_file != -1))) {
      if (pulsed->seek_pos < 0 && IS_VALID_CLIP(pulsed->playing_file) && pulsed->in_arate > 0) {
        pulsed->seek_pos += (double)pulsed->in_arate / (double)pulsed->out_arate * nsamples * qnt;
        pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);

        if (pulsed->seek_pos > 0) {
          pad_bytes = -pulsed->real_seek_pos;
          pulsed->real_seek_pos = pulsed->seek_pos = 0;
        }
      }

      if (IS_VALID_CLIP(pulsed->playing_file) && !mainw->multitrack
          && pulsed->seek_pos > afile->afilesize && pulsed->in_arate < 0) {
        pulsed->seek_pos += ((double)pulsed->in_arate / (double)pulsed->out_arate) * nsamples * qnt;
        pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos - qnt + 1, qnt);
        if (pulsed->seek_pos < afile->afilesize) {
          pad_bytes = (afile->afilesize - pulsed->real_seek_pos); // -ve val
          pulsed->real_seek_pos = pulsed->seek_pos = afile->afilesize;
        }
      }
      if (!pad_bytes) {
        if (!alock_mixer) {
          sample_silence_pulse(pulsed, -nbytes);
          in_ap = FALSE;
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          return;
        }
      }
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
#if HAVE_PA_STREAM_BEGIN_WRITE
        xbytes = -1;
        ret = pa_stream_begin_write(pulsed->pstream, (void **)&pulsed->sound_buffer, &xbytes);
        if (ret) {
          sample_silence_pulse(pulsed, nbytes);
          in_ap = FALSE;
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          return;
        }
        if (xbytes < nbytes) {
          nbytes = xbytes;
          pulseFramesAvailable = nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
        }
#endif
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
          in_bytes = (size_t)(in_framesd + fastrand_dbl(1.)) * pulsed->in_achans * (pulsed->in_asamps >> 3);
          xin_bytes = (size_t)in_framesd * pulsed->in_achans * (pulsed->in_asamps >> 3);

#ifdef DEBUG_PULSE
          g_print("in bytes=%ld %d %d %lu %d %lu\n", in_bytes, pulsed->in_arate, pulsed->out_arate, pulseFramesAvailable,
                  pulsed->in_achans, pulsed->in_asamps);
#endif

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
          }

          pulsed->aPlayPtr->size = 0;

          /// expand the buffer if necessary
          if (LIVES_UNLIKELY((in_bytes > pulsed->aPlayPtr->max_size && !(*pulsed->cancelled) && fabsf(shrink_factor)
                              <= 100.f))) {
            boolean update_sbuffer = FALSE;
            if (pulsed->sound_buffer == pulsed->aPlayPtr->data) update_sbuffer = TRUE;
            lives_freep((void **)(&pulsed->aPlayPtr->data));
            pulsed->aPlayPtr->data = lives_calloc_safety(in_bytes >> 2, 4);
            pulsed->aPlayPtr->size = 0;
            if (update_sbuffer) pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data);
            if (pulsed->aPlayPtr->data) pulsed->aPlayPtr->max_size = in_bytes;
            else pulsed->aPlayPtr->max_size = 0;
          }

          if (shrink_factor > 0.) {
            // forward playback
            if ((mainw->agen_key == 0 || mainw->multitrack || mainw->preview) && in_bytes > 0) {
              //lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
              //pulsed->seek_pos += in_bytes - pad_bytes;
              lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
              if (pad_bytes < 0) pad_bytes = 0;
              else {
                pad_bytes *= shrink_factor;
                pad_bytes = ALIGN_CEIL64(pad_bytes - qnt, qnt);
              }
              if (!alock_mixer || !prefs->pogo_mode) {
                if (!pulsed->mute) {
                  if (!pulsed->aPlayPtr->data)
                    pulsed->aPlayPtr->data = lives_calloc_safety(in_bytes / 4 + 1, 4);
                  else if (pad_bytes) lives_memset((void *)pulsed->aPlayPtr->data, 0, pad_bytes);
                  pulsed->aPlayPtr->size = lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data + pad_bytes),
                                           (in_bytes  - pad_bytes), TRUE) + pad_bytes;
                }
              }
            } else pulsed->aPlayPtr->size = in_bytes;

#if !HAVE_PA_STREAM_BEGIN_WRITE
            pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data);
#endif
            //if (pulsed->fd > -1) pulsed->seek_pos = lives_buffered_offset(pulsed->fd);
            //else
            pulsed->seek_pos += in_bytes - pad_bytes;
            pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
            if (pulsed->seek_pos >= pulsed->seek_end && !afile->opening) {
              ssize_t rem = pulsed->seek_end - pulsed->real_seek_pos;
              if (pulsed->aPlayPtr->size + rem > in_bytes) rem = in_bytes - pulsed->aPlayPtr->size;
              if (rem > 0)
                if (!alock_mixer || !prefs->pogo_mode) {
                  pulsed->aPlayPtr->size += lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data)
                                            + pulsed->aPlayPtr->size,
                                            pulsed->seek_end - pulsed->real_seek_pos, TRUE);
                }
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
                    if (!alock_mixer || !prefs->pogo_mode) {
                      lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
                      if (pulsed->aPlayPtr->size < in_bytes) {
                        pulsed->aPlayPtr->size += lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data)
                                                  + pulsed->aPlayPtr->size, in_bytes - pulsed->aPlayPtr->size, TRUE);
                        pulsed->real_seek_pos = pulsed->seek_pos = lives_buffered_offset(pulsed->fd);
                        pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
                      }
                    }
                  } while (pulsed->aPlayPtr->size < in_bytes && !lives_read_buffered_eof(pulsed->fd));
                  if (!alock_mixer || !prefs->pogo_mode) {
                    if (pulsed->aPlayPtr->size < in_bytes) {
                      pad_bytes = in_bytes - pulsed->aPlayPtr->size;
                      /// TODO: use append_silence() for all padding
                      lives_memset((void *)pulsed->aPlayPtr->data + in_bytes - pad_bytes, 0, pad_bytes);
                    }
                    pulsed->aPlayPtr->size = in_bytes;
                  }
                }
              }
              pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos - qnt, qnt);
              fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;
              if (mainw->record && !mainw->record_paused) pulse_set_rec_avals(pulsed);
            }
          }

          else if (pulsed->playing_file != mainw->ascrap_file && shrink_factor < 0.f) {
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
                if (mainw->agen_key == 0 && !mainw->agen_needs_reinit) {
                  if (!alock_mixer || !prefs->pogo_mode) {
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
                    pulsed->aPlayPtr->size += pad_bytes;
                  } else pulsed->aPlayPtr->size = in_bytes;
                }
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
              //pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);

              if (!alock_mixer || !prefs->pogo_mode) {
                if (pulsed->playing_file == mainw->ascrap_file || pulsed->in_arate > 0) {
                  lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
                  lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE);
                } else {
                  lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE);
                  if (pulsed->aPlayPtr->size < in_bytes) {

                    //g_print("SEEKING NOW %ld\n", pulsed->seek_pos);
                    // here seek to pulsed->seek_pos
                    pulsed->real_seek_pos = pulsed->seek_pos = ALIGN_CEIL64(pulsed->seek_pos, qnt);
                    if (!alock_mixer || !prefs->pogo_mode) {
                      lives_lseek_buffered_rdonly_absolute(pulsed->fd, pulsed->seek_pos);
                      pulsed->aPlayPtr->size
                      += lives_read_buffered(pulsed->fd, (void *)(pulsed->aPlayPtr->data)
                                             + pulsed->aPlayPtr->size,
                                             in_bytes - pulsed->aPlayPtr->size, TRUE);
		      // *INDENT-OFF*
		    }}}}
	      else pulsed->aPlayPtr->size = in_bytes;
	    }}
	  // *INDENT-ON*

          if (pulsed->aPlayPtr->size < in_bytes) {
            /// if we are a few bytes short, pad with silence.
            // If playing fwd we pad at the end, backwards we pad the beginning
            /// (since the buffer will be reversed)

            // NOTE: this is on the Input side - shortfalls in the output side after conversion
            // are handled below
            if (pulsed->in_arate > 0) {
              append_silence(-1, (void *)pulsed->aPlayPtr->data, pulsed->aPlayPtr->size, in_bytes,
                             afile->asampsize >> 3, afile->signed_endian & AFORM_UNSIGNED,
                             afile->signed_endian & AFORM_BIG_ENDIAN);
            } else {
              lives_memmove((void *)pulsed->aPlayPtr->data + (in_bytes - pulsed->aPlayPtr->size), (void *)pulsed->aPlayPtr->data,
                            pulsed->aPlayPtr->size);
              append_silence(-1, (void *)pulsed->aPlayPtr->data, 0, in_bytes - pulsed->aPlayPtr->size,
                             afile->asampsize >> 3, afile->signed_endian & AFORM_UNSIGNED,
                             afile->signed_endian & AFORM_BIG_ENDIAN);
            }
          }
          //pulsed->aPlayPtr->size = in_bytes;
        }

        /// put silence if anything changed

        if (pulsed->mute || in_bytes == 0 || pulsed->aPlayPtr->size == 0 || !IS_VALID_CLIP(pulsed->playing_file)
            || (!pulsed->aPlayPtr->data && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) ||
                                            mainw->multitrack || mainw->preview))) {
          if (!alock_mixer) {
            sample_silence_pulse(pulsed, nbytes);
            in_ap = FALSE;
            lives_proc_thread_include_states(self, THRD_STATE_IDLING);
            lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
            return;
          }
        }

        if (mainw->agen_key != 0 && !mainw->multitrack && !mainw->preview) {
          in_bytes = pulseFramesAvailable * pulsed->out_achans * 2;
          if (xin_bytes == 0) xin_bytes = in_bytes;
        }

        if ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack || (mainw->preview &&
            !mainw->preview_rendering)) {
          /// read from file
          if (pulsed->playing_file > -1) swap_sign = afile->signed_endian & AFORM_UNSIGNED;
          else swap_sign = FALSE;

          inputFramesAvailable = pulsed->aPlayPtr->size / (pulsed->in_achans * (pulsed->in_asamps >> 3));
#ifdef DEBUG_PULSE
          lives_printerr("%ld inputFramesAvailable == %ld, %f, %d %d,pulseFramesAvailable == %lu\n", pulsed->aPlayPtr->size,
                         inputFramesAvailable,
                         in_framesd, pulsed->in_arate, pulsed->out_arate, pulseFramesAvailable);
#endif

          //// buffer is just a ref to pulsed->aPlayPtr->data
          buffer = (uint8_t *)pulsed->aPlayPtr->data;
          ////

          numFramesToWrite = (uint64_t)((double)inputFramesAvailable / (double)fabsf(shrink_factor) + .001);

#ifdef DEBUG_PULSE
          lives_printerr("inputFramesAvailable after conversion %ld\n", numFramesToWrite);
          lives_printerr("nsamples == %ld, pulseFramesAvailable == %ld,\n\tpulsed->num_input_channels == %d, "
                         "pulsed->out_achans == %d\n",
                         nsamples, pulseFramesAvailable, pulsed->in_achans, pulsed->out_achans);
#endif

          // pulsed->sound_buffer will either point to pulsed->aPlayPtr->data or will hold transformed audio
          if (pulsed->in_asamps == pulsed->out_asamps && shrink_factor == 1. && pulsed->in_achans == pulsed->out_achans &&
              !pulsed->reverse_endian && !swap_sign) {
            if (!buffer) sample_silence_pulse(pulsed, nbytes);
            else {
#if !HAVE_PA_STREAM_BEGIN_WRITE
              // no transformation needed
              pulsed->sound_buffer = buffer;
#else
              lives_memcpy(pulsed->sound_buffer, buffer, nbytes);
#endif
            }
          } else {
#if !HAVE_PA_STREAM_BEGIN_WRITE
            if (pulsed->sound_buffer != pulsed->aPlayPtr->data) {
              lives_freep((void **)&pulsed->sound_buffer);
              pulsed->aPlayPtr->max_size = pulsed->aPlayPtr->size = 0;
              pulsed->sound_buffer = (uint8_t *)lives_calloc_safety(nbytes >> 2, 4);
            }
            if (!pulsed->sound_buffer) {
              sample_silence_pulse(pulsed, nbytes);
              in_ap = FALSE;
              lives_proc_thread_include_states(self, THRD_STATE_IDLING);
              lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
              return;
            }
            pulsed->aPlayPtr->data = pulsed->sound_buffer;
            pulsed->aPlayPtr->max_size = pulsed->aPlayPtr->size = nbytes;
#endif
            if (!from_memory) {
              buffer = (uint8_t *)pulsed->aPlayPtr->data;

              /// convert 8 bit to 16 if applicable, and resample to out rate
              if (pulsed->in_asamps == 8) {
                sample_move_d8_d16((short *)(pulsed->sound_buffer), (uint8_t *)buffer, nsamples, xin_bytes,
                                   shrink_factor, pulsed->out_achans, pulsed->in_achans, swap_sign ? SWAP_U_TO_S : 0);
              } else {
                sample_move_d16_d16((short *)pulsed->sound_buffer, (short *)buffer, nsamples, xin_bytes, shrink_factor,
                                    pulsed->out_achans, pulsed->in_achans, pulsed->reverse_endian ? SWAP_X_TO_L : 0,
                                    swap_sign ? SWAP_U_TO_S : 0);
              }

              inputFramesAvailable = xin_bytes / (pulsed->in_achans * (pulsed->in_asamps >> 3));
              numFramesToWrite = (uint64_t)((double)inputFramesAvailable / (double)fabsf(shrink_factor) + .001);

#ifdef DEBUG_PULSE
              lives_printerr("inputFramesAvailable after conversion2u %ld\n", numFramesToWrite);
#endif
              if (numFramesToWrite > pulseFramesAvailable) {
#ifdef DEBUG_PULSE
                lives_printerr("dropping last %ld samples\n", numFramesToWrite - pulseFramesAvailable);
#endif
              } else if (numFramesToWrite < pulseFramesAvailable) {
                // because of rounding, occasionally we get a sample or two short. Here we duplicate the last samples
                // so as not to leave a zero filled gap
                int start = 1, len = 1;
                size_t lack = (pulseFramesAvailable - numFramesToWrite);
                lives_memcpy((short *)pulsed->sound_buffer + (numFramesToWrite) * pulsed->out_achans,
                             (short *)pulsed->sound_buffer + (numFramesToWrite - 1) * pulsed->out_achans, qnt);
                while (--lack) {
                  start++;
                  len += 2;
                  lives_memmove((short *)pulsed->sound_buffer + (numFramesToWrite - start + 1) * pulsed->out_achans,
                                (short *)pulsed->sound_buffer + (numFramesToWrite - start)
                                * pulsed->out_achans, len * qnt);
                }

#ifdef DEBUG_PULSE
                lives_printerr("duplicated last %ld samples\n", lack);
#endif
              }
            }
          }
          numFramesToWrite = pulseFramesAvailable;

          // ascyn_trigger();

        } else {
          // PULLING AUDIO FROM AN AUDIO GENERATOR
          // get float audio from gen, convert it to S16
          float **fltbuf = NULL;
          boolean pl_error = FALSE;
          numFramesToWrite = pulseFramesAvailable;

          if (mainw->agen_needs_reinit) pl_error = TRUE;
          else {
            fltbuf = (float **)lives_calloc(pulsed->out_achans, sizeof(float *));
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
              lives_freep((void **) & (pulsed->aPlayPtr->data));
              pulsed->aPlayPtr->size = pulsed->aPlayPtr->max_size = 0;
              pulsed->aPlayPtr->data = lives_calloc_safety(nbytes / 4 + 1, 4);
              //_print("realloc 2\n");
              if (update_sbuffer) pulsed->sound_buffer = (void *)(pulsed->aPlayPtr->data);
              if (pulsed->aPlayPtr->data) {
                pulsed->aPlayPtr->max_size = pulsed->aPlayPtr->size = nbytes;
              } else pl_error = TRUE;
            }
          }

          // get back non-interleaved float fbuffer; rate and channels should match
          if (pl_error) nbytes = 0;
        }

#if 0
        ////////////////////////////////////////
        // TODO - attach a callback to data_ready hook
        //////////////////////////////////////////////////////////


        if (mainw->record && !mainw->record_paused && IS_VALID_CLIP(mainw->ascrap_file) && LIVES_IS_PLAYING) {
          if (!rec_lpt) {
            dets = (arec_details *)lives_calloc(1, sizeof(arec_details));
            ;	      /// if we are recording then write generated audio to ascrap_file
            dets->clipno = mainw->ascrap_file;
            dets->fd = mainw->aud_rec_fd;
            dets->rec_samples = -1;

            rec_lpt = lives_proc_thread_add_hook_full(NULL, DATA_READY_HOOK, 0, write_aud_data_cb,
                      WEED_SEED_BOOLEAN, "pv", self, dets);
            // this ensures ana_lpt is not freed as soon as we cancel it
            lives_proc_thread_ref(rec_lpt);
          }

          /* // end from gen */
        }
#endif
        pulseFramesAvailable -= numFramesToWrite;

#ifdef DEBUG_PULSE
        lives_printerr("pulseFramesAvailable == %ld\n", pulseFramesAvailable);
#endif
      }

      // playback from memory or file
      if (pulsed->playing_file > -1 && !mainw->multitrack) clip_vol = afile->vol;
      if (future_prefs->volume * clip_vol != pulsed->volume_linear) {
        // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +) ??
        pa_operation *paop;
        pulsed->volume_linear = future_prefs->volume * clip_vol;
        pavol = pa_sw_volume_from_linear(pulsed->volume_linear);
        pa_cvolume_set(&pulsed->volume, pulsed->out_achans, pavol);

        g_print("set vol to %f %f %f %u\n", future_prefs->volume, clip_vol,
                pulsed->volume_linear, pavol);

        paop = pa_context_set_sink_input_volume(pulsed->con,
                                                pa_stream_get_index(pulsed->pstream), &pulsed->volume, NULL, NULL);
        g_print("2set vol to %f %f %f %u\n", future_prefs->volume, clip_vol,
                pulsed->volume_linear, pavol);
        pa_operation_unref(paop);
      }
    }

    // buffer is reused here, it's what we'll actually push to pulse

    if (!from_memory) {
#if !HAVE_PA_STREAM_BEGIN_WRITE
      if (nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3) <= numFramesToWrite) {
        buffer = pulsed->sound_buffer;
      }
#else
      if (0) {
        // do nothing
      }
#endif
      else {
#if !HAVE_PA_STREAM_BEGIN_WRITE
        buffer = (uint8_t *)lives_calloc(1, nbytes);
        //}
        if (!buffer) {
          sample_silence_pulse(pulsed, nbytes);
          in_ap = FALSE;
          if (fltbuf) {
            for (int i = 0; i < pulsed->out_achans; i++) lives_freep((void **)&fltbuf[i]);
            lives_free(fltbuf);
            fltbuf = NULL;
          }
          in_ap = FALSE;
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          return;
        }
#endif
      }
#if !HAVE_PA_STREAM_BEGIN_WRITE
      lives_memcpy(buffer, pulsed->sound_buffer, nbytes);
      needs_free = TRUE;
#endif
    }

#if HAVE_PA_STREAM_BEGIN_WRITE
    buffer = pulsed->sound_buffer;
#endif

    ///
    /// convert to float if necessary - e.g have fx. mixers,

    // send to fx as float


    // convert back to s16, but keep float

    // s16 -> pulse







    if (!pulsed->is_corked) {
#if 0
      if (alock_mixer) {
        boolean had_fltbuf = FALSE;
        if (fltbuf) had_fltbuf = TRUE;
        if (nsamples && (had_fltbuf || !pthread_mutex_trylock(&mainw->alock_mutex))) {
          float xshrink_factor = 1.;
          int64_t xin_framesd;
          size_t xxin_bytes;

          // if we had effected audio from earlier we can use that, otherwise pull from bufferf
          if (!had_fltbuf) {
            off_t offs = mainw->alock_abuf->seek / (mainw->alock_abuf->in_achans
                                                    * (mainw->alock_abuf->in_asamps >> 3));
            xshrink_factor = (float)mainw->alock_abuf->arate / (float)pulsed->out_arate / mainw->audio_stretch;
            fltbuf = lives_calloc(pulsed->out_achans, sizeof(float *));
            if (offs + nsamples > mainw->alock_abuf->samp_space) {
              offs = mainw->alock_abuf->seek = 0;
            }
            for (int i = 0; i < pulsed->out_achans; i++) {
              if (i > mainw->alock_abuf->in_achans) break;
              fltbuf[i] = &mainw->alock_abuf->bufferf[i][offs];
            }
          }

          xin_framesd = fabs((double)xshrink_factor * (double)nsamples);
          xxin_bytes = (size_t)(xin_framesd * mainw->alock_abuf->in_achans * (mainw->alock_abuf->in_asamps >> 3));

          if (!had_fltbuf) mainw->alock_abuf->seek += xxin_bytes;

          pthread_mutex_lock(&mainw->vpp_stream_mutex);
          if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
            (*mainw->vpp->render_audio_frame_float)(fltbuf, numFramesToWrite);
          }
          pthread_mutex_unlock(&mainw->vpp_stream_mutex);

          fbdata = lives_calloc(xin_framesd, pulsed->out_achans * (PA_SAMPSIZE >> 3));

          sample_move_float_int((void *)fbdata, fltbuf, xin_framesd, xshrink_factor,
                                pulsed->out_achans, PA_SAMPSIZE, 0, (capable->hw.byte_order == LIVES_LITTLE_ENDIAN),
                                FALSE, 1.0);

          if (had_fltbuf) {
            for (int i = 0; i < pulsed->out_achans; i++) {
              if (fltbuf[i]) lives_free(fltbuf[i]);
            }
          }
          lives_freep((void **)&fltbuf);
          if (!had_fltbuf) pthread_mutex_unlock(&mainw->alock_mutex);
        }
      }
#endif

#if 0
      if (fbdata) {
        for (int zz = 0; zz < nbytes / 2; zz++) {
          if (prefs->pogo_mode)
            ((int16_t *)(buffer))[zz] = ((int16_t *)buffer)[zz] / 2 + fbdata[zz] / 2;
          else
            ((int16_t *)(buffer))[zz] = fbdata[zz];
        }
        lives_free(fbdata);
        fbdata = NULL;
      }

      if (mainw->alock_abuf && mainw->alock_abuf->is_ready && mainw->alock_abuf->_fd == -1) {
        if (mainw->record && !mainw->record_paused && sfile && LIVES_IS_PLAYING) {
          /// if we are recording then write mixed audio to ascrap_file
          rec_output = TRUE;
        }
      } else if (get_primary_src_type(sfile) == LIVES_SRC_TYPE_RECORDER) {
        // here we are recording for the desktop grabber
        rec_output = TRUE;
      }
#endif
#if 0
      if (rec_output) {
        if (bbsize < nbytes) {
          if (back_buff) lives_free(back_buff);
          back_buff = lives_malloc(nbytes);
          bbsize = nbytes;
        }
      }
#endif

      lives_aplayer_set_data_len(self, nsamples);
      lives_aplayer_set_data(self, (void *)buffer);

      /// todo = call data ready cbs

      ///

      ///

      ///


      /// Finally... we actually write to pulse buffers
#if !HAVE_PA_STREAM_BEGIN_WRITE
      if (!pulsed->is_corked) {
        async_writer_count = lives_hooks_trigger_async(NULL, DATA_READY_HOOK);
        pa_stream_write(pulsed->pstream, buffer, nbytes, buffer == pulsed->aPlayPtr->data ? NULL :
                        pulse_buff_free, 0, PA_SEEK_RELATIVE);
      } else pulse_buff_free(pulsed->aPlayPtr->data);
#else
      buffer = NULL;
      if (!pulsed->is_corked) {
#ifdef DEBUG_PULSE
        g_print("writing %ld bytes to pulse\n", nbytes);
#endif
        async_writer_count = lives_hooks_trigger_async(NULL, DATA_READY_HOOK);
        pa_stream_write(pulsed->pstream, pulsed->sound_buffer, nbytes, NULL, 0, PA_SEEK_RELATIVE);
      }
#endif
    } else {
      // from memory (e,g multitrack)
      if (pulsed->read_abuf > -1 && !pulsed->mute) {
#if HAVE_PA_STREAM_BEGIN_WRITE
        xbytes = -1;
        ret = pa_stream_begin_write(pulsed->pstream, (void **)&shortbuffer, &xbytes);
        if (xbytes < nbytes) nbytes = xbytes;
#else
        shortbuffer = (short *)lives_calloc(nbytes, 2);
#endif
        if (ret || !shortbuffer) {
          sample_silence_pulse(pulsed, nbytes);
          in_ap = FALSE;
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          if (fbdata) lives_free(fbdata);
          return;
        }

        // convert float -> s16
        sample_move_abuf_int16(shortbuffer, pulsed->out_achans, (nbytes >> 1) / pulsed->out_achans, pulsed->out_arate);
        if (pulsed->astream_fd != -1) audio_stream(shortbuffer, nbytes, pulsed->astream_fd);

#if !HAVE_PA_STREAM_BEGIN_WRITE
        if (!pulsed->is_corked) {
          async_writer_count = lives_hooks_trigger_async(NULL, DATA_READY_HOOK);
          pa_stream_write(pulsed->pstream, shortbuffer, nbytes, pulse_buff_free, 0, PA_SEEK_RELATIVE);
        } else pulse_buff_free(shortbuffer);
#else
        if (!pulsed->is_corked) {
          async_writer_count = lives_hooks_trigger_async(NULL, DATA_READY_HOOK);
          pa_stream_write(pulsed->pstream, shortbuffer, nbytes, NULL, 0, PA_SEEK_RELATIVE);
        }
#endif
        pulsed->frames_written += nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
      } else {
        sample_silence_pulse(pulsed, nbytes);
      }
      pulseFramesAvailable = 0;
    }
    if (needs_free && pulsed->sound_buffer != pulsed->aPlayPtr->data && pulsed->sound_buffer) {
      lives_freep((void **)&pulsed->sound_buffer);
    }

    fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos;

    if (pulseFramesAvailable) {
      //    #define DEBUG_PULSE
#ifdef DEBUG_PULSE
      lives_printerr("buffer underrun of %ld frames\n", pulseFramesAvailable);
#endif
      sample_silence_pulse(pulsed, pulseFramesAvailable * pulsed->out_achans * (pulsed->out_asamps >> 3));
      if (!pulsed->is_paused) pulsed->frames_written += pulseFramesAvailable;
    }
    if (LIVES_IS_PLAYING) afile->aseek_pos = pulsed->seek_pos;
  }

#ifdef DEBUG_PULSE
  lives_printerr("done\n");
#endif
}


static void pulse_audio_read_process(pa_stream * pstream, size_t nbytes, void *arg) {
  // read nsamples from pulse buffer, and then possibly write to mainw->aud_rec_fd
  // this is the callback from pulse when we are recording or playing external audio
  static lives_thread_data_t *tdata = NULL;
  static void *back_buff = NULL;
  static size_t bbsize = 0;
  static int async_reader_count = 0;

  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  void *data;
  size_t rbytes = nbytes, zbytes, nframes;;
  lives_proc_thread_t self = pulsed->inst;

  if (!tdata) {
    tdata = get_thread_data();
    lives_thread_set_active(self);
    lives_snprintf(tdata->vars.var_origin, 128, "%s", "Pulseaudio Writer Thread");
    lives_proc_thread_include_states(self, THRD_STATE_EXTERN);
    tdata->vars.var_thrd_type = tdata->thrd_type = THRD_TYPE_AUDIO_WRITER;
  }

  if (async_reader_count) {
    // here we make sure that the DATA_READY hook callbacks have all completed
    // we must do this before we can free the data from the previous cycle
    lives_hooks_async_join(NULL, DATA_READY_HOOK);
    async_reader_count = 0;
  }

  lives_aplayer_set_data_len(self, 0);
  lives_aplayer_set_data(self, NULL);

  if (pulsed->is_corked) {
    lives_proc_thread_include_states(self, THRD_STATE_IDLING | THRD_STATE_BLOCKED);
    return;
  }

  lives_proc_thread_include_states(self, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(self, THRD_STATE_IDLING | THRD_STATE_BLOCKED);

  pulsed->pstream = pstream;

  if (!pulsed->in_use || (mainw->playing_file < 0 && AUD_SRC_EXTERNAL) || mainw->effects_paused) {
    pa_stream_peek(pulsed->pstream, (const void **)&data, &rbytes);
    if (rbytes > 0) {
      //g_print("PVAL %d\n", (*(uint8_t *)data & 0x80) >> 7);
      pa_stream_drop(pulsed->pstream);
    }

    nframes = rbytes / pulsed->in_achans / (pulsed->in_asamps >> 3);

    pthread_mutex_lock(&xtra_mutex);
    if (pulsed->in_use)
      pulsed->extrausec += ((double)nframes / (double)pulsed->in_arate * ONE_MILLION_DBL + .5);
    pthread_mutex_unlock(&xtra_mutex);
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return;
  }

  zbytes = pa_stream_readable_size(pulsed->pstream);

  if (zbytes == 0) {
    //g_print("nothing to read from PA\n");
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return;
  }

  if (pa_stream_peek(pulsed->pstream, (const void **)&data, &rbytes)) {
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return;
  }

  if (!data) {
    if (rbytes > 0) pa_stream_drop(pulsed->pstream);
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return;
  }

  nframes = rbytes / pulsed->in_achans / (pulsed->in_asamps >> 3);

  if (mainw->ext_audio_mon && !mainw->fs && !mainw->faded && !mainw->multitrack)
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_mon), (*(uint8_t *)data & 0x80) >> 7);

  // time interpolation
  pthread_mutex_lock(&xtra_mutex);
  pulsed->extrausec += ((double)nframes / (double)pulsed->in_arate * ONE_MILLION_DBL + .5);
  pthread_mutex_unlock(&xtra_mutex);

  // should really be frames_read here
  if (!pulsed->is_paused) {
    pulsed->frames_written += nframes;
  }

  // the DATA_READY_HOOKs are run as async_callbacks, so there is zero blocking here !
  // however we must ensure that back_buff is not freed until the next cycle has called asyn_hooks_join()

  if (bbsize < rbytes) {
    if (back_buff) lives_free(back_buff);
    back_buff = lives_malloc(rbytes);
    bbsize = rbytes;
  }

  lives_memcpy(back_buff, data, rbytes);
  lives_aplayer_set_data_len(self, nframes);
  lives_aplayer_set_data(self, (void *)back_buff);

  async_reader_count = lives_hooks_trigger_async(NULL, DATA_READY_HOOK);

  pulsed->seek_pos += rbytes;

  pa_stream_drop(pulsed->pstream);

  if (pulsed->playing_file == -1 || (mainw->record && mainw->record_paused) || pulsed->is_paused) {
    if (pulsed->is_paused) {
      // This is NECESSARY to reduce / eliminate huge latencies.
      // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +)
      pa_operation *paop = pa_stream_flush(pulsed->pstream, NULL,
                                           NULL); // if not recording, flush the rest of audio (to reduce latency)
      pa_operation_unref(paop);
    }
  }

  if (mainw->rec_samples == 0 && mainw->cancelled == CANCEL_NONE) {
    mainw->cancelled = CANCEL_KEEP; // we wrote the required #
  }
  lives_proc_thread_include_states(self, THRD_STATE_IDLING);
  lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
}


void pulse_shutdown(void) {
  //g_print("pa shutdown\n");
  if (pcon) {
    //g_print("pa shutdown2\n");
    pa_context_disconnect(pcon);
    pa_context_unref(pcon);
  }
  if (pa_mloop) {
    lives_sleep_while_true(in_ap);
    pa_threaded_mainloop_stop(pa_mloop);
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
  pulsed.volume_linear = -1.;
  pulsed.state = (pa_stream_state_t)PA_STREAM_UNCONNECTED;
  pulsed.in_arate = 48000;
  pulsed.fd = -1;
  pulsed.seek_pos = pulsed.seek_end = pulsed.real_seek_pos = 0;
  pulsed.msgq = NULL;
  pulsed.num_calls = 0;
  pulsed.astream_fd = -1;
  pulsed.abs_maxvol_heard = 0.;
  pulsed.pulsed_died = FALSE;
  pulsed.aPlayPtr = (audio_buffer_t *)lives_calloc(1, sizeof(audio_buffer_t));
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
  pa_sample_spec pa_spec;
  pa_channel_map pa_map;
  pa_buffer_attr pa_battr;

  pa_operation *pa_op;

  lives_rfx_t *rfx;

  char *pa_clientname;
  char *mypid;
  char *desc;

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
    pa_battr.tlength = LIVES_PA_BUFF_TARGET >> 2;
    pa_battr.minreq = LIVES_PA_BUFF_MINREQ >> 1;
    pa_battr.prebuf = 0;  /// must set this to zero else we hang, since pa is waiting for the buffer to be filled first
  } else {
    pa_battr.maxlength = LIVES_PA_BUFF_MAXLEN * 2;
    pa_battr.fragsize = LIVES_PA_BUFF_FRAGSIZE * 4;
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

  if (!pdriver->inst) pdriver->inst = lives_player_inst_create(PLAYER_SUBTYPE_AUDIO);
  mainw->aplayer = pdriver->inst;

  lives_aplayer_set_float(pdriver->inst, FALSE);
  lives_aplayer_set_interleaved(pdriver->inst, TRUE);
  lives_aplayer_set_signed(pdriver->inst, TRUE);

  if (pdriver->is_output) {
    pa_volume_t pavol;
    pdriver->is_corked = TRUE;

    lives_aplayer_set_source(pdriver->inst, AUDIO_SRC_INT);
    lives_aplayer_set_achans(pdriver->inst, pdriver->out_achans);
    lives_aplayer_set_arate(pdriver->inst, pdriver->out_arate);
    lives_aplayer_set_sampsize(pdriver->inst, pdriver->out_asamps);

    // set write callback
    pa_stream_set_write_callback(pdriver->pstream, pulse_audio_write_process, pdriver);

    /* pa_stream_set_underflow_callback(pdriver->pstream, stream_underflow_callback, pdriver); */
    /* pa_stream_set_overflow_callback(pdriver->pstream, stream_overflow_callback, pdriver); */
    /* pa_stream_set_moved_callback(pdriver->pstream, stream_moved_callback, pdriver); */
    /* pa_stream_set_buffer_attr_callback(pdriver->pstream, stream_buffer_attr_callback, pdriver); */

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
    pa_stream_connect_playback(pdriver->pstream, NULL,
                               &pa_battr, (pa_stream_flags_t)
                               (0
                                | PA_STREAM_RELATIVE_VOLUME
                                | PA_STREAM_START_CORKED
                                | PA_STREAM_INTERPOLATE_TIMING
                                | PA_STREAM_START_UNMUTED
                                | PA_STREAM_AUTO_TIMING_UPDATE),
                               &pdriver->volume, NULL);
#endif
    pa_mloop_unlock();

    lives_millisleep_while_false(pa_stream_get_state(pdriver->pstream) == PA_STREAM_READY);

#if PA_SW_CONNECTION
    // get the volume from the server

    pa_op = pa_context_get_sink_info(pdriver->con, info_cb, &pdriver);

    while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
      pa_threaded_mainloop_wait(pa_mloop);
    }

    pa_operation_unref(pa_op);
#endif
    pdriver->volume_linear = -1.;
  } else {
    // set read callback
    pdriver->frames_written = 0;
    pdriver->usec_start = 0;
    pdriver->in_use = FALSE;
    pdriver->abs_maxvol_heard = 0.;
    pdriver->is_corked = TRUE;

    lives_aplayer_set_source(pdriver->inst, AUDIO_SRC_EXT);
    lives_aplayer_set_achans(pdriver->inst, pdriver->in_achans);
    lives_aplayer_set_arate(pdriver->inst, pdriver->in_arate);
    lives_aplayer_set_sampsize(pdriver->inst, pdriver->in_asamps);

    pa_stream_set_underflow_callback(pdriver->pstream, stream_underflow_callback, pdriver);
    pa_stream_set_overflow_callback(pdriver->pstream, stream_overflow_callback, pdriver);

    pa_stream_set_moved_callback(pdriver->pstream, stream_moved_callback, pdriver);
    pa_stream_set_buffer_attr_callback(pdriver->pstream, stream_buffer_attr_callback, pdriver);
    pa_stream_set_read_callback(pdriver->pstream, pulse_audio_read_process, pdriver);

    pa_stream_connect_record(pdriver->pstream, NULL, &pa_battr,
                             (pa_stream_flags_t)(PA_STREAM_START_CORKED
                                 | PA_STREAM_INTERPOLATE_TIMING
                                 | PA_STREAM_AUTO_TIMING_UPDATE
                                 | PA_STREAM_NOT_MONOTONIC));

    pa_mloop_unlock();
    lives_millisleep_while_false(pa_stream_get_state(pdriver->pstream) == PA_STREAM_READY);
  }

  rfx = obj_attrs_to_rfx(pdriver->inst, TRUE);
  if (pdriver->is_output) {
    desc = _("Pulse audio player details");
  } else {
    desc = _("Pulse audio reader details");
  }
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup_printf("layout|\"%s\"|", desc));
  lives_free(desc);
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p0|")); // source
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p1|\"(from PA server)\"")); // rate
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p2|")); // channels
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p3|")); // sampsize
  // skip status (uninteresting)
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p5|p6|")); // signed / endian

  pdriver->interface = rfx;

  if (pdriver->is_output) {
    lives_widget_set_sensitive(mainw->show_aplayer_attr, TRUE);
  } else {
    lives_widget_set_sensitive(mainw->show_aplayer_read_attr, TRUE);
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
  //while (!await_audio_queue(LIVES_DEFAULT_TIMEOUT));
}


void pulse_driver_cork(pulse_driver_t *pdriver) {
  pa_operation *paop;
  ticks_t timeout;
  lives_alarm_t alarm_handle;
  if (pdriver->is_corked) {
    //g_print("IS CORKED\n");
    return;
  }

  while (!await_audio_queue(LIVES_DEFAULT_TIMEOUT));

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
static int64_t last_usec = -1;
static int64_t last_retval = 0;
static double sclf = 1.;


LIVES_GLOBAL_INLINE pulse_driver_t *pulse_get_driver(boolean is_output) {
  if (is_output) return &pulsed;
  return &pulsed_reader;
}


LIVES_GLOBAL_INLINE volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *pulsed) {
  if (pulsed->pulsed_died || mainw->aplayer_broken) return NULL;
  return pulsed->msgq;
}


boolean pa_time_reset(pulse_driver_t *pulsed, ticks_t offset) {
  pa_operation *pa_op;
  int64_t usec;

  if (!pulsed->pstream) return FALSE;

  pa_mloop_lock();
  pa_op = pa_stream_update_timing_info(pulsed->pstream, pulse_success_cb, pa_mloop);

  lives_millisleep_until_nonzero_timeout(MILLIONS(50),
                                         pa_operation_get_state(pa_op) != PA_OPERATION_RUNNING);

  pa_operation_unref(pa_op);
  pa_mloop_unlock();

  lives_millisleep_while_true(pa_stream_get_time(pulsed->pstream, (pa_usec_t *)&usec) < 0);

  pthread_mutex_lock(&xtra_mutex);
  pulsed->extrausec = 0;
  pthread_mutex_unlock(&xtra_mutex);
  last_retval = 0;
  sclf = 1.;
  last_usec = pulsed->usec_start = usec - offset  / USEC_TO_TICKS;
  pulsed->frames_written = 0;
  return TRUE;
}


/**
   @brief calculate the playback time based on samples sent to the soundcard
*/
ticks_t lives_pulse_get_time(pulse_driver_t *pulsed) {
  // get the time in ticks since playback started
  // we try first to get tie from pa_stream_get_time()
  // if this fails then increment last time with nframes written | read / samplerate (extrausec)
  // otherwise we will return same value as previous
  // the clock reader is smart enough to be able to interpolate between equal values
  //
  // the value returned mut be monotonic after resset. Thus when we get a new reading we can only reduce extrausec by
  // (new_reading / old_reading). However we will keep track of the ratio extrausec / measured_usec, and we will
  // then start dividing extrausec by this value (clamped between 0.5 <= scale <= 2.0(

  int64_t usec;
  ticks_t retval = -1;
  volatile aserver_message_t *msg = pulsed->msgq;

  if (msg && (msg->command == ASERVER_CMD_FILE_SEEK || msg->command == ASERVER_CMD_FILE_OPEN)) {
    // if called from audio thread, then we have nothing else to clear the message queue
    // so just return the most recent value
    if (THREADVAR(fx_is_audio)) return mainw->currticks;
    if (await_audio_queue(BILLIONS(2)) != LIVES_RESULT_SUCCESS) return -1;
  }
  for (int z = 0; z < 5000; z++) {
    if (!(pa_stream_get_time(pulsed->pstream, (pa_usec_t *)&usec) < 0 || usec == 0)) {
      retval = 0;
      break;
    }
    lives_microsleep;
  }

  if (!retval) {
    if (usec > last_usec) {
      last_usec = usec;
      pthread_mutex_lock(&xtra_mutex);
      if (pulsed->extrausec) {
        sclf = (double)pulsed->extrausec / (double)(usec - pulsed->usec_start);
        //g_print("ratio %.4f\n", sclf);
        if (sclf > 1.2) sclf = 1.2;
        if (sclf < 0.8) sclf = 0.8;
      }
      pthread_mutex_unlock(&xtra_mutex);
    }
  }

  retval = (ticks_t)(usec - pulsed->usec_start) * USEC_TO_TICKS;
  if (retval < last_retval) retval = last_retval;

  if (retval < 0) retval = 0;
  else last_retval = retval;

  return retval;
}


double lives_pulse_get_timing_ratio(pulse_driver_t *pulsed) {return sclf;}


off_t lives_pulse_get_offset(pulse_driver_t *pulsed) {
  // get current time position (seconds) in audio file
  //return (double)pulsed->real_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
  if (pulsed->playing_file > -1) return pulsed->seek_pos;
  return -1;
}


double lives_pulse_get_pos(pulse_driver_t *pulsed) {
  // get current time position (seconds) in audio file
  //return (double)pulsed->real_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
  if (pulsed->playing_file > -1) {
    return (double)pulsed->seek_pos
           / (double)(afile->arate * afile->achans * afile->asampsize / 8);
  }
  // from memory
  return (double)pulsed->frames_written / (double)pulsed->out_arate;
}


boolean pulse_audio_seek_frame_velocity(pulse_driver_t *pulsed, double frame, double vel) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample
  off_t seekstart;

  if (!IS_VALID_CLIP(pulsed->playing_file)) return FALSE;

  if (frame > afile->frames && afile->frames > 0) frame = afile->frames;

  seekstart = (off_t)(((frame - 1.) / afile->fps
                       + (LIVES_IS_PLAYING ? (double)(mainw->currticks - mainw->startticks) / TICKS_PER_SECOND_DBL
                          * LIVES_DIRECTION_SIG(pulsed->in_arate) : 0.)) * (double)afile->arate)
              * afile->achans * afile->asampsize / 8;

  /* g_print("vals %ld and %ld %d\n", mainw->currticks, mainw->startticks, afile->arate); */
  /* g_print("bytes %f     %f       %d        %ld          %f\n", frame, afile->fps, LIVES_IS_PLAYING, seekstart, */
  /*         (double)seekstart / (double)afile->arate / 4.); */
  pulse_audio_seek_bytes_velocity(pulsed, seekstart, afile, vel);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean pulse_audio_seek_frame(pulse_driver_t *pulsed, double frame) {
  return pulse_audio_seek_frame_velocity(pulsed, frame, 0.);
}


off_t pulse_audio_seek_bytes_velocity(pulse_driver_t *pulsed, off_t bytes, lives_clip_t *sfile, double vel) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file
  off_t seekstart;

  // set this here so so that pulse_get_rev_avals returns the forward seek position
  if (pulsed->is_corked) pulse_driver_uncork(pulsed);

  if (!pulsed->is_corked) {
    if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT * 10) || pulsed->playing_file == -1) {
      if (pulsed->playing_file > -1) LIVES_WARN("Pulse connect timed out");
      seek_err = TRUE;
      return 0;
    }
  }

  if (bytes < 0) bytes = 0;
  if (bytes > sfile->afilesize) bytes = sfile->afilesize;

  seekstart = ((off_t)(bytes / sfile->achans / (sfile->asampsize / 8))) * sfile->achans * (sfile->asampsize / 8);

  if (seekstart < 0) seekstart = 0;
  if (seekstart > sfile->afilesize) seekstart = sfile->afilesize;

  fwd_seek_pos = seekstart;

  pulse_message2.command = ASERVER_CMD_FILE_SEEK;

  if (0 && LIVES_IS_PLAYING && !mainw->preview) {
    pulse_message2.tc = lives_get_current_ticks();
    pulse_message2.command = ASERVER_CMD_FILE_SEEK_ADJUST;
  }
  pulse_message2.next = NULL;
  pulse_message2.data = lives_strdup_printf("%"PRId64, seekstart);
  pulse_message2.tc = 0;
  if (vel !=  0.) pulse_message2.extra = lives_strdup_printf("%f", vel);
  else lives_freep((void **)&pulse_message2.extra);
  if (!pulsed->msgq) pulsed->msgq = &pulse_message2;
  else pulsed->msgq->next = &pulse_message2;

  return seekstart;
}


LIVES_GLOBAL_INLINE off_t pulse_audio_seek_bytes(pulse_driver_t *pulsed, int64_t bytes, lives_clip_t *sfile) {
  return pulse_audio_seek_bytes_velocity(pulsed, bytes, sfile, 0.);
}


boolean pulse_try_reconnect(void) {
  do_threaded_dialog(_("Resetting pulseaudio connection..."), FALSE);

  pulse_shutdown();
  mainw->pulsed = NULL;
  if (prefs->pa_restart && !prefs->vj_mode) {
    char *com = lives_strdup_printf("%s %s", EXEC_PULSEAUDIO, prefs->pa_start_opts);
    lives_system(com, TRUE);
    lives_free(com);
  } else lives_system("pulseaudio -k", TRUE);
  _lives_millisleep(5000);
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
void pulse_aud_pb_ready(pulse_driver_t *pulsed, int fileno) {
  char *tmpfilename = NULL;
  lives_clip_t *sfile;

  if (!pulsed || !IS_VALID_CLIP(fileno)) return;
  lives_freep((void **)&pulsed->aPlayPtr->data);
  pulsed->aPlayPtr->size = pulsed->aPlayPtr->max_size = 0;

  avsync_force();

  // hmmm
  if (pulsed && pulsed->is_corked) pulse_driver_uncork(pulsed);

  sfile = mainw->files[fileno];

  if ((!mainw->multitrack || mainw->multitrack->is_rendering) &&
      (!mainw->event_list || mainw->record || (mainw->preview && mainw->is_processing))) {
    // tell pulse server to open audio file and start playing it
    // if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) seek_err = TRUE;
    // else {
    pulse_message.command = ASERVER_CMD_FILE_OPEN;
    pulse_message.data = lives_strdup_printf("%d", fileno);
    /* pulse_message.next = NULL; */
    /* pulsed->msgq = &pulse_message; */

    if (sfile->achans > 0 && (!mainw->preview || (mainw->preview && mainw->is_processing)) &&
        (sfile->laudio_time > 0. || sfile->opening ||
         (mainw->multitrack && mainw->multitrack->is_rendering &&
          lives_file_test((tmpfilename = lives_get_audio_file_name(fileno)), LIVES_FILE_TEST_EXISTS)))) {
      pulse_message2.command = ASERVER_CMD_FILE_SEEK;
      pulse_message.next = &pulse_message2;
      pulse_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos);
      pulse_message2.next = NULL;

      mainw->pulsed->msgq = &pulse_message;

      lives_pulse_set_client_attributes(pulsed, fileno, TRUE, FALSE);
    }

    /* pulse_audio_seek_bytes(pulsed, sfile->aseek_pos, sfile); */

    //if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) seek_err = TRUE;

    /* if (seek_err) { */
    /*   seek_err = FALSE; */
    /*   if (pulse_try_reconnect()) pulse_audio_seek_bytes(pulsed, sfile->aseek_pos, sfile); */
    /* } */

    if (mainw->agen_key != 0 && !mainw->multitrack) pulsed->in_use = TRUE; // audio generator is active
    if (AUD_SRC_EXTERNAL && (prefs->audio_opts & AUDIO_OPTS_EXT_FX)) register_audio_client(FALSE);

    if ((mainw->agen_key != 0 || mainw->agen_needs_reinit)
        && !mainw->multitrack && !mainw->preview) pulsed->in_use = TRUE; // audio generator is active

    /* mainw->rec_aclip = fileno; */
    /* if (mainw->rec_aclip != -1) { */
    /*   //mainw->rec_aseek = fabs((double)fwd_seek_pos */
    /*   mainw->rec_aseek = fabs((double)sfile->aseek_pos */
    /*                           / (double)(afile->achans * afile->asampsize / 8) / (double)afile->arps) */
    /*                      + (double)(mainw->startticks - mainw->currticks) / TICKS_PER_SECOND_DBL; */
    /*   mainw->rec_avel = fabs((double)pulsed->in_arate */
    /*                          / (double)afile->arps) * (double)afile->adirection; */
    /* } */
  }
}

#undef afile

#endif

