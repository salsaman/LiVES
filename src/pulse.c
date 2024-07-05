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
#include "diagnostics.h"

#define afile mainw->files[pulsed->playing_file]

//#define DEBUG_PULSE

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

static weed_layer_t *alayers[4] = {NULL, NULL, NULL, NULL};

static weed_layer_t *make_output_layer(pulse_driver_t *, int clipno);

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
  /* if (pulsed->playing_file == mainw->ascrap_file) return; */
  /* if (!CLIP_HAS_AUDIO(clipno)) { */
  /*   pulsed->in_arate = myround(pulsed->out_arate * ratio); */
  /* } else { */
  /*   lives_clip_t *sfile = mainw->files[clipno]; */
  /*   sfile->adirection = LIVES_DIRECTION_SIG(ratio); */
  /*   if (sfile->adirection == LIVES_DIRECTION_REVERSE) { */
  /*     pulsed->in_arate = sfile->arps * ratio - .5; */
  /*     lives_buffered_rdonly_set_reversed(pulsed->fd, TRUE); */
  /*   } else { */
  /*     pulsed->in_arate = sfile->arps * ratio + .5; */
  /*     lives_buffered_rdonly_set_reversed(pulsed->fd, FALSE); */
  /*   } */
  /* } */
  /* if (AV_CLIPS_EQUAL) pulse_get_rec_avals(pulsed); */
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


static void handle_data_ready_hook(pulse_driver_t *pulsed, weed_layer_t *alayer, size_t nbytes) {
  static int writer_count = 0;
  static weed_layer_t *dr_layer = NULL;
  static lives_hook_stack_t **mystacks = NULL;

  int nchans;
  float **fltbuff;

  if (!mystacks) mystacks = self_hook_stacks(DATA_READY_HOOK);

  // here we make sure that the DATA_READY hook callbacks have all completed
  // we must do this before we can free the data from the previous cycle
  if (writer_count) {
    if (lives_hook_async_join(DATA_READY_HOOK)) {
      if (dr_layer) {
        fltbuff = weed_layer_get_audio_data(dr_layer, &nchans);
        if (fltbuff) {
          for (int i = 0; i < nchans; i++)
            if (fltbuff[i]) lives_free(fltbuff[i]);
          lives_free(fltbuff);
          weed_layer_set_audio_data(dr_layer, NULL, 0, 0, 0);
        }
      }
      cleanup_self_receipts();
      writer_count = 0;
    }
  }

  fltbuff = (float **)weed_get_voidptr_array_counted(alayer, "sbf_buff", &nchans);

  if (fltbuff) {
    if (alayers[0] && has_hook_cbs(mystacks, DATA_READY_HOOK)) {
      // if we have data ready callbacks, we will insert silence in the float buffer
      // then try to trigger the hook
      int xnchans = nchans;
      int64_t sbf_size = weed_get_int64_value(alayer, "sbf_size", NULL);
      size_t bytes = nbytes;

      if (weed_layer_get_audio_interleaved(alayer)) xnchans = 1;
      else bytes = nbytes / xnchans;

      // inject silence

      for (int i = 0; i < xnchans; i++) {
        fltbuff[i] = lives_realloc(fltbuff[i], sbf_size + bytes);
        lives_memset(&fltbuff[i][sbf_size / sizeof(float)], 0, bytes);
      }

      weed_set_voidptr_array(alayer, "sbf_buff", nchans, (void **)fltbuff);

      sbf_size += bytes;
      weed_set_int64_value(alayer, "sbf_size", sbf_size);

      if (!writer_count) {
        if (!dr_layer) dr_layer = make_output_layer(pulsed, -1);
        if (dr_layer) {
          int nch = weed_layer_get_naudchans(dr_layer);
          int arate = weed_layer_get_audio_rate(dr_layer);
          int asamps = weed_layer_get_audio_asamps(dr_layer);
          weed_layer_set_audio_data(dr_layer, fltbuff, arate, nch, sbf_size / (asamps >> 3));

          //g_print("CALLING DR hook %ld\n", sbf_size / (asamps >> 3));
          writer_count = lives_hook_trigger_async(DATA_READY_HOOK, NULL, "P", dr_layer);
          weed_set_voidptr_value(alayer, "sbf_buff", NULL);
        }
      }
    } else {
      for (int i = 0; i < nchans; i++) if (fltbuff[i]) lives_free(fltbuff[i]);
      weed_set_voidptr_value(alayer, "sbf_buff", NULL);
    }
    lives_free(fltbuff);
  }
}


static uint8_t *silbuff = NULL;
static size_t silbufsz = 0;

static void sample_silence_pulse(pulse_driver_t *pulsed, ssize_t nbytes) {
#if HAVE_PA_STREAM_BEGIN_WRITE
  size_t xbytes;
  uint8_t *pa_buff;
#endif
  size_t nsamples;
  int ret = 0;
  boolean no_add = FALSE;

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

  //if (pulsed->astream_fd != -1) audio_stream(silbuff, nbytes, pulsed->astream_fd); // old streaming API

  nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);

  // streaming API
  /* if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float && pulsed->playing_file != -1 */
  /*     && pulsed->playing_file != mainw->ascrap_file) { */
  /*   sample_silence_stream(pulsed->out_achans, nsamples); */
  /* } */


  if (mainw->audio_seek_ready != not_seeking) no_add = TRUE;

  if (no_add) goto done;

  handle_data_ready_hook(pulsed, alayers[0], nbytes);

  if (pulsed->in_use) {
    pthread_mutex_lock(&xtra_mutex);
    pulsed->extrausec += ((double)nsamples / (double)pulsed->out_arate * ONE_MILLION_DBL + .5);
    pthread_mutex_unlock(&xtra_mutex);
  }
  pulsed->samples_written += nsamples;
  if (!pulsed->is_paused) pulsed->samples_written += nsamples;
  pulsed->real_seek_pos = pulsed->seek_pos;

  /* if (LIVES_IS_PLAYING) { */
  /*   if (IS_VALID_CLIP(pulsed->playing_file) && pulsed->seek_pos < afile->afilesize) */
  /*     afile->aseek_pos = pulsed->seek_pos; */

done:
  return;
}

static volatile boolean in_ap = FALSE;

static void lives_pulse_set_client_attributes(pulse_driver_t *pulsed, int fileno,
    boolean activate, boolean running) {
  // called from CMD_FILE_OPEN and also prepare...
  // set:
  // reverse endian, paused, mute, loop mode.
  if (IS_VALID_CLIP(fileno)) {
    lives_clip_t *sfile = mainw->files[fileno];
    int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
    int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

    if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN))
        || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
      pulsed->reverse_endian = TRUE;
    else pulsed->reverse_endian = FALSE;

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
    }
  }
}


static weed_layer_t *make_output_layer(pulse_driver_t *pulsed, int clipno) {
  // analogous to how we have clip_src_grps with apparent palette / apparent_gamma for clip video
  // we can now also have clip audio_srcs and       || pulsed->seek_pos < 0 ||

  weed_layer_t *layer;
  audio_dtls adtls;
  // set intermediate format - this becomes 'apparent' audio for the clip
  adtls.rate = DEFAULT_AUDIO_RATE;
  adtls.chans = pulsed->out_achans;
  adtls.sampsz = 32;
  adtls.interleaved = FALSE;
  adtls.isflt = TRUE;
  adtls.asigned = TRUE;
  adtls.endian = DEFAULT_AUDIO_ENDIAN;

  if (!IS_VALID_CLIP(clipno))
    layer = config_alayer_dtls(NULL, &adtls);
  else {
    set_audio_apparent(clipno, &adtls);
    layer = config_alayer_for_clip(NULL, clipno);
  }
  return layer;
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

   ---- these are all now handled by audio_clipsrcs

   During playback we always send audio, even if it is silence, since the video timings may be derived from the
   (actual) audio sample count.

   ///////

   NEW: we have two new modes:
   - "pogo" mode" in this mode we have a "locked" audiop track, which is then mixed with the audio from the current clip
   thus we need to read from two file sources, and combine the result

   - in / out mode - in this mode, the audio input is read, then mixed with the normal outuput
   the input audio may be for eaxmple from a voiceover. We add ourselves like a consumer of the audio arena,
   and pull the latest audio from the arena. Any excess data
   if we dont have enought we write what we have, which may cause an underflow.

   - we could use the channel mixer from multitrack to set relative volume levels

   recording:
   for pogo mode, we need to store clip/track/velocity/offsets fro both audio, as well as the relaitve volume

   - for in / out mode, we can treat the input audio as coming from a generator and use the same recording logic
*/

/* this is now the model for a generic audio player,, steps are:
   - handle commands for open/close/seek
   - create audio_srcs and audio_layers
   - check if the player is inactive / muted

   - if we have new or changed layers,
   -- call audio_cache passing in layers
   -- layers will start to fill

   - check if we need to resync with video

   - adjust channel volume !

   - set player volume

   - if we have data_preview callbacks
   -- apply data_preview callbacks tp new_float_data

   -- convert float to s16, append back i changed
   - try to join async_callbacks from last cycle

   - if succesfull
   -- call data_ready hooks (async parallel)
   -- steal float data

   to ensure caching tracks audio we must -

   - inform dirchanges
   - inform when ping pong mode changes
*/


static tab_data_t *avers = NULL;

static void pulse_audio_write_process(pa_stream *pstream, ...) {
  va_list ap;
  va_start(ap, pstream);
  size_t nbytes = va_arg(ap, size_t);
  pulse_driver_t *pulsed = va_arg(ap, pulse_driver_t *);
  va_end(ap);

  static double last_xtime = 0.;
  double xtime = lives_get_session_time();

  //g_print("pulse wants %ld\n", nbytes);

  pa_operation *paop;
  aserver_message_t *msg;
  //ssize_t pad_bytes = 0;
  uint16_t *buffer;
  uint64_t nsamples = nbytes / pulsed->out_achans / (pulsed->out_asamps >> 3);
#if HAVE_PA_STREAM_BEGIN_WRITE
  size_t xbytes;
#endif
  off_t seek, xseek;
  pa_volume_t pavol;

  static lives_thread_data_t *tdata = NULL;
  //static arec_details *dets = NULL;

  boolean got_cmd = FALSE;
  boolean from_memory = FALSE;

  int new_file;
  int ret = 0;

  lives_proc_thread_t self = pulsed->inst;
  //g_print("pa ping\n");

  lives_hook_stack_t **mystacks = self_hook_stacks(DATA_PREVIEW_HOOK);

  float **xfbuffer = NULL;
  float **fbuffer = NULL;

  //#if NEW_CACHE
  //#endif

  if (!tdata) {
    tdata = get_thread_data();
    lives_proc_thread_set_thread_data(self, tdata);

    lives_snprintf(tdata->vars.var_origin, 128, "%s", "Pulseaudio Writer Thread");
    lives_proc_thread_include_states(self, THRD_STATE_EXTERN);
    tdata->vars.var_thrd_type = tdata->thrd_type = THRD_TYPE_AUDIO_WRITER;
  }

  ____FUNC_ENTRY____(pulse_audio_write_process, "", "VIV");

  lives_proc_thread_include_states(self, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(self, THRD_STATE_IDLING);

  in_ap = TRUE;

  //pa_thread_make_realtime(50);
  //g_print("PA\n");
  pulsed->real_seek_pos = pulsed->seek_pos;
  pulsed->pstream = pstream;

  if (mainw->aud_rec_rcpt && (!mainw->record || !IS_VALID_CLIP(mainw->ascrap_file) || !LIVES_IS_PLAYING)) {
    // TODO !!! - check if we need to call *_rec_audio_end
    lives_cb_receipt_block_cb(mainw->aud_rec_rcpt);
    /* lives_hook_cb_remove(mainw->aud_rec_rcpt); */
    /* mainw->aud_rec_rcpt = NULL; */
  }

  if (!mainw->is_ready || !pulsed || ((!LIVES_IS_PLAYING || !pulsed->in_use) && !pulsed->msgq)) {
    sample_silence_pulse(pulsed, -nbytes);
    //g_print("pt a1 %ld %d %p %d %p %ld\n",nsamples, mainw->is_ready, pulsed, mainw->playing_file, pulsed->msgq, nbytes);
    goto done;
  }

  /// handle control commands from the main (video) thread
  if ((msg = (aserver_message_t *)pulsed->msgq) != NULL) {
    //got_cmd = TRUE;
    int cmd = (int)msg->command;
    while (1) {
      g_print("pulse got cmd %d\n", cmd);
      switch (cmd) {
      case ASERVER_CMD_FILE_OPEN:

        //LIVES_ASSERT(msg->next);
        new_file = atoi((char *)msg->data);

        if (pulsed->playing_file == new_file) break;

        if (pulsed->playing_file != -1 && IS_VALID_CLIP(new_file)) {
          cmd = ASERVER_CMD_FILE_CLOSE;
          break;
        }

        got_cmd = TRUE;

        pulsed->in_use = TRUE;
        pulsed->playing_file = new_file;
        lives_pulse_set_client_attributes(pulsed, new_file, FALSE, TRUE);

        alayers[0] = make_output_layer(pulsed, new_file);

        paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
        pa_operation_unref(paop);

        if (IS_VALID_CLIP(new_file)) {
          pulsed->in_use = TRUE;
          pulsed->playing_file = new_file;
          lives_pulse_set_client_attributes(pulsed, new_file, FALSE, TRUE);
        }

        audio_cache(1, 1, alayers, self);

        fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = 0;
        break;

      case ASERVER_CMD_FILE_CLOSE:
        got_cmd = TRUE;

        alayers[0] = NULL;

        paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
        pa_operation_unref(paop);
        pulsed->playing_file = -1;
        pulsed->in_use = FALSE;
        if (cmd != (int)msg->command) {
          cmd = (int)msg->command;
          continue;
        }
        break;
      case ASERVER_CMD_FILE_SEEK:
      case ASERVER_CMD_FILE_SEEK_ADJUST:
        if (!IS_VALID_CLIP(pulsed->playing_file)) break;
        got_cmd = TRUE;
        pulsed->in_use = TRUE;

        paop = pa_stream_flush(pulsed->pstream, NULL, NULL);
        pa_operation_unref(paop);
        xseek = seek = atol((char *)msg->data);

        if (msg->command == ASERVER_CMD_FILE_SEEK_ADJUST) {
          ticks_t delta = mainw->currticks - msg->tc;
          xseek += delta / TICKS_PER_SECOND_DBL * (double)(afile->adirection * afile->arate
                   * afile->achans * (afile->asampsize >> 3));
        }
        if (seek < 0.) xseek = 0.;
        xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3));

        weed_layer_set_audio_seek(alayers[0], xseek);
        weed_layer_set_audio_vel(alayers[0], 1.0);

        g_print("layer primed and ready, -> fill big bucket\n%p and %p\n", alayers, alayers[0]);

        audio_cache(1, 1, alayers, self);

        fwd_seek_pos = pulsed->real_seek_pos = pulsed->seek_pos = afile->aseek_pos = xseek;

        if (msg->extra) {
          double ratio = lives_strtod(msg->extra);
          pulse_set_avel(pulsed, pulsed->playing_file, ratio);
        }

        /* if (pulsed->playing_file == mainw->ascrap_file || afile->adirection == LIVES_DIRECTION_FORWARD) */
        /*   //dir = fwd; */
        break;
      default:
        pulsed->msgq = NULL;
        msg->data = NULL;
      }

      if (msg->next != msg) {
        lives_freep((void **)&msg->data);
        lives_freep((void **)&msg->extra);
      }

      msg->command = ASERVER_CMD_PROCESSED;
      pulsed->msgq = msg->next;

      if (!pulsed->msgq) break;

      msg = (aserver_message_t *)pulsed->msgq;
      cmd = msg->command;
    }
  }

  //////// handled commands

  /// this is the value we will return from pulse_get_rec_avals
  fwd_seek_pos = pulsed->real_seek_pos;

  int64_t ibytes = 0, fbytes = 0, ofbytes = 0;
  double vel = 1.;
  float clip_vol = 1.;
  /* the ratio of samples in : samples out - may be negative. This is NOT the same as the velocity as it incluides a resampling factor */
  //boolean alock_mixer = FALSE;

  double seek_time = lives_aplayer_get_seek(self);

  if (seek_time >= 0.) {
    if (mainw->video_seek_ready == -1) {
      audio_sync_ready();
    } else {
      lives_aplayer_set_seek(self, -1.);
      if (mainw->audio_seek_ready != seek_converging) {
        lives_aplayer_set_pos(self, seek_time
                              * (double)(pulsed->in_arate
                                         * pulsed->in_achans * (pulsed->in_asamps >> 3)));
        mainw->audio_seek_ready = seek_active;
      }
    }
  }

  if (mainw->audio_seek_ready == seek_notarget) {
    if (!mainw->video_seek_ready) {
      sample_silence_pulse(pulsed, nbytes);
      goto done;
    }
    if (mainw->video_seek_ready == -1) {
      audio_sync_ready();
    } else {
      lives_aplayer_set_pos(self, (double)(mainw->video_seek_ready - 1.) /
                            (double)afile->fps * pulsed->in_arate
                            * pulsed->in_achans * (pulsed->in_asamps >> 3));
      mainw->audio_seek_ready = seek_active;
    }
  }

  if (mainw->audio_seek_ready == seek_active) {
    xseek = lives_aplayer_get_pos(self);
    if (xseek < 0) mainw->audio_seek_ready = seek_notarget;
    else {
      mainw->audio_seek_ready = seek_converging;
      weed_layer_set_audio_seek(alayers[0], xseek);
      weed_layer_set_audio_vel(alayers[0], 1.0);
      audio_cache(1, 1, alayers, self);
    }
  }

#ifdef DEBUG_PULSE
  lives_printerr("playing... pulseSamplesAvailable = %ld\n", pulseSamplesAvailable);
#endif

  pulsed->num_calls++;

  // set input volume level for output to sink
  if (future_prefs->volume != pulsed->volume_linear) {
    // TODO: pa_threaded_mainloop_once_unlocked() (pa 13.0 +) ??
    pa_operation *paop;
    pulsed->volume_linear = future_prefs->volume;
    pavol = pa_sw_volume_from_linear(pulsed->volume_linear);
    pa_cvolume_set(&pulsed->volume, pulsed->out_achans, pavol);

    g_print("set vol to %f %f %f %u\n", future_prefs->volume, clip_vol,
            pulsed->volume_linear, pavol);

    paop = pa_context_set_sink_input_volume
           (pulsed->con, pa_stream_get_index(pulsed->pstream), &pulsed->volume, NULL, NULL);
    g_print("2set vol to %f %f %f %u\n", future_prefs->volume, clip_vol,
            pulsed->volume_linear, pavol);
    pa_operation_unref(paop);
  }

  if (!pulsed->in_use || pulsed->seek_pos < 0 ||
      ((((!mainw->multitrack && IS_VALID_CLIP(pulsed->playing_file) && pulsed->seek_pos > afile->afilesize))
        && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit))) || mainw->multitrack)
      || pulsed->is_paused || ((mainw->pulsed_read && mainw->pulsed_read->playing_file != -1))) {
    sample_silence_pulse(pulsed, nbytes);
    goto done;
  }

  if ((alayers[0] && pulsed->in_achans > 0) ||
      (((mainw->agen_key != 0 || mainw->agen_needs_reinit)
        && !mainw->preview) && !mainw->multitrack)) {
    if (0) {
      //if (LIVES_IS_PLAYING && pulsed->read_abuf > -1) {
      from_memory = TRUE;
    } else {
      int64_t fb_offs, bb_in_offs, bb_in_pos;

      if (pulsed->playing_file > -1 && !mainw->multitrack) clip_vol = afile->vol;

      if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && !(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED))
        vel = (double)afile->pb_fps / (double)afile->fps;

      lives_aplayer_set_data_len(self, nsamples);
      LIVES_ASSERT(lives_aplayer_get_data_len(self) ==  nsamples);

      if (audio_cache(0, vel, clip_vol) == LIVES_RESULT_BUSY_RETRY) {
        if (got_cmd || mainw->audio_seek_ready == seek_converging) {
          sample_silence_pulse(pulsed, nbytes);
          goto done;
        }
        lives_microsleep_while_true(audio_cache(0, vel, clip_vol) == LIVES_RESULT_BUSY_RETRY);
      }

      if (mainw->audio_seek_ready == seek_converging) {
        mainw->audio_seek_ready = seek_approximate; // Corrected: == to =
        if (!mainw->video_seek_ready) {
          sample_silence_pulse(pulsed, nbytes);
          goto done;
        }
        if (!pthread_mutex_trylock(&mainw->avseek_mutex)) {
          pthread_mutex_unlock(&mainw->avseek_mutex);
          sample_silence_pulse(pulsed, nbytes);
          goto done;
        } else {
          if (mainw->video_seek_ready != -1) {
            weed_set_int64_value(alayers[0], "bb_adj", ((mainw->video_seek_ready - 1.) * pulsed->in_arate
                                 * pulsed->in_achans * (pulsed->in_asamps >> 3))
                                 - pulsed->seek_pos);
            mainw->audio_seek_ready = seek_ready;
          }
        }
      }

      /////////////////////////////////
      fbuffer = (float **)weed_get_voidptr_array(alayers[0], "sbf_buff", NULL);
      ofbytes = weed_get_int64_value(alayers[0], "sbf_size", NULL);
      fbytes = weed_get_int64_value(alayers[0], "sbf_newsize", NULL);
      //////////////////////////////////////////////////////

      if (!fbuffer || fbytes < nbytes) {
        sample_silence_pulse(pulsed, nbytes);
        goto done;
      }

      fb_offs = ofbytes - fbytes;
      xfbuffer = (float **)LIVES_CALLOC_SIZEOF(void *, pulsed->out_achans);
      for (int i = 0; i < pulsed->out_achans; i++)
        xfbuffer[i] = &fbuffer[i][fb_offs >> 2];

      bb_in_pos = weed_get_int64_value(alayers[0], "bb_in_pos", NULL);
      bb_in_offs = weed_get_int64_value(alayers[0], "bb_in_offs", NULL);
      pulsed->seek_pos = bb_in_pos + bb_in_offs;
      g_print("seek is at %ld: %ld +  %ld\n", pulsed->seek_pos, bb_in_pos, bb_in_offs);
    }
  } else from_memory = TRUE;


  if (from_memory) {
    /* if (pulsed->read_abuf > -1 && !pulsed->mute) { */
    /*   sample_move_abuf_float(xfbuffer, pulsed->out_achans, */
    /* 			     (nbytes >> 1) / pulsed->out_achans, pulsed->out_arate, clip_vol); */
    /* } else { */
    /*   sample_silence_pulse(pulsed, nbytes); */
    /*   goto done; */
    /* } */
  } else {
    if (has_hook_cbs(mystacks, DATA_PREVIEW_HOOK)) {
      lives_aplayer_set_data(self, (void **)xfbuffer);
      lives_aplayer_set_data_len(self, fbytes / (pulsed->out_asamps >> 3));
      lives_hook_trigger(mystacks, DATA_PREVIEW_HOOK, "P", self);
    }
  }


  ////////

  if (mainw->audio_seek_ready == seek_ready || mainw->audio_seek_ready == seek_approximate) {
    if (mainw->video_seek_ready && LIVES_IS_PLAYING) {
      if (mainw->video_seek_ready != -1) {
        if (pthread_mutex_trylock(&mainw->avseek_mutex)) {
          lives_proc_thread_sync_with(mainw->player_proc, SYNCIDX_AVSYNC, MM_IGNORE);
        } else pthread_mutex_unlock(&mainw->avseek_mutex);
      }
      audio_sync_ready();
    } else {
      if (mainw->video_seek_ready && !LIVES_IS_PLAYING) {
        audio_sync_ready();
        sample_silence_pulse(pulsed, nbytes);
        goto done;
      }
    }
  }

#if !HAVE_PA_STREAM_BEGIN_WRITE
  buffer = (uint8_t *)lives_calloc(1, nbytes);
  //
  if (!buffer) {
    sample_silence_pulse(pulsed, nbytes);
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    if (xfbuffer) lives_free(xfbuffer);
    if (fbuffer) lives_free(fbuffer);
    in_ap = FALSE;
    ____FUNC_EXIT____;
  }
#else
  xbytes = -1;
  ret = pa_stream_begin_write(pulsed->pstream, (void **)&pulsed->sound_buffer, &xbytes);
  if (ret) {
    sample_silence_pulse(pulsed, nbytes);
    in_ap = FALSE;
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    if (xfbuffer) lives_free(xfbuffer);
    if (fbuffer) lives_free(fbuffer);
    return;
  }

  if (xbytes < ibytes) ibytes = xbytes;
  buffer = pulsed->sound_buffer;
#endif
  // convert to s16
  nsamples = sample_move_float_int((void *)buffer, (float **)xfbuffer, nsamples, 1.,
                                   pulsed->out_achans, PA_SAMPSIZE, FALSE, FALSE, FALSE, 1.0);

  ibytes = nsamples * pulsed->out_achans * (pulsed->out_asamps >> 3);
  //g_print("out was %d samps, %ld bytes\n", nsamples,  ibytes);

  if (pulsed->in_use) {
    pthread_mutex_lock(&xtra_mutex);
    pulsed->extrausec += ((double)nsamples / (double)pulsed->out_arate * ONE_MILLION_DBL + .5);
    pthread_mutex_unlock(&xtra_mutex);
  }
  if (!pulsed->is_paused) pulsed->samples_written += nsamples;

  if (!pulsed->is_corked) {
    g_print("writing %ld\n", ibytes);
    pa_stream_write(pulsed->pstream, buffer, ibytes, NULL, 0, PA_SEEK_RELATIVE);
  }

  handle_data_ready_hook(pulsed, alayers[0], nbytes);

  // measure 3 things - average reuest size - average call frequency, average response time
  // averages over 50 cycles
  if (!avers) avers = init_tab_data(3, 50);
  else {
    double newvals[3];
    newvals[0] = (double)nsamples;
    newvals[1] = (double)(xtime - last_xtime);
    newvals[2] = (double)(lives_get_session_time() - xtime);
    tabdata_update(avers, newvals);


    /* g_print("pulse diag : avg nsamps %.2f avg freq, %.4f, avg service time %s\nav samps * freq = %.4f,, occupancy = %.2f %%\n ", */
    /* 	avers->avgs[0], 1. / avers->avgs[1], lives_format_timing_string(avers->avgs[2]), */
    /* 	avers->avgs[0] / avers->avgs[1], 100. * avers->avgs[2] / avers->avgs[1]); */
  }
  last_xtime = xtime;

  if (xfbuffer) lives_free(xfbuffer);
  if (fbuffer) lives_free(fbuffer);

#ifdef DEBUG_PULSE
  lives_printerr("done\n");
#endif

#endif
  ____FUNC_EXIT____;


done:
  lives_proc_thread_include_states(self, THRD_STATE_IDLING);
  lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
  in_ap = FALSE;
  if (xfbuffer) lives_free(xfbuffer);
  if (fbuffer) lives_free(fbuffer);
  ____FUNC_EXIT____;
}


//static void pulse_audio_read_process(pa_stream * pstream, size_t nbytes, void *arg) {
static void pulse_audio_read_process(pa_stream *pstream, ...) {
  va_list ap;

  va_start(ap, pstream);
  size_t nbytes = va_arg(ap, size_t);
  void *arg = va_arg(ap, void *);
  va_end(ap);

  // read nsamples from pulse buffer, and then possibly write to mainw->aud_rec_fd
  // this is the callback from pulse when we are recording or playing external audio
  static lives_thread_data_t *tdata = NULL;
  static void *back_buff = NULL;
  static size_t bbsize = 0;
  static int async_reader_count = 0;

  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  void *data;
  size_t rbytes = nbytes, zbytes, nsamples;;
  lives_proc_thread_t self = pulsed->inst;

  if (!tdata) {
    tdata = get_thread_data();
    lives_proc_thread_set_thread_data(self, tdata);
    lives_snprintf(tdata->vars.var_origin, 128, "%s", "Pulseaudio Reader Thread");
    lives_proc_thread_include_states(self, THRD_STATE_EXTERN);
    tdata->vars.var_thrd_type = tdata->thrd_type = THRD_TYPE_AUDIO_READER;
  }

  ____FUNC_ENTRY____(pulse_audio_read_process, "", "VIV");

  if (pulsed->is_corked) {
    lives_proc_thread_include_states(self, THRD_STATE_IDLING | THRD_STATE_BLOCKED);
    ____FUNC_EXIT____;
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

    nsamples = rbytes / pulsed->in_achans / (pulsed->in_asamps >> 3);

    pthread_mutex_lock(&xtra_mutex);
    if (pulsed->in_use)
      pulsed->extrausec += ((double)nsamples / (double)pulsed->in_arate * ONE_MILLION_DBL + .5);
    pthread_mutex_unlock(&xtra_mutex);
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    ____FUNC_EXIT____;
  }

  zbytes = pa_stream_readable_size(pulsed->pstream);

  if (zbytes == 0) {
    //g_print("nothing to read from PA\n");
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    ____FUNC_EXIT____;
  }

  if (pa_stream_peek(pulsed->pstream, (const void **)&data, &rbytes)) {
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    ____FUNC_EXIT____;
  }

  if (!data) {
    if (rbytes > 0) pa_stream_drop(pulsed->pstream);
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    ____FUNC_EXIT____;
  }

  nsamples = rbytes / pulsed->in_achans / (pulsed->in_asamps >> 3);

  if (mainw->ext_audio_mon && !mainw->fs && !mainw->faded && !mainw->multitrack)
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_mon), (*(uint8_t *)data & 0x80) >> 7);

  // time interpolation
  pthread_mutex_lock(&xtra_mutex);
  pulsed->extrausec += ((double)nsamples / (double)pulsed->in_arate * ONE_MILLION_DBL + .5);
  pthread_mutex_unlock(&xtra_mutex);

  // should really be samples_read here
  if (!pulsed->is_paused) {
    pulsed->samples_written += nsamples;
  }

  if (async_reader_count) {
    // here we make sure that the DATA_READY hook callbacks have all completed
    // we must do this before we can free the data from the previous cycle
    lives_hook_async_join(DATA_READY_HOOK);
    async_reader_count = 0;
    cleanup_self_receipts();
    weed_set_int64_value(alayers[0], "sbf_size", 0);
  }

  if (bbsize < rbytes) {
    if (back_buff) lives_free(back_buff);
    back_buff = lives_malloc(rbytes);
    bbsize = rbytes;
  }

  lives_memcpy(back_buff, data, rbytes);

  /* void **vbuffer = weed_get_voidptr_value(alayers[0], "sbi_buff", NULL); */
  /* nbytes = weed_get_int64_value(alayers[0], "sbi_size", NULL); */
  /* lives_memcpy(pulsed->sound_buffer, vbuffer[0], nbytes); */

  /* if (has_hook_cbs(mystacks, DATA_READY_HOOK)) { */
  /*   dr_layer = make_output_layer(pulsed, -1); */
  /*   weed_layer_set_audio_data(dr_layer, fbuffer, arate, achans, nsamples);	     */
  /*   // the DATA_READY_HOOK callbacks are run async parallel, so there is zero blocking here ! */
  /*   // however we must ensure that back_buff is not freed until the next cycle has called lives_hook_async_join() */
  /*   async_reader_count = lives_hook_trigger_async(DATA_READY_HOOK, NULL, "P", dr_layer); */
  /* } */

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
  ____FUNC_EXIT____;
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
  pulsed.seek_pos = pulsed.seek_end = pulsed.real_seek_pos = 0;
  pulsed.msgq = NULL;
  pulsed.num_calls = 0;
  pulsed.astream_fd = -1;
  pulsed.abs_maxvol_heard = 0.;
  pulsed.pulsed_died = FALSE;
  pulsed.in_achans = PA_ACHANS;
  pulsed.out_achans = PA_ACHANS;
  pulsed.out_asamps = PA_SAMPSIZE;
  pulsed.mute = FALSE;
  pulsed.out_chans_available = PULSE_MAX_OUTPUT_CHANS;
  pulsed.is_output = TRUE;
  //pulsed.read_abuf = -1;
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
  //  pulsed_reader.fd = -1;
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
static void info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
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
    pa_battr.maxlength = LIVES_PA_BUFF_MAXLEN << 2;
    pa_battr.tlength = LIVES_PA_BUFF_TARGET << 2;
    pa_battr.minreq = LIVES_PA_BUFF_MINREQ << 2;
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
  lives_aplayer_set_seek(mainw->aplayer, -1.);

  /* // create the arena buffer */
  /* register_audio_client(FALSE); */

  if (pdriver->is_output) {
    pa_volume_t pavol;
    pdriver->is_corked = TRUE;

    lives_aplayer_set_source(pdriver->inst, AUDIO_SRC_INT);
    lives_aplayer_set_achans(pdriver->inst, pdriver->out_achans);
    lives_aplayer_set_arate(pdriver->inst, pdriver->out_arate);
    lives_aplayer_set_sampsize(pdriver->inst, pdriver->out_asamps);

    // set write callback
    pa_stream_set_write_callback(pdriver->pstream, (pa_stream_request_cb_t)pulse_audio_write_process, pdriver);

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
    pdriver->samples_written = 0;
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
    pa_stream_set_read_callback(pdriver->pstream, (pa_stream_request_cb_t)pulse_audio_read_process, pdriver);

    pa_stream_connect_record(pdriver->pstream, NULL, &pa_battr,
                             (pa_stream_flags_t)(PA_STREAM_START_CORKED
                                 | PA_STREAM_INTERPOLATE_TIMING
                                 | PA_STREAM_AUTO_TIMING_UPDATE
                                 | PA_STREAM_NOT_MONOTONIC));

    pa_mloop_unlock();
    lives_millisleep_while_false(pa_stream_get_state(pdriver->pstream) == PA_STREAM_READY);
  }

  /* rfx = obj_attrs_to_rfx(pdriver->inst, TRUE); */
  /* if (pdriver->is_output) { */
  /*   desc = _("Pulse audio player details"); */
  /* } else { */
  /*   desc = _("Pulse audio reader details"); */
  /* } */
  /* rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup_printf("layout|\"%s\"|", desc)); */
  /* lives_free(desc); */
  /* rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p0|")); // source */
  /* rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p1|\"(from PA server)\"")); // rate */
  /* rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p2|")); // channels */
  /* rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p3|")); // sampsize */
  /* // skip status (uninteresting) */
  /* rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p5|p6|")); // signed / endian */

  /* pdriver->interface = rfx; */

  /* if (pdriver->is_output) { */
  /*   lives_widget_set_sensitive(mainw->show_aplayer_attr, TRUE); */
  /* } else { */
  /*   lives_widget_set_sensitive(mainw->show_aplayer_read_attr, TRUE); */
  /* } */

  return 0;
}



static void flushed_cb(pa_stream *s, int success, void *userdata) {
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
  pa_operation_unref(pdriver->paop);
  pdriver->paop = NULL;
  pdriver->is_corked = FALSE;
  prefs->force_system_clock = FALSE;
}

//#define DEBUG_PULSE_CORK
static void uncorked_cb(pa_stream *s, int success, void *userdata) {
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
#ifdef DEBUG_PULSE_CORK
  g_print("uncorked %p\n", pdriver);
#endif
  pa_operation_unref(pdriver->paop);
  pdriver->paop = pa_stream_flush(pdriver->pstream, flushed_cb, pdriver);
}



static void corked_cb(pa_stream *s, int success, void *userdata) {
  pulse_driver_t *pdriver = (pulse_driver_t *)userdata;
#ifdef DEBUG_PULSE_CORK
  g_print("corked %p\n", pdriver);
#endif
  pdriver->is_corked = TRUE;
  prefs->force_system_clock = TRUE;
  pa_threaded_mainloop_signal(pa_mloop, 0);
}


void pulse_driver_uncork(pulse_driver_t *pdriver) {
  pdriver->abs_maxvol_heard = 0.;
  if (!pdriver->is_corked) return;
  pa_mloop_lock();
  pdriver->paop = pa_stream_cork(pdriver->pstream, 0, uncorked_cb, pdriver);
  pa_mloop_unlock();
}


void pulse_driver_cork(pulse_driver_t *pdriver) {
  ticks_t timeout;
  if (pdriver->is_corked) {
    //g_print("IS CORKED\n");
    return;
  }

  while (!await_audio_queue(LIVES_DEFAULT_TIMEOUT));

  lives_alarm_set_timeout(BILLIONS(2));
  pa_mloop_lock();
  pdriver->paop = pa_stream_cork(pdriver->pstream, 1, corked_cb, pdriver);
  while (pa_operation_get_state(pdriver->paop) == PA_OPERATION_RUNNING
         && !lives_alarm_triggered()) {
    pa_threaded_mainloop_wait(pa_mloop);
  }
  pa_operation_unref(pdriver->paop);
  pa_mloop_unlock();
  lives_alarm_disarm();

  pa_mloop_lock();
  pdriver->paop = pa_stream_flush(pdriver->pstream, flushed_cb, pdriver);
  while (pa_operation_get_state(pdriver->paop) == PA_OPERATION_RUNNING
         && (timeout = lives_alarm_check(alarm_handle)) > 0) {
    pa_threaded_mainloop_wait(pa_mloop);
  }
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
  last_usec = pulsed->usec_start = usec - offset;
  pulsed->samples_written = 0;
  return TRUE;
}


/**
   @brief calculate the playback time based on samples sent to the soundcard
*/
int64_t lives_pulse_get_time(pulse_driver_t *pulsed) {
  // get the time in ticks since playback started
  // we try to get time from pa_stream_get_time() and subtract initial time
  // if we fail, just return the previous value - the timer will interpolate
  // using clock time

  volatile aserver_message_t *msg = pulsed->msgq;

  if (msg && (msg->command == ASERVER_CMD_FILE_SEEK || msg->command == ASERVER_CMD_FILE_OPEN)) {
    // if called from audio thread, then we have nothing else to clear the message queue
    // so just return the most recent value
    if (THREADVAR(fx_is_audio)) return TICKS_TO_NSEC(mainw->currticks);
    if (await_audio_queue(BILLIONS(2)) != LIVES_RESULT_SUCCESS) return -1;
  }
  return 1000 * pulsed->extrausec;
}


#define MAX_DUR BILLIONS(10)

double lives_pulse_get_timing_ratio(pulse_driver_t *pulsed, int64_t current) {
  static int64_t stcurrent0 = 0, stextra0 = 0;
  static int64_t stcurrent1 = 0, stextra1 = 0;
  int64_t extrausec = pulsed->extrausec;
  double ratio;

  if (current - stcurrent1 > MAX_DUR) {
    stcurrent0 = stcurrent1;
    stextra0 = stextra1;
    stcurrent1 = current;
    stextra1 = extrausec;
  }

  //g_print("t  rat %f / %f\n", (double)(1000. * (extrausec - stextra0)), (double)(current - stcurrent0));

  ratio = (double)(1000. * (extrausec - stextra0)) / (double)(current - stcurrent0);

  if (ratio > 1.2) ratio = 1.2;
  if (ratio < 0.8) ratio = 0.8;

  return ratio;
}


off_t lives_pulse_get_offset(pulse_driver_t *pulsed) {
  if (pulsed->playing_file > -1) return pulsed->seek_pos;
  return -1;
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


LIVES_GLOBAL_INLINE void pulse_audio_seek_frame(pulse_driver_t *pulsed, int clip, frames_t frame) {
  lives_aplayer_set_seek(pulsed->inst, (double)(frame - 1.) / (double)afile->fps);
  audio_cache(1, 1, alayers, pulsed->inst);
}


off_t pulse_audio_seek_bytes_velocity(pulse_driver_t *pulsed, off_t bytes, lives_clip_t *sfile, double vel) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file
  off_t seekstart;

  // set this here so so that pulse_get_rec_avals returns the forward seek position
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

  if (vel !=  0. && LIVES_IS_PLAYING && !mainw->preview) {
    pulse_message2.extra = lives_strdup_printf("%f", vel);
    pulse_message2.tc = mainw->currticks;
    pulse_message2.command = ASERVER_CMD_FILE_SEEK_ADJUST;
  } else lives_freep((void **)&pulse_message2.extra);

  pulse_message2.next = NULL;
  pulse_message2.data = lives_strdup_printf("%"PRId64, seekstart);
  pulse_message2.tc = 0.;

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

  //avsync_force();

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


      /* pulse_message2.command = ASERVER_CMD_FILE_SEEK; */
      /* pulse_message.next = &pulse_message2; */
      /* pulse_message2.data = lives_strdup_printf("%"PRId64, sfile->aseek_pos); */
      /* pulse_message2.next = NULL; */

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

    // wrong - we just need data_preview
    if (AUD_SRC_EXTERNAL && (prefs->audio_opts & AUDIO_OPTS_EXT_FX)) register_audio_client();

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

