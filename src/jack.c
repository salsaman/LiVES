// jack.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2019
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include <jack/jslist.h>
#include <jack/control.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_JACK
#include "callbacks.h"
#include "effects.h"
#include "effects-weed.h"

#define afile mainw->files[jackd->playing_file]

static jack_client_t *jack_transport_client;
static lives_audio_buf_t *cache_buffer = NULL;

static unsigned char *zero_buff = NULL;
static size_t zero_buff_count = 0;

static boolean seek_err;

#ifdef USE_JACKCTL
static jackctl_server_t *jackserver = NULL;
#endif

#define JACK_READ_BYTES 262144  ///< 256 * 1024

static uint8_t jrbuf[JACK_READ_BYTES * 2];

static size_t jrb = 0;

static off_t fwd_seek_pos = 0;

static size_t audio_read_inner(jack_driver_t *jackd, float **in_buffer, int fileno,
                               int nframes, double out_scale, boolean rev_endian, boolean out_unsigned, size_t rbytes);


#ifdef ENABLE_JACK_TRANSPORT
static boolean jack_playall(livespointer data) {
  on_playall_activate(NULL, NULL);
  return FALSE;
}
#endif

// round int a up to next multiple of int b, unless a is already a multiple of b
LIVES_LOCAL_INLINE int64_t align_ceilng(int64_t val, int mod) {
  return (int64_t)((double)(val + mod - 1.) / (double)mod) * (int64_t)mod;
}

static boolean check_zero_buff(size_t check_size) {
  if (check_size > zero_buff_count) {
    zero_buff = (unsigned char *)lives_realloc(zero_buff, check_size);
    if (zero_buff) {
      lives_memset(zero_buff + zero_buff_count, 0, check_size - zero_buff_count);
      zero_buff_count = check_size;
      return TRUE;
    }
    zero_buff_count = 0;
    return FALSE;
  }
  return TRUE;

}


boolean lives_jack_init(void) {
  char *jt_client = lives_strdup_printf("LiVES-%d", capable->mainpid);
  jack_options_t options = JackServerName;
  jack_status_t status;
#ifdef USE_JACKCTL
  jackctl_driver_t *driver = NULL;
#endif

  const char *server_name = JACK_DEFAULT_SERVER_NAME;

  jack_transport_client = NULL;

#ifdef USE_JACKCTL
  if ((prefs->jack_opts & JACK_OPTS_START_TSERVER) || (prefs->jack_opts & JACK_OPTS_START_ASERVER)) {
    const JSList *drivers;
#ifdef JACK_SYNC_MODE
    const JSList *params;
#endif
    // start the server
    jackserver = jackctl_server_create(NULL, NULL);
    if (!jackserver) {
      LIVES_ERROR("Could not create jackd server");
      return FALSE;
    }

#ifdef JACK_SYNC_MODE
    // list server parameters
    params = jackctl_server_get_parameters(jackserver);
    while (params) {
      jackctl_parameter_t *parameter = (jackctl_parameter_t *)params->data;
      if (!strcmp(jackctl_parameter_get_name(parameter), "sync")) {
        union jackctl_parameter_value value;
        value.b = TRUE;
        jackctl_parameter_set_value(parameter, &value);
        break;
      }
      params = jack_slist_next(params);
    }
#endif

    drivers = jackctl_server_get_drivers_list(jackserver);
    while (drivers) {
      driver = (jackctl_driver_t *)drivers->data;
      g_print("DRIVER %s\n", jackctl_driver_get_name(driver));
      if (!strcmp(jackctl_driver_get_name(driver), JACK_DRIVER_NAME)) {
        break;
      }
      drivers = jack_slist_next(drivers);
    }
    if (!driver) {
      LIVES_ERROR("Could not find a suitable driver for jack");
      return FALSE;
    }

    if (!jackctl_server_open(jackserver, driver)) {
      LIVES_ERROR("Could not create the driver for jack");
      return FALSE;
    }

    if (!jackctl_server_start(jackserver)) {
      LIVES_ERROR("Could not start the jack server");
      return FALSE;
    }
  }

  mainw->jack_inited = TRUE;

  options = (jack_options_t)((int)options | (int)JackNoStartServer);

#else

  if ((prefs->jack_opts & JACK_OPTS_START_TSERVER) || (prefs->jack_opts & JACK_OPTS_START_ASERVER)) {
    unsetenv("JACK_NO_START_SERVER");
    setenv("JACK_START_SERVER", "1", 0);

    if (!lives_file_test(prefs->jack_aserver, LIVES_FILE_TEST_EXISTS)) {
      char *com;
      char jackd_loc[PATH_MAX];
      get_location(EXEC_JACKD, jackd_loc, PATH_MAX);
      if (strlen(jackd_loc)) {
        com = lives_strdup_printf("echo \"%s -d %s\">\"%s\"", jackd_loc, JACK_DRIVER_NAME, prefs->jack_aserver);
        lives_system(com, FALSE);
        lives_free(com);
        lives_chmod(prefs->jack_aserver, "o+x");
      }
    }

  } else {
    unsetenv("JACK_START_SERVER");
    setenv("JACK_NO_START_SERVER", "1", 0);
    options = (jack_options_t)((int)options | (int)JackNoStartServer);
  }

#endif

  // startup the transport client now; we will open another later for audio
  jack_transport_client = jack_client_open(jt_client, options, &status, server_name);
  lives_free(jt_client);

  if (!jack_transport_client) return FALSE;

#ifdef ENABLE_JACK_TRANSPORT
  jack_activate(jack_transport_client);
  jack_set_sync_timeout(jack_transport_client, 5000000); // seems to not work
  jack_set_sync_callback(jack_transport_client, lives_start_ready_callback, NULL);
  mainw->jack_trans_poll = TRUE;
#else
  jack_client_close(jack_transport_client);
  jack_transport_client = NULL;
#endif

  if (status & JackServerStarted) {
    d_print(_("JACK server started\n"));
  }

  return TRUE;
}

/////////////////////////////////////////////////////////////////
// transport handling


ticks_t jack_transport_get_current_ticks(void) {
#ifdef ENABLE_JACK_TRANSPORT
  double val;
  jack_nframes_t srate;
  jack_position_t pos;

  jack_transport_query(jack_transport_client, &pos);

  srate = jack_get_sample_rate(jack_transport_client);
  val = (double)pos.frame / (double)srate;

  if (val > 0.) return val * TICKS_PER_SECOND_DBL;
#endif
  return -1;
}


#ifdef ENABLE_JACK_TRANSPORT
static void jack_transport_check_state(void) {
  jack_position_t pos;
  jack_transport_state_t jacktstate;

  // go away until the app has started up properly
  if (mainw->go_away) return;

  if (!(prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT)) return;

  if (!jack_transport_client) return;

  jacktstate = jack_transport_query(jack_transport_client, &pos);

  if (mainw->jack_can_start && (jacktstate == JackTransportRolling || jacktstate == JackTransportStarting) &&
      !LIVES_IS_PLAYING && mainw->current_file > 0 && !mainw->is_processing) {
    mainw->jack_can_start = FALSE;
    mainw->jack_can_stop = TRUE;
    lives_timer_add_simple(0, jack_playall, NULL);
    return;
  }

  if (jacktstate == JackTransportStopped) {
    if (LIVES_IS_PLAYING && mainw->jack_can_stop) {
      on_stop_activate(NULL, NULL);
    }
    mainw->jack_can_start = TRUE;
  }
}
#endif


boolean lives_jack_poll(void) {
  // data is always NULL
  // must return TRUE
#ifdef ENABLE_JACK_TRANSPORT
  jack_transport_check_state();
#endif
  return TRUE;
}


void lives_jack_end(void) {
#ifdef ENABLE_JACK_TRANSPORT
  jack_client_t *client = jack_transport_client;
#endif
  jack_transport_client = NULL; // stop polling transport
#ifdef ENABLE_JACK_TRANSPORT
  if (client) {
    jack_deactivate(client);
    jack_client_close(client);
  }
#endif
#ifdef USE_JACKCTL
  if (jackserver) jackctl_server_destroy(jackserver);
  jackserver = NULL;
#endif
}


void jack_pb_start(double pbtime) {
  // call this ASAP, then in load_frame_image; we will wait for sync from other clients (and ourself !)
#ifdef ENABLE_JACK_TRANSPORT
  if (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER) {
    if (pbtime >= 0. && !mainw->jack_can_stop && (prefs->jack_opts & JACK_OPTS_TIMEBASE_LSTART))
      jack_transport_locate(jack_transport_client, pbtime * jack_get_sample_rate(jack_transport_client));
    jack_transport_start(jack_transport_client);
  }
#endif
}


void jack_pb_stop(void) {
  // call this after pb stops
#ifdef ENABLE_JACK_TRANSPORT
  if (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER) jack_transport_stop(jack_transport_client);
#endif
}

////////////////////////////////////////////
// audio

static jack_driver_t outdev[JACK_MAX_OUTDEVICES];
static jack_driver_t indev[JACK_MAX_INDEVICES];


/* not used yet */
/*
  static float set_pulse(float *buf, size_t bufsz, int step) {
  float *ptr=buf;
  float *end=buf+bufsz;

  float tot;
  int count=0;

  while (ptr<end) {
    tot+=*ptr;
    count++;
    ptr+=step;
  }
  if (count>0) return tot/(float)count;
  return 0.;
  }*/


void jack_get_rec_avals(jack_driver_t *jackd) {
  mainw->rec_aclip = jackd->playing_file;
  if (mainw->rec_aclip != -1) {
    mainw->rec_aseek = fabs((double)fwd_seek_pos / (double)(afile->achans * afile->asampsize / 8) / (double)afile->arps)
                       + (double)(mainw->startticks - mainw->currticks) / TICKS_PER_SECOND_DBL;
    mainw->rec_avel = fabs((double)jackd->sample_in_rate / (double)afile->arps) * (double)afile->adirection;
  }
}


static void jack_set_rec_avals(jack_driver_t *jackd) {
  // record direction change (internal)
  mainw->rec_aclip = jackd->playing_file;
  if (mainw->rec_aclip != -1) {
    jack_get_rec_avals(jackd);
  }
}


size_t jack_get_buffsize(jack_driver_t *jackd) {
  if (cache_buffer) return cache_buffer->bytesize;
  return 0;
}


static void push_cache_buffer(lives_audio_buf_t *cache_buffer, jack_driver_t *jackd,
                              size_t in_bytes, size_t nframes, double shrink_factor) {
  // push a cache_buffer for another thread to fill
  int qnt;
  if (!cache_buffer) return;

  qnt = afile->achans * (afile->asampsize >> 3);
  jackd->seek_pos = align_ceilng(jackd->seek_pos, qnt);

  if (mainw->ascrap_file > -1 && jackd->playing_file == mainw->ascrap_file) cache_buffer->sequential = TRUE;
  else cache_buffer->sequential = FALSE;

  cache_buffer->fileno = jackd->playing_file;

  cache_buffer->seek = jackd->seek_pos;
  cache_buffer->bytesize = in_bytes;

  cache_buffer->in_achans = jackd->num_input_channels;
  cache_buffer->out_achans = jackd->num_output_channels;

  cache_buffer->in_asamps = afile->asampsize;
  cache_buffer->out_asamps = -32;  ///< 32 bit float

  cache_buffer->shrink_factor = shrink_factor;

  cache_buffer->swap_sign = jackd->usigned;
  cache_buffer->swap_endian = jackd->reverse_endian ? SWAP_X_TO_L : 0;

  cache_buffer->samp_space = nframes;

  cache_buffer->in_interleaf = TRUE;
  cache_buffer->out_interleaf = FALSE;

  cache_buffer->operation = LIVES_READ_OPERATION;

  wake_audio_thread();
}


LIVES_INLINE lives_audio_buf_t *pop_cache_buffer(void) {
  // get next available cache_buffer
  return audio_cache_get_buffer();
}


static void output_silence(size_t offset, nframes_t nframes, jack_driver_t *jackd, float **out_buffer) {
  // write nframes silence to all output streams
  for (int i = 0; i < jackd->num_output_channels; i++) {
    if (!jackd->is_silent) {
      sample_silence_dS(out_buffer[i] + offset, nframes);
    }
    pthread_mutex_lock(&mainw->abuf_frame_mutex);
    if (mainw->audio_frame_buffer && prefs->audio_src != AUDIO_SRC_EXT) {
      // audio to be sent to video generator plugins
      append_to_audio_bufferf(out_buffer[i] + offset, nframes, i);
      if (i == jackd->num_output_channels - 1) mainw->audio_frame_buffer->samples_filled += nframes;
    }
    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
  }
  if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
    // audio to be sent to video playback plugin
    sample_silence_stream(jackd->num_output_channels, nframes);
  }
  if (jackd->astream_fd != -1) {
    // external streaming
    size_t rbytes = nframes * jackd->num_output_channels * 2;
    check_zero_buff(rbytes);
    audio_stream(zero_buff, rbytes, jackd->astream_fd);
  }
  if (!jackd->is_paused) jackd->frames_written += nframes;
  jackd->real_seek_pos = jackd->seek_pos;
  if (IS_VALID_CLIP(jackd->playing_file) && jackd->seek_pos < afile->afilesize)
    afile->aseek_pos = jackd->seek_pos;
}


static int audio_process(nframes_t nframes, void *arg) {
  // JACK calls this periodically to get the next audio buffer
  float *out_buffer[JACK_MAX_OUTPUT_PORTS];
  jack_driver_t *jackd = (jack_driver_t *)arg;
  jack_position_t pos;
  aserver_message_t *msg;
  int64_t xseek;
  int new_file;
  boolean got_cmd = FALSE;
  boolean from_memory = FALSE;
  boolean wait_cache_buffer = FALSE;
  boolean pl_error = FALSE; ///< flag tells if we had an error during plugin processing
  size_t nbytes, rbytes;
  boolean sync_ready = FALSE;

  int i;
#define DEBUG_JACK
#ifdef DEBUG_AJACK
  lives_printerr("nframes %ld, sizeof(float) == %d\n", (int64_t)nframes, sizeof(float));
#endif

  if (!mainw->is_ready || !jackd || (!LIVES_IS_PLAYING && jackd->is_silent && !jackd->msgq)) return 0;

  /* process one message */
  while ((msg = (aserver_message_t *)jackd->msgq) != NULL) {
    got_cmd = TRUE;
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file = atoi((char *)msg->data);
      if (jackd->playing_file != new_file) {
        jackd->playing_file = new_file;
      }
      jackd->seek_pos = jackd->real_seek_pos = 0;
      break;
    case ASERVER_CMD_FILE_CLOSE:
      jackd->playing_file = -1;
      jackd->in_use = FALSE;
      jackd->seek_pos = jackd->real_seek_pos = fwd_seek_pos = 0;
      break;
    case ASERVER_CMD_FILE_SEEK:
      if (jackd->playing_file < 0) break;
      xseek = atol((char *)msg->data);
      xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3));
      if (xseek < 0) xseek = 0;
      jackd->seek_pos = jackd->real_seek_pos = afile->aseek_pos = xseek;
      push_cache_buffer(cache_buffer, jackd, 0, 0, 1.);
      jackd->in_use = TRUE;
      break;
    default:
      jackd->msgq = NULL;
      msg->data = NULL;
    }

    if (msg->next != msg) lives_freep((void **) & (msg->data));
    msg->command = ASERVER_CMD_PROCESSED;
    jackd->msgq = msg->next;
    if (jackd->msgq && jackd->msgq->next == jackd->msgq) jackd->msgq->next = NULL;
  }

  /* retrieve the buffers for the output ports */
  for (i = 0; i < jackd->num_output_channels; i++)
    out_buffer[i] = (float *)jack_port_get_buffer(jackd->output_port[i], nframes);

  if (got_cmd) {
    output_silence(0, nframes, jackd, out_buffer);
    return 0;
  }

  fwd_seek_pos = jackd->real_seek_pos = jackd->seek_pos;

  if (nframes == 0) {
    return 0;
  }

  if ((mainw->agen_key == 0 || mainw->agen_needs_reinit || mainw->multitrack || mainw->preview)
      && jackd->in_use && jackd->playing_file > -1) {
    //if ((mainw->agen_key == 0 || mainw->agen_needs_reinit || mainw->multitrack) && jackd->in_use) {
    // if a plugin is generating audio we do not use cache_buffers, otherwise:
    if (jackd->read_abuf == -1) {
      // assign local copy from cache_buffers
      if (!LIVES_IS_PLAYING || (cache_buffer = pop_cache_buffer()) == NULL) {
        // audio buffer is not ready yet
        if (!jackd->is_silent) {
          output_silence(0, nframes, jackd, out_buffer);
          jackd->is_silent = TRUE;
        }
        return 0;
      }
      if (cache_buffer->fileno == -1) jackd->playing_file = -1;
    }
    if (cache_buffer && cache_buffer->in_achans > 0 && !cache_buffer->is_ready) wait_cache_buffer = TRUE;
  }

  jackd->state = jack_transport_query(jackd->client, &pos);

#ifdef DEBUG_AJACK
  lives_printerr("STATE is %d %d\n", jackd->state, jackd->play_when_stopped);
#endif

  /* handle playing state */
  if (jackd->state == JackTransportRolling || jackd->play_when_stopped) {
    uint64_t jackFramesAvailable = nframes; /* frames we have left to write to jack */
    uint64_t inputFramesAvailable;          /* frames we have available this loop */
    uint64_t numFramesToWrite;              /* num frames we are writing this loop */
    int64_t in_frames = 0;
    uint64_t in_bytes = 0, xin_bytes = 0;
    float shrink_factor = 1.f;
    double vol;
    lives_clip_t *xfile = afile;
    int qnt = 1;
    if (IS_VALID_CLIP(jackd->playing_file)) qnt = afile->achans * (afile->asampsize >> 3);

#ifdef DEBUG_AJACK
    lives_printerr("playing... jackFramesAvailable = %ld\n", jackFramesAvailable);
#endif

    jackd->num_calls++;

    if (!jackd->in_use || ((jackd->playing_file < 0 || jackd->seek_pos < 0.) && jackd->read_abuf < 0
                           && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit)
                               || mainw->multitrack || mainw->preview))
        || jackd->is_paused) {
      /* output silence if nothing is being outputted */
      if (!jackd->is_silent) {
        output_silence(0, nframes, jackd, out_buffer);
        jackd->is_silent = TRUE;
      }
      return 0;
    }

    jackd->is_silent = FALSE;

    if (!mainw->audio_seek_ready) {
      if (!mainw->video_seek_ready) {
        if (!jackd->is_silent) {
          output_silence(0, nframes, jackd, out_buffer);
          jackd->is_silent = TRUE;
        }
        fwd_seek_pos = jackd->real_seek_pos = jackd->seek_pos;
        return 0;
      }

      // preload the buffer for first read
      in_bytes = ABS((in_frames = ((double)jackd->sample_in_rate / (double)jackd->sample_out_rate *
                                   (double)jackFramesAvailable + ((double)fastrand() / (double)LIVES_MAXUINT64))))
                 * jackd->num_input_channels * jackd->bytes_per_channel;

      if (cache_buffer) push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, shrink_factor);
      mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
      mainw->fps_mini_ticks = mainw->currticks;
      mainw->fps_mini_measure = 0;

      /* g_print("@ SYNC %d seek pos %ld = %f  ct %ld   st %ld\n", mainw->actual_frame, jackd->seek_pos, */
      /* 	      ((double)jackd->seek_pos / (double)afile->arps / 4. * afile->fps + 1.), mainw->currticks, mainw->startticks); */

      pthread_mutex_lock(&mainw->avseek_mutex);
      mainw->audio_seek_ready = TRUE;
      pthread_cond_signal(&mainw->avseek_cond);
      pthread_mutex_unlock(&mainw->avseek_mutex);
    }

    if (LIVES_LIKELY(jackFramesAvailable > 0)) {
      /* (bytes of data) / (2 bytes(16 bits) * X input channels) == frames */
      if (LIVES_IS_PLAYING && jackd->read_abuf > -1) {
        // playing back from memory buffers instead of from file
        // this is used in multitrack
        from_memory = TRUE;

        numFramesToWrite = jackFramesAvailable;
        jackd->frames_written += numFramesToWrite;
        jackFramesAvailable = 0;
      } else {
        boolean eof = FALSE;
        int playfile = mainw->playing_file;
        jackd->seek_end = 0;
        if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && IS_VALID_CLIP(jackd->playing_file)) {
          if (mainw->playing_sel) {
            jackd->seek_end = (int64_t)((double)(afile->end - 1.) / afile->fps * afile->arps) * afile->achans
                              * (afile->asampsize / 8);
            if (jackd->seek_end > afile->afilesize) jackd->seek_end = afile->afilesize;
          } else {
            if (!mainw->loop_video) jackd->seek_end = (int64_t)((double)(mainw->play_end - 1.) / afile->fps * afile->arps)
                  * afile->achans * (afile->asampsize / 8);
            else jackd->seek_end = afile->afilesize;
          }
          if (jackd->seek_end > afile->afilesize) jackd->seek_end = afile->afilesize;
        }
        if (jackd->seek_end == 0 || ((jackd->playing_file == mainw->ascrap_file && !mainw->preview) && IS_VALID_CLIP(playfile)
                                     && mainw->files[playfile]->achans > 0)) jackd->seek_end = INT64_MAX;

        /* if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && IS_VALID_CLIP(jackd->playing_file)) { */
        /*   if (mainw->playing_sel) { */
        /*     jackd->seek_end = (int64_t)((double)(mainw->play_end - 1.) / afile->fps * afile->arps) */
        /*                       * afile->achans * (afile->asampsize / 8); */
        /*     if (jackd->seek_end > afile->afilesize) jackd->seek_end = afile->afilesize; */
        /*   } else jackd->seek_end = afile->afilesize; */
        /* } */
        /* if (jackd->seek_end == 0 || ((jackd->playing_file == mainw->ascrap_file && !mainw->preview) && playfile >= -1 */
        /*                              && mainw->files[playfile] && mainw->files[playfile]->achans > 0)) { */
        /*   jackd->seek_end = INT64_MAX; */
        /* } */

        in_bytes = ABS((in_frames = ((double)jackd->sample_in_rate / (double)jackd->sample_out_rate *
                                     (double)jackFramesAvailable + ((double)fastrand() / (double)LIVES_MAXUINT64))))
                   * jackd->num_input_channels * jackd->bytes_per_channel;

        // update looping mode
        if ((mainw->loop_cont || mainw->whentostop != STOP_ON_AUD_END) && !mainw->preview) {
          if (mainw->ping_pong && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
              && ((prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) || mainw->current_file == jackd->playing_file)
              && (!mainw->event_list || mainw->record || mainw->record_paused)
              && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
            jackd->loop = AUDIO_LOOP_PINGPONG;
          else jackd->loop = AUDIO_LOOP_FORWARD;
        } else {
          jackd->loop = AUDIO_LOOP_NONE;
        }

        if (cache_buffer) eof = cache_buffer->eof;

        if ((shrink_factor = (float)in_frames / (float)jackFramesAvailable) >= 0.f) {
          jackd->seek_pos += in_bytes;
          if (jackd->playing_file != mainw->ascrap_file) {
            if (eof || (jackd->seek_pos >= jackd->seek_end && !afile->opening)) {
              if (jackd->loop == AUDIO_LOOP_NONE) {
                if (*jackd->whentostop == STOP_ON_AUD_END) {
                  *jackd->cancelled = CANCEL_AUD_END;
                  jackd->in_use = FALSE;
                }
                in_bytes = 0;
              } else {
                if (jackd->loop == AUDIO_LOOP_PINGPONG && ((jackd->playing_file != mainw->playing_file)
                    || clip_can_reverse(mainw->playing_file))) {
                  jackd->sample_in_rate = -jackd->sample_in_rate;
                  afile->adirection = -afile->adirection;
                  jackd->seek_pos -= (jackd->seek_pos - jackd->seek_end);
                } else {
                  if (mainw->playing_sel) {
                    fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos
                                                     = (int64_t)((double)(afile->start - 1.) / afile->fps * afile->arps)
                                                       * afile->achans * (afile->asampsize / 8);
                  } else fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos = 0;
                  if (mainw->record && !mainw->record_paused) jack_set_rec_avals(jackd);
		// *INDENT-OFF*
                }}}}
	  // *INDENT-ON*
        } else {
          // reverse playback
          off_t seek_start = (mainw->playing_sel ?
                              (int64_t)((double)(afile->start - 1.) / afile->fps * afile->arps)
                              * afile->achans * (afile->asampsize / 8) : 0);
          seek_start = ALIGN_CEIL64(seek_start - qnt, qnt);

          if ((jackd->seek_pos -= in_bytes) < seek_start) {
            // reached beginning backwards
            if (jackd->playing_file != mainw->ascrap_file) {
              if (jackd->loop == AUDIO_LOOP_NONE) {
                if (*jackd->whentostop == STOP_ON_AUD_END) {
                  *jackd->cancelled = CANCEL_AUD_END;
                }
                jackd->in_use = FALSE;
              } else {
                if (jackd->loop == AUDIO_LOOP_PINGPONG && ((jackd->playing_file != mainw->playing_file)
                    || clip_can_reverse(mainw->playing_file))) {
                  jackd->sample_in_rate = -jackd->sample_in_rate;
                  afile->adirection = -afile->adirection;
                  shrink_factor = -shrink_factor;
                  jackd->seek_pos = seek_start;
                } else {
                  jackd->seek_pos += jackd->seek_end;
                  if (jackd->seek_pos > jackd->seek_end - in_bytes) jackd->seek_pos = jackd->seek_end - in_bytes;
                }
              }
              fwd_seek_pos = jackd->real_seek_pos = jackd->seek_pos;
              if (mainw->record && !mainw->record_paused) jack_set_rec_avals(jackd);
            }
          }
        }

        if (jackd->mute || !cache_buffer ||
            (in_bytes == 0 &&
             ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack || mainw->preview))) {
          if (!mainw->multitrack && cache_buffer && !wait_cache_buffer
              && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit)
                  || mainw->preview)) {
            push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, shrink_factor);
          }
          output_silence(0, nframes, jackd, out_buffer);
          return 0;
        } else {
          xin_bytes = 0;
        }
        if (mainw->agen_key != 0 && !mainw->multitrack && !mainw->preview) {
          // how much audio do we want to pull from any generator ?
          in_bytes = jackFramesAvailable * jackd->num_output_channels * 4;
          xin_bytes = in_bytes;
        }

        if (!jackd->in_use || in_bytes == 0) {
          // reached end of audio with no looping
          output_silence(0, nframes, jackd, out_buffer);

          jackd->is_silent = TRUE;

          if (jackd->seek_pos < 0. && jackd->playing_file > -1 && xfile) {
            jackd->seek_pos += nframes * xfile->achans * xfile->asampsize / 8;
          }
          return 0;
        }

        if (mainw->multitrack || mainw->preview || (mainw->agen_key == 0 && !mainw->agen_needs_reinit))
          inputFramesAvailable = cache_buffer->samp_space;
        else inputFramesAvailable = jackFramesAvailable;

#ifdef DEBUG_AJACK
        lives_printerr("%d inputFramesAvailable == %ld, %ld %ld,jackFramesAvailable == %ld\n", inputFramesAvailable,
                       in_frames, jackd->sample_in_rate, jackd->sample_out_rate, jackFramesAvailable);
#endif

        /* write as many bytes as we have space remaining, or as much as we have data to write */
        numFramesToWrite = MIN(jackFramesAvailable, inputFramesAvailable);

#ifdef DEBUG_AJACK
        lives_printerr("nframes == %d, jackFramesAvailable == %ld,\n\tjackd->num_input_channels == %ld,"
                       "jackd->num_output_channels == %ld, nf2w %ld, in_bytes %d, sf %.8f\n",
                       nframes, jackFramesAvailable, jackd->num_input_channels, jackd->num_output_channels,
                       numFramesToWrite, in_bytes, shrink_factor);
#endif
        jackd->frames_written += numFramesToWrite;
        jackFramesAvailable -= numFramesToWrite; /* take away what was written */

#ifdef DEBUG_AJACK
        lives_printerr("jackFramesAvailable == %ld\n", jackFramesAvailable);
#endif
      }

      // playback from memory or file
      vol = lives_vol_from_linear(future_prefs->volume * cfile->vol);

      if (sync_ready) {
        /* if (nbytes >= 8192) { */
        /*   mainw->syncticks += ((double)nbytes / (double)(jackd->out_arate) * 1000000. */
        /* 		       / (double)(jackd->out_achans * jackd->out_asamps >> 3) + .5) * USEC_TO_TICKS; */
        /* } else { */
        mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
        //}
        mainw->fps_mini_measure = 0;
        mainw->fps_mini_ticks = mainw->currticks;
        pthread_mutex_lock(&mainw->avseek_mutex);
        mainw->audio_seek_ready = TRUE;
        pthread_cond_signal(&mainw->avseek_cond);
        pthread_mutex_unlock(&mainw->avseek_mutex);
      }

      if (numFramesToWrite > 0) {
        if (!from_memory) {
          //	if (((int)(jackd->num_calls/100.))*100==jackd->num_calls) if (mainw->soft_debug) g_print("audio pip\n");
          if ((mainw->agen_key != 0 || mainw->agen_needs_reinit || cache_buffer->bufferf) && !mainw->preview &&
              !jackd->mute) { // TODO - try buffer16 instead of bufferf
            float *fbuffer = NULL;

            if (!mainw->preview && !mainw->multitrack && (mainw->agen_key != 0 || mainw->agen_needs_reinit)) {
              // audio generated from plugin
              if (mainw->agen_needs_reinit) pl_error = TRUE;
              else {
                if (!get_audio_from_plugin(out_buffer, jackd->num_output_channels,
                                           jackd->sample_out_rate, numFramesToWrite, TRUE)) {
                  pl_error = TRUE;
                }
              }

              // get back non-interleaved float fbuffer; rate and channels should match
              if (pl_error) {
                // error in plugin, put silence
                output_silence(0, numFramesToWrite, jackd, out_buffer);
              } else {
                for (i = 0; i < jackd->num_output_channels; i++) {
                  // push non-interleaved audio in fbuffer to jack
                  pthread_mutex_lock(&mainw->abuf_frame_mutex);
                  if (mainw->audio_frame_buffer && prefs->audio_src != AUDIO_SRC_EXT) {
                    // we will push the pre-effected audio to any audio reactive generators
                    append_to_audio_bufferf(out_buffer[i], numFramesToWrite, i);
                    if (i == jackd->num_output_channels - 1) mainw->audio_frame_buffer->samples_filled += numFramesToWrite;
                  }
                  pthread_mutex_unlock(&mainw->abuf_frame_mutex);
                }
              }
              //}
              if (!pl_error && has_audio_filters(AF_TYPE_ANY)) {
                float **xfltbuf;
                ticks_t tc = mainw->currticks;
                // apply inplace any effects with audio in_channels, result goes to jack
                weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                weed_layer_set_audio_data(layer, out_buffer, jackd->sample_out_rate,
                                          jackd->num_output_channels, numFramesToWrite);
                weed_set_boolean_value(layer, WEED_LEAF_HOST_KEEP_ADATA, WEED_TRUE);
                weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                xfltbuf = weed_layer_get_audio_data(layer, NULL);
                for (i = 0; i < jackd->num_output_channels; i++) {
                  if (xfltbuf[i] != out_buffer[i]) {
                    lives_memcpy(out_buffer[i], xfltbuf[i], numFramesToWrite * sizeof(float));
                    lives_free(xfltbuf[i]);
                  }
                }
                lives_free(xfltbuf);
                weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                weed_layer_free(layer);
              }

              pthread_mutex_lock(&mainw->vpp_stream_mutex);
              if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
                (*mainw->vpp->render_audio_frame_float)(out_buffer, numFramesToWrite);
              }
              pthread_mutex_unlock(&mainw->vpp_stream_mutex);

              if (mainw->record && mainw->ascrap_file != -1 && mainw->playing_file > 0) {
                // if recording we will save this audio fragment
                int out_unsigned = mainw->files[mainw->ascrap_file]->signed_endian & AFORM_UNSIGNED;
                rbytes = numFramesToWrite * mainw->files[mainw->ascrap_file]->achans *
                         mainw->files[mainw->ascrap_file]->asampsize >> 3;

                rbytes = audio_read_inner(jackd, out_buffer, mainw->ascrap_file, numFramesToWrite, 1.0,
                                          !(mainw->files[mainw->ascrap_file]->signed_endian & AFORM_BIG_ENDIAN),
                                          out_unsigned, rbytes);

                mainw->files[mainw->ascrap_file]->aseek_pos += rbytes;
              }
            } else {
              // audio from a file
              if (wait_cache_buffer) {
                while (!cache_buffer->is_ready && !cache_buffer->die) {
                  lives_usleep(prefs->sleep_time);
                }
                wait_cache_buffer = FALSE;
              }

              pthread_mutex_lock(&mainw->cache_buffer_mutex);
              if (!cache_buffer->die) {
                // push audio from cache_buffer to jack
                for (i = 0; i < jackd->num_output_channels; i++) {
                  jackd->abs_maxvol_heard = sample_move_d16_float(out_buffer[i], cache_buffer->buffer16[0] + i, numFramesToWrite,
                                            jackd->num_input_channels, afile->signed_endian & AFORM_UNSIGNED, FALSE, vol);

                  pthread_mutex_lock(&mainw->abuf_frame_mutex);
                  if (mainw->audio_frame_buffer && prefs->audio_src != AUDIO_SRC_EXT) {
                    // we will push the pre-effected audio to any audio reactive generators
                    append_to_audio_bufferf(out_buffer[i], numFramesToWrite, i);
                    if (i == jackd->num_output_channels - 1) mainw->audio_frame_buffer->samples_filled += numFramesToWrite;
                  }
                  pthread_mutex_unlock(&mainw->abuf_frame_mutex);
                }
                pthread_mutex_unlock(&mainw->cache_buffer_mutex);

                if (has_audio_filters(AF_TYPE_ANY) && jackd->playing_file != mainw->ascrap_file) {
                  float **xfltbuf;
                  ticks_t tc = mainw->currticks;
                  // apply inplace any effects with audio in_channels
                  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                  weed_set_voidptr_array(layer, WEED_LEAF_AUDIO_DATA, jackd->num_output_channels, (void **)out_buffer);
                  weed_layer_set_audio_data(layer, out_buffer, jackd->sample_out_rate,
                                            jackd->num_output_channels, numFramesToWrite);
                  weed_set_boolean_value(layer, WEED_LEAF_HOST_KEEP_ADATA, WEED_TRUE);
                  weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                  xfltbuf = weed_layer_get_audio_data(layer, NULL);
                  for (i = 0; i < jackd->num_output_channels; i++) {
                    if (xfltbuf[i] != out_buffer[i]) {
                      lives_memcpy(out_buffer[i], xfltbuf[i], numFramesToWrite * sizeof(float));
                      lives_free(xfltbuf[i]);
                    }
                  }
                  lives_free(xfltbuf);
                  weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                  weed_layer_free(layer);
                }

                pthread_mutex_lock(&mainw->vpp_stream_mutex);
                if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
                  (*mainw->vpp->render_audio_frame_float)(out_buffer, numFramesToWrite);
                }
                pthread_mutex_unlock(&mainw->vpp_stream_mutex);
              } else {
                // cache_buffer->die == TRUE
                pthread_mutex_unlock(&mainw->cache_buffer_mutex);
                output_silence(0, numFramesToWrite, jackd, out_buffer);
              }
            }

            if (jackd->astream_fd != -1) {
              // audio streaming if enabled
              unsigned char *xbuf;

              nbytes = numFramesToWrite * jackd->num_output_channels * 4;
              rbytes = numFramesToWrite * jackd->num_output_channels * 2;

              if (pl_error) {
                // generator plugin error - output silence
                check_zero_buff(rbytes);
                audio_stream(zero_buff, rbytes, jackd->astream_fd);
              } else {
                if ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) && !mainw->multitrack && !mainw->preview)
                  xbuf = (unsigned char *)cache_buffer->buffer16[0];
                else {
                  // plugin is generating and we are streaming: convert fbuffer to s16
                  float **fp = (float **)lives_malloc(jackd->num_output_channels * sizeof(float *));
                  for (i = 0; i < jackd->num_output_channels; i++) {
                    fp[i] = fbuffer + i;
                  }
                  xbuf = (unsigned char *)lives_malloc(nbytes * jackd->num_output_channels);
                  sample_move_float_int((void *)xbuf, fp, numFramesToWrite, 1.0,
                                        jackd->num_output_channels, 16, 0, TRUE, TRUE, 1.0);
                }

                if (jackd->num_output_channels != 2) {
                  // need to remap channels to stereo (assumed for now)
                  size_t bysize = 4, tsize = 0;
                  unsigned char *inbuf, *oinbuf = NULL;

                  if ((mainw->agen_key != 0 || mainw->agen_needs_reinit) && !mainw->multitrack && !mainw->preview)
                    inbuf = (unsigned char *)cache_buffer->buffer16[0];
                  else oinbuf = inbuf = xbuf;

                  xbuf = (unsigned char *)lives_malloc(nbytes);
                  if (!xbuf) {
                    // external streaming
                    rbytes = numFramesToWrite * jackd->num_output_channels * 2;
                    if (check_zero_buff(rbytes))
                      audio_stream(zero_buff, rbytes, jackd->astream_fd);
                    return 0;
                  }
                  if (jackd->num_output_channels == 1) bysize = 2;
                  while (nbytes > 0) {
                    lives_memcpy(xbuf + tsize, inbuf, bysize);
                    tsize += bysize;
                    nbytes -= bysize;
                    if (bysize == 2) {
                      // duplicate mono channel
                      lives_memcpy(xbuf + tsize, inbuf, bysize);
                      tsize += bysize;
                      nbytes -= bysize;
                      inbuf += bysize;
                    } else {
                      // or skip extra channels
                      inbuf += jackd->num_output_channels * 4;
                    }
                  }
                  nbytes = numFramesToWrite * jackd->num_output_channels * 4;
                  lives_freep((void **)&oinbuf);
                }

                // push to stream
                rbytes = numFramesToWrite * jackd->num_output_channels * 2;
                audio_stream(xbuf, rbytes, jackd->astream_fd);
                if (((mainw->agen_key != 0 || mainw->agen_needs_reinit) && !mainw->multitrack
                     && !mainw->preview) || xbuf != (unsigned char *)cache_buffer->buffer16[0]) lives_free(xbuf);
              }
            } // end audio stream
            lives_freep((void **)&fbuffer);
          } else {
            // no generator plugin, but audio is muted
            output_silence(0, numFramesToWrite, jackd, out_buffer);
          }
        } else {
          // cached from files - multitrack mode
          if (jackd->read_abuf > -1 && !jackd->mute) {
            sample_move_abuf_float(out_buffer, jackd->num_output_channels, nframes, jackd->sample_out_rate, vol);

            if (jackd->astream_fd != -1) {
              // audio streaming if enabled
              unsigned char *xbuf = (unsigned char *)out_buffer;
              nbytes = numFramesToWrite * jackd->num_output_channels * 4;

              if (jackd->num_output_channels != 2) {
                // need to remap channels to stereo (assumed for now)
                size_t bysize = 4, tsize = 0;
                unsigned char *inbuf = (unsigned char *)out_buffer;
                xbuf = (unsigned char *)lives_malloc(nbytes);
                if (!xbuf) {
                  output_silence(0, numFramesToWrite, jackd, out_buffer);
                  return 0;
                }

                if (jackd->num_output_channels == 1) bysize = 2;
                while (nbytes > 0) {
                  lives_memcpy(xbuf + tsize, inbuf, bysize);
                  tsize += bysize;
                  nbytes -= bysize;
                  if (bysize == 2) {
                    // duplicate mono channel
                    lives_memcpy(xbuf + tsize, inbuf, bysize);
                    tsize += bysize;
                    nbytes -= bysize;
                    inbuf += bysize;
                  } else {
                    // or skip extra channels
                    inbuf += jackd->num_output_channels * 4;
                  }
                }
                nbytes = numFramesToWrite * jackd->num_output_channels * 2;
              }
              rbytes = numFramesToWrite * jackd->num_output_channels * 2;
              audio_stream(xbuf, rbytes, jackd->astream_fd);
              if (xbuf != (unsigned char *)out_buffer) lives_free(xbuf);
            }
          } else {
            // muted or no audio available
            output_silence(0, numFramesToWrite, jackd, out_buffer);
          }
        }
      } else {
        // no input frames left, pad with silence
        output_silence(nframes - jackFramesAvailable, jackFramesAvailable, jackd, out_buffer);
        jackFramesAvailable = 0;
      }
    }

    if (!from_memory) {
      // push the cache_buffer to be filled
      if (!mainw->multitrack && !wait_cache_buffer && ((mainw->agen_key == 0 && ! mainw->agen_needs_reinit)
          || mainw->preview)) {
        push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, shrink_factor);
      }
      /// advance the seek pos even if we are reading from a generator
      /// audio gen outptut is float, so convert to playing file bytesize
      if (shrink_factor > 0.) jackd->seek_pos += xin_bytes / 4 * jackd->bytes_per_channel;
    }

    /*    jackd->jack_jack[0]=set_jack(out_buffer[0],jack->buffer_size,8);
      if (jackd->num_output_channels>1) {
      jackd->jack_jack[1]=set_jack(out_buffer[1],jackd->buffer_size,8);
      }
      else jackd->jack_jack[1]=jackd->jack_jack[0];
    */

    if (jackFramesAvailable > 0) {
#ifdef DEBUG_AJACK
      ++mainw->uflow_count;
      lives_printerr("buffer underrun of %ld frames\n", jackFramesAvailable);
#endif
      output_silence(nframes - jackFramesAvailable, jackFramesAvailable, jackd, out_buffer);
    }
  } else if (jackd->state == JackTransportStarting || jackd->state == JackTransportStopped ||
             jackd->state == JackTClosed || jackd->state == JackTReset) {
#ifdef DEBUG_AJACK
    lives_printerr("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif

    /* output silence if nothing is being outputted */
    output_silence(0, nframes, jackd, out_buffer);
    jackd->is_silent = TRUE;

    /* if we were told to reset then zero out some variables */
    /* and transition to STOPPED */
    if (jackd->state == JackTReset) {
      jackd->state = (jack_transport_state_t)JackTStopped; /* transition to STOPPED */
    }
  }

#ifdef DEBUG_AJACK
  lives_printerr("done\n");
#endif

  return 0;
}


int lives_start_ready_callback(jack_transport_state_t state, jack_position_t *pos, void *arg) {
  // mainw->video_seek_ready is generally FALSE
  // if we are not playing, the transport poll should start playing which will set set
  // mainw->video_seek_ready to true, as soon as the video is at the right place

  // if we are playing, we set mainw->scratch
  // this will either force a resync of audio in free playback
  // or reset the event_list position in multitrack playback

  /// TODO ****::   NEEDS retesting !!!!!

  // go away until the app has started up properly
  if (mainw->go_away) {
    if (state == JackTransportStopped) mainw->jack_can_start = TRUE;
    else mainw->jack_can_start = mainw->jack_can_stop = FALSE;
    return TRUE;
  }

  if (!(prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT)) return TRUE;
  if (!jack_transport_client) return TRUE;

  if (!LIVES_IS_PLAYING && state == JackTransportStopped) {
    if (prefs->jack_opts && JACK_OPTS_TIMEBASE_CLIENT) {
      double trtime = (double)jack_transport_get_current_ticks() / TICKS_PER_SECOND_DBL;
      if (!mainw->multitrack) {
#ifndef ENABLE_GIW_3
        lives_ruler_set_value(LIVES_RULER(mainw->hruler), x);
        lives_widget_queue_draw_if_visible(mainw->hruler);
#else
        lives_adjustment_set_value(giw_timeline_get_adjustment(GIW_TIMELINE(mainw->hruler)), trtime);
#endif
      } else mt_tl_move(mainw->multitrack, trtime);
    }
    return TRUE;
  }

  if (state != JackTransportStarting) return TRUE;

  if (LIVES_IS_PLAYING && prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) {
    // trigger audio resync
    mainw->scratch = SCRATCH_JUMP;
  }

  return (mainw->video_seek_ready & mainw->audio_seek_ready);
}


size_t jack_flush_read_data(size_t rbytes, void *data) {
  // rbytes here is how many bytes to write
  size_t bytes = 0;

  if (!data) {
    // final flush at end
    data = jrbuf;
    rbytes = jrb;
  }

  jrb = 0;

  if (!THREADVAR(bad_aud_file)) {
    // use write not use lives_write - because of potential threading issues
    bytes = write(mainw->aud_rec_fd, data, rbytes);
    if (bytes > 0) {
      uint64_t chk = (mainw->aud_data_written & AUD_WRITE_CHECK);
      mainw->aud_data_written += bytes;
      if (mainw->ascrap_file != -1 && mainw->files[mainw->ascrap_file] &&
          mainw->aud_rec_fd == mainw->files[mainw->ascrap_file]->cb_src)
        add_to_ascrap_mb(bytes);
      check_for_disk_space((mainw->aud_data_written & AUD_WRITE_CHECK) != chk);
    }
    if (bytes < rbytes) THREADVAR(bad_aud_file) = filename_from_fd(NULL, mainw->aud_rec_fd);
  }
  return bytes;
}


static size_t audio_read_inner(jack_driver_t *jackd, float **in_buffer, int ofileno, int nframes,
                               double out_scale, boolean rev_endian, boolean out_unsigned, size_t rbytes) {

  // read audio, buffer it and when we have enough, write it to aud_rec_fd

  int frames_out;

  size_t bytes_out;

  void *holding_buff;

  lives_clip_t *ofile = mainw->files[ofileno];

  frames_out = (int64_t)((double)nframes / out_scale + 1.);
  bytes_out = frames_out * ofile->achans * (ofile->asampsize >> 3);

  holding_buff = lives_malloc(bytes_out);
  if (!holding_buff) return 0;

  frames_out = sample_move_float_int(holding_buff, in_buffer, nframes, out_scale, ofile->achans,
                                     ofile->asampsize, out_unsigned, rev_endian, FALSE, 1.);

  if (mainw->rec_samples > 0) {
    if (frames_out > mainw->rec_samples) frames_out = mainw->rec_samples;
    mainw->rec_samples -= frames_out;
  }

  rbytes = frames_out * (ofile->asampsize / 8) * ofile->achans;
  jrb += rbytes;

  // write to jrbuf
  if (jrb < JACK_READ_BYTES && (mainw->rec_samples == -1 || frames_out < mainw->rec_samples)) {
    // buffer until we have enough
    lives_memcpy(&jrbuf[jrb - rbytes], holding_buff, rbytes);
    return rbytes;
  }

  // if we have enough, flush it to file
  if (jrb <= JACK_READ_BYTES * 2) {
    lives_memcpy(&jrbuf[jrb - rbytes], holding_buff, rbytes);
    jack_flush_read_data(jrb, jrbuf);
  } else {
    if (jrb > rbytes) jack_flush_read_data(jrb - rbytes, jrbuf);
    jack_flush_read_data(rbytes, holding_buff);
  }

  lives_free(holding_buff);

  return rbytes;
}


static int audio_read(nframes_t nframes, void *arg) {
  // read nframes from jack buffer, and then write to mainw->aud_rec_fd

  // this is the jack callback for when we are recording audio

  // for AUDIO_SRC_EXT, jackd->playing_file is actually the file we write audio to
  // which can be either the ascrap file (for playback recording), or a normal file (for voiceovers), or -1 (just listening)

  // TODO - get abs_maxvol_heard

  jack_driver_t *jackd = (jack_driver_t *)arg;
  float *in_buffer[jackd->num_input_channels];
  float out_scale;
  float tval = 0;
  int out_unsigned = AFORM_UNSIGNED;
  int i;

  size_t rbytes = 0;

  if (!jackd->in_use) return 0;

  if (mainw->playing_file < 0 && prefs->audio_src == AUDIO_SRC_EXT) return 0;

  if (mainw->effects_paused) return 0; // pause during record

  if (mainw->rec_samples == 0) return 0; // wrote enough already, return until main thread stop

  for (i = 0; i < jackd->num_input_channels; i++) {
    in_buffer[i] = (float *) jack_port_get_buffer(jackd->input_port[i], nframes);
    tval += *in_buffer[i];
  }

  if (!mainw->fs && !mainw->faded && !mainw->multitrack && mainw->ext_audio_mon)
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_mon), tval > 0.);

  jackd->frames_written += nframes;

  if (prefs->audio_src == AUDIO_SRC_EXT && (jackd->playing_file == -1 || jackd->playing_file == mainw->ascrap_file)) {
    // TODO - dont apply filters when doing ext window grab, or voiceover

    // in this case we read external audio, but maybe not record it
    // we may wish to analyse the audio for example, or push it to a video generator

    if (has_audio_filters(AF_TYPE_A)) { // AF_TYPE_A are Analyser filters (audio in but no audio channels out)
      ticks_t tc = mainw->currticks;
      weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);

      if (mainw->audio_frame_buffer && prefs->audio_src == AUDIO_SRC_EXT) {
        // if we have audio triggered gens., push audio to it
        pthread_mutex_lock(&mainw->abuf_frame_mutex);
        for (i = 0; i < jackd->num_input_channels; i++) {
          append_to_audio_bufferf(in_buffer[i], nframes, i);
        }
        mainw->audio_frame_buffer->samples_filled += nframes;
        pthread_mutex_unlock(&mainw->abuf_frame_mutex);
      }
      // apply any audio effects with in_channels and no out_channels
      weed_layer_set_audio_data(layer, in_buffer, jackd->sample_in_rate, jackd->num_output_channels, nframes);
      weed_apply_audio_effects_rt(layer, tc, TRUE, TRUE);
      weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
      weed_layer_free(layer);
    }
  }

  pthread_mutex_lock(&mainw->audio_filewriteend_mutex);

  if (mainw->record && mainw->record_paused && jrb > 0) {
    jack_flush_read_data(jrb, jrbuf);
  }

  if (jackd->playing_file == -1 || (mainw->record && mainw->record_paused)) {
    jrb = 0;
    pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
    return 0;
  }

  if (!IS_VALID_CLIP(jackd->playing_file)) out_scale = 1.0; // just listening
  else {
    out_scale = (float)afile->arate / (float)jackd->sample_in_rate; // recording to ascrap_file
    out_unsigned = afile->signed_endian & AFORM_UNSIGNED;
  }

  rbytes = audio_read_inner(jackd, in_buffer, jackd->playing_file, nframes, out_scale, jackd->reverse_endian,
                            out_unsigned, rbytes);

  if (mainw->playing_file != mainw->ascrap_file && IS_VALID_CLIP(mainw->playing_file))
    mainw->files[mainw->playing_file]->aseek_pos += rbytes;
  if (mainw->ascrap_file != -1 && !mainw->record_paused) mainw->files[mainw->ascrap_file]->aseek_pos += rbytes;

  jackd->seek_pos += rbytes;

  pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);

  if (mainw->rec_samples == 0 && mainw->cancelled == CANCEL_NONE) mainw->cancelled = CANCEL_KEEP; // we wrote the required #

  return 0;
}


int jack_get_srate(nframes_t nframes, void *arg) {
  //lives_printerr("the sample rate is now %ld/sec\n", (int64_t)nframes);
  // TODO: reset timebase
  return 0;
}


void jack_shutdown(void *arg) {
  jack_driver_t *jackd = (jack_driver_t *)arg;

  jackd->client = NULL; /* reset client */
  jackd->jackd_died = TRUE;
  jackd->msgq = NULL;

  lives_printerr("jack shutdown, setting client to 0 and jackd_died to true\n");
  lives_printerr("trying to reconnect right now\n");

  /////////////////////

  jack_audio_init();

  // TODO: init reader as well

  mainw->jackd = jack_get_driver(0, TRUE);
  mainw->jackd->msgq = NULL;

  if (mainw->jackd->playing_file != -1 && afile)
    jack_audio_seek_bytes(mainw->jackd, mainw->jackd->seek_pos, afile); // at least re-seek to the right place
}


static void jack_reset_driver(jack_driver_t *jackd) {
  // does nothing ?
  jackd->state = (jack_transport_state_t)JackTReset;
}


void jack_close_device(jack_driver_t *jackd) {
  //int i;

  //lives_printerr("closing the jack client thread\n");
  if (jackd->client) {
    jack_deactivate(jackd->client); /* supposed to help the jack_client_close() to succeed */
    //lives_printerr("after jack_deactivate()\n");
    jack_client_close(jackd->client);
  }

  jack_reset_driver(jackd);
  jackd->client = NULL;

  jackd->is_active = FALSE;

  /* free up the port strings */
  //lives_printerr("freeing up port strings\n");
  // TODO: check this
  /* if (jackd->jack_port_name_count > 1) { */
  /*   for (i = 0; i < jackd->jack_port_name_count; i++) free(jackd->jack_port_name[i]); */
  /*   free(jackd->jack_port_name); */
  /* } */
}


static void jack_error_func(const char *desc) {
  lives_printerr("Jack audio error %s\n", desc);
}


// create a new client but don't connect the ports yet
boolean jack_create_client_writer(jack_driver_t *jackd) {
  const char *client_name = "LiVES_audio_out";
  const char *server_name = JACK_DEFAULT_SERVER_NAME;
  jack_options_t options = (jack_options_t)((int)JackServerName | (int)JackNoStartServer);
  jack_status_t status;
  boolean needs_sigs = FALSE;
  lives_alarm_t alarm_handle;
  int i;

  if (mainw->aplayer_broken) return FALSE;

  jackd->is_active = FALSE;

  /* set up an error handler */
  jack_set_error_function(jack_error_func);
  jackd->client = NULL;

  if (!mainw->signals_deferred) {
    // try to handle crashes in jack_client_open()
    set_signal_handlers((SignalHandlerPointer)defer_sigint);
    needs_sigs = TRUE;
    mainw->crash_possible = 1;
  }

  alarm_handle = lives_alarm_set(LIVES_SHORT_TIMEOUT);
  while (!jackd->client && lives_alarm_check(alarm_handle) > 0) {
    jackd->client = jack_client_open(client_name, options, &status, server_name);
    lives_usleep(prefs->sleep_time);
  }
  lives_alarm_clear(alarm_handle);

  if (needs_sigs) {
    set_signal_handlers((SignalHandlerPointer)catch_sigint);
    mainw->crash_possible = 0;
  }

  if (!jackd->client) {
    lives_printerr("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      d_print(_("Unable to connect to JACK server\n"));
    }
    return FALSE;
  }

  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(jackd->client);
    lives_printerr("unique name `%s' assigned\n", client_name);
  }

  jackd->sample_out_rate = jackd->sample_in_rate = jack_get_sample_rate(jackd->client);

  //lives_printerr (lives_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  for (i = 0; i < jackd->num_output_channels; i++) {
    char portname[32];
    lives_snprintf(portname, 32, "out_%d", i);

#ifdef DEBUG_JACK_PORTS
    lives_printerr("output port %d is named '%s'\n", i, portname);
#endif

    jackd->output_port[i] = jack_port_register(jackd->client, portname,
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput | JackPortIsTerminal,
                            0);

    if (!jackd->output_port[i]) {
      lives_printerr("nay more JACK output ports available\n");
      return FALSE;
    }
    jackd->out_chans_available++;
  }

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(jackd->client, jack_get_srate, jackd);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(jackd->client, jack_shutdown, jackd);

  jack_set_process_callback((jack_client_t *)jackd->client, audio_process, jackd);

  return TRUE;
}


boolean jack_create_client_reader(jack_driver_t *jackd) {
  // open a device to read audio from jack
  const char *client_name = "LiVES_audio_in";
  const char *server_name = JACK_DEFAULT_SERVER_NAME;
  jack_options_t options = (jack_options_t)((int)JackServerName | (int)JackNoStartServer);
  jack_status_t status;
  int i;

  jackd->is_active = FALSE;

  /* set up an error handler */
  jack_set_error_function(jack_error_func);
  jackd->client = NULL;

  // create a client and attach it to the server
  while (!jackd->client)
    jackd->client = jack_client_open(client_name, options, &status, server_name);

  if (!jackd->client) {
    lives_printerr("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      d_print(_("Unable to connect to JACK server\n"));
    }
    return FALSE;
  }

  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(jackd->client);
    lives_printerr("unique name `%s' assigned\n", client_name);
  }

  jackd->sample_in_rate = jackd->sample_out_rate = jack_get_sample_rate(jackd->client);

  //lives_printerr (lives_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  // create ports for the client (left and right channels)
  for (i = 0; i < jackd->num_input_channels; i++) {
    char portname[32];
    lives_snprintf(portname, 32, "in_%d", i);

#ifdef DEBUG_JACK_PORTS
    lives_printerr("input port %d is named '%s'\n", i, portname);
#endif
    jackd->input_port[i] = jack_port_register(jackd->client, portname,
                           JACK_DEFAULT_AUDIO_TYPE,
                           JackPortIsInput | JackPortIsTerminal,
                           0);
    if (!jackd->input_port[i]) {
      lives_printerr("ne more JACK input ports available\n");
      return FALSE;
    }
  }

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(jackd->client, jack_get_srate, jackd);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(jackd->client, jack_shutdown, jackd);

  jrb = 0;
  // set process callback and start
  jack_set_process_callback(jackd->client, audio_read, jackd);

  return 0;
}


boolean jack_write_driver_activate(jack_driver_t *jackd) {
  // connect client and activate it
  int i;
  const char **ports;
  boolean failed = FALSE;

  if (jackd->is_active) return TRUE; // already running

  /* tell the JACK server that we are ready to roll */
  if (jack_activate(jackd->client)) {
    LIVES_ERROR("Cannot activate jack writer client");
    return FALSE;
  }

  // we are looking for input ports to connect to
  jackd->jack_port_flags |= JackPortIsInput;

  ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);

  if (!ports) {
    LIVES_ERROR("No jack input ports available !");
    return FALSE;
  }

  for (i = 0; ports[i]; i++) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("ports[%d] = '%s'\n", i, ports[i]);
#endif
  }
  jackd->in_chans_available = i;

  if (jackd->in_chans_available < jackd->num_output_channels) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("ERR: jack_get_ports() failed to find enough ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
    free(ports);
    LIVES_ERROR("Not enough jack input ports available !");
    return FALSE;
  }

  /* connect the ports. Note: you can't do this before
     the client is activated (this may change in the future). */
  for (i = 0; i < jackd->num_output_channels; i++) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("jack_connect() %s to port %d('%s')\n", jack_port_name(jackd->output_port[i]), i, ports[i]);
#endif
    if (jack_connect(jackd->client, jack_port_name(jackd->output_port[i]), ports[i])) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("cannot connect to output port %d('%s')\n", i, ports[i]);
#endif
      LIVES_ERROR("Cannot connect all our jack outputs");
      failed = TRUE;
    }
  }

  free(ports);

  /* if something failed we need to shut the client down and return 0 */
  if (failed) {
    LIVES_ERROR("Jack writer creation failed, closing and returning error");
    jack_close_device(jackd);
    return FALSE;
  }

  // start using soundcard as timer source
  prefs->force_system_clock = FALSE;

  jackd->is_active = TRUE;
  jackd->jackd_died = FALSE;
  jackd->in_use = FALSE;
  jackd->is_paused = FALSE;

  d_print(_("Started jack audio subsystem.\n"));

  return TRUE;
}


boolean jack_read_driver_activate(jack_driver_t *jackd, boolean autocon) {
  // connect driver for reading
  const char **ports;
  boolean failed = FALSE;
  int i;

  if (!jackd->is_active) {
    if (jack_activate(jackd->client)) {
      LIVES_ERROR("Cannot activate jack reader client");
      return FALSE;
    }
  }

  if (!autocon && (prefs->jack_opts & JACK_OPTS_NO_READ_AUTOCON)) goto jackreadactive;

  // we are looking for output ports to connect to
  jackd->jack_port_flags |= JackPortIsOutput;

  ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);

  if (!ports) {
    LIVES_ERROR("No jack output ports available !");
    return FALSE;
  }

  for (i = 0; ports[i]; i++) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("ports[%d] = '%s'\n", i, ports[i]);
#endif
  }
  jackd->out_chans_available = i;

  if (jackd->out_chans_available < jackd->num_input_channels) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("ERR: jack_get_ports() failed to find enough ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
    free(ports);
    LIVES_ERROR("Not enough jack output ports available !");
    return FALSE;
  }

  for (i = 0; i < jackd->num_input_channels; i++) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("jack_connect() %s to port name %d('%s')\n", jack_port_name(jackd->input_port[i]), i, ports[i]);
#endif
    if (jack_connect(jackd->client, ports[i], jack_port_name(jackd->input_port[i]))) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("cannot connect to input port %d('%s')\n", i, ports[i]);
#endif
      LIVES_ERROR("Cannot connect all our jack inputs");
      failed = TRUE;
    }
  }

  free(ports);

  if (failed) {
    lives_printerr("Failed, closing and returning error\n");
    jack_close_device(jackd);
    return FALSE;
  }

  // do we need to be connected for this ?
  // start using soundcard as timer source
  //prefs->force_system_clock = FALSE;

jackreadactive:

  jackd->is_active = TRUE;
  jackd->jackd_died = FALSE;
  jackd->in_use = FALSE;
  jackd->is_paused = FALSE;
  jackd->nframes_start = 0;
  d_print(_("Started jack audio reader.\n"));

  // start using soundcard as timer source
  prefs->force_system_clock = FALSE;

  return TRUE;
}


jack_driver_t *jack_get_driver(int dev_idx, boolean is_output) {
  jack_driver_t *jackd;

  if (is_output) jackd = &outdev[dev_idx];
  else jackd = &indev[dev_idx];
#ifdef TRACE_getReleaseDevice
  lives_printerr("dev_idx is %d\n", dev_idx);
#endif

  return jackd;
}


int jack_audio_init(void) {
  // initialise variables
  int i, j;
  jack_driver_t *jackd;

  for (i = 0; i < JACK_MAX_OUTDEVICES; i++) {
    jackd = &outdev[i];
    //jack_reset_dev(i, TRUE);
    jackd->dev_idx = i;
    jackd->client = NULL;
    jackd->in_use = FALSE;
    for (j = 0; j < JACK_MAX_OUTPUT_PORTS; j++) jackd->volume[j] = 1.0f;
    jackd->state = (jack_transport_state_t)JackTClosed;
    jackd->sample_out_rate = jackd->sample_in_rate = 0;
    jackd->seek_pos = jackd->seek_end = jackd->real_seek_pos = 0;
    jackd->msgq = NULL;
    jackd->num_calls = 0;
    jackd->astream_fd = -1;
    jackd->abs_maxvol_heard = 0.;
    jackd->jackd_died = FALSE;
    jackd->num_output_channels = 2;
    jackd->play_when_stopped = FALSE;
    jackd->mute = FALSE;
    jackd->is_silent = FALSE;
    jackd->out_chans_available = 0;
    jackd->is_output = TRUE;
    jackd->read_abuf = -1;
    jackd->playing_file = -1;
    jackd->frames_written = 0;
  }
  return 0;
}


int jack_audio_read_init(void) {
  int i, j;
  jack_driver_t *jackd;

  for (i = 0; i < JACK_MAX_INDEVICES; i++) {
    jackd = &indev[i];
    //jack_reset_dev(i, FALSE);
    jackd->dev_idx = i;
    jackd->client = NULL;
    jackd->in_use = FALSE;
    for (j = 0; j < JACK_MAX_INPUT_PORTS; j++) jackd->volume[j] = 1.0f;
    jackd->state = (jack_transport_state_t)JackTClosed;
    jackd->sample_out_rate = jackd->sample_in_rate = 0;
    jackd->seek_pos = jackd->seek_end = jackd->real_seek_pos = 0;
    jackd->msgq = NULL;
    jackd->num_calls = 0;
    jackd->astream_fd = -1;
    jackd->abs_maxvol_heard = 0.;
    jackd->jackd_died = FALSE;
    jackd->num_input_channels = 2;
    jackd->play_when_stopped = FALSE;
    jackd->mute = FALSE;
    jackd->in_chans_available = 0;
    jackd->is_output = FALSE;
    jackd->playing_file = -1;
    jackd->frames_written = 0;
  }
  return 0;
}


volatile aserver_message_t *jack_get_msgq(jack_driver_t *jackd) {
  if (jackd->jackd_died || mainw->aplayer_broken) return NULL;
  return jackd->msgq;
}


void jack_time_reset(jack_driver_t *jackd, int64_t offset) {
  jackd->nframes_start = jack_frame_time(jack_transport_client) + (jack_nframes_t)((float)(offset / USEC_TO_TICKS) *
                         (jack_get_sample_rate(jackd->client) / 1000000.));
  jackd->frames_written = 0;
  mainw->currticks = offset;
  mainw->deltaticks = mainw->startticks = 0;
}


ticks_t lives_jack_get_time(jack_driver_t *jackd) {
  // get the time in ticks since playback started
  volatile aserver_message_t *msg = jackd->msgq;
  jack_nframes_t frames, retframes;
  static jack_nframes_t last_frames = 0;

  if (!jackd->client) return -1;

  if (msg && msg->command == ASERVER_CMD_FILE_SEEK) {
    ticks_t timeout;
    lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
    while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(jackd)) {
      sched_yield(); // wait for seek
      lives_usleep(prefs->sleep_time);
    }
    lives_alarm_clear(alarm_handle);
    if (timeout == 0) return -1;
  }

  frames = jack_frame_time(jackd->client);

  retframes = frames;
  if (last_frames > 0 && frames <= last_frames) {
    retframes += jackd->frames_written;
  } else jackd->frames_written = 0;
  last_frames = frames;

  return (ticks_t)((frames - jackd->nframes_start) * (1000000. / jack_get_sample_rate(
                     jackd->client)) * USEC_TO_TICKS);
}


double lives_jack_get_pos(jack_driver_t *jackd) {
  // get current time position (seconds) in audio file
  if (jackd->playing_file > -1)
    return fwd_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
  // from memory
  return (double)jackd->frames_written / (double)jackd->sample_out_rate;

}


boolean jack_audio_seek_frame(jack_driver_t *jackd, double frame) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample

  volatile aserver_message_t *jmsg;
  int64_t seekstart;
  ticks_t timeout;
  double thresh = 0., delta = 0.;
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

  if (alarm_handle == ALL_USED) return FALSE;

  if (frame < 1) frame = 1;

  do {
    jmsg = jack_get_msgq(jackd);
  } while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jmsg && jmsg->command != ASERVER_CMD_FILE_SEEK);
  lives_alarm_clear(alarm_handle);
  if (timeout == 0 || jackd->playing_file == -1) {
    return FALSE;
  }
  if (frame > afile->frames && afile->frames != 0) frame = afile->frames;
  seekstart = (int64_t)((double)(frame - 1.) / afile->fps * afile->arps) * afile->achans * (afile->asampsize / 8);
  if (cache_buffer) {
    delta = (double)(seekstart - lives_buffered_offset(cache_buffer->_fd)) / (double)(afile->arps * afile->achans *
            (afile->asampsize / 8));
    thresh = 1. / (double)afile->fps;
  }
  if (delta >= thresh || delta <= -thresh)
    jack_audio_seek_bytes(jackd, seekstart, afile);
  return TRUE;
}


int64_t jack_audio_seek_bytes(jack_driver_t *jackd, int64_t bytes, lives_clip_t *sfile) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file

  volatile aserver_message_t *jmsg;
  int64_t seekstart;

  ticks_t timeout;
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

  fwd_seek_pos = bytes;

  seek_err = FALSE;

  if (alarm_handle == -1) {
    LIVES_WARN("Invalid alarm handle");
    return 0;
  }

  if (jackd->in_use) {
    do {
      jmsg = jack_get_msgq(jackd);
    } while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jmsg && jmsg->command != ASERVER_CMD_FILE_SEEK);
    lives_alarm_clear(alarm_handle);
    if (timeout == 0 || jackd->playing_file == -1) {
      if (timeout == 0) LIVES_WARN("Jack connect timed out");
      seek_err = TRUE;
      return 0;
    }
  }

  seekstart = ((int64_t)(bytes / sfile->achans / (sfile->asampsize / 8))) * sfile->achans * (sfile->asampsize / 8);

  if (seekstart < 0) seekstart = 0;
  if (seekstart > sfile->afilesize) seekstart = sfile->afilesize;
  jack_message2.command = ASERVER_CMD_FILE_SEEK;
  jack_message2.tc = lives_get_current_ticks();
  jack_message2.next = NULL;
  jack_message2.data = lives_strdup_printf("%"PRId64, seekstart);
  if (!jackd->msgq) jackd->msgq = &jack_message2;
  else jackd->msgq->next = &jack_message2;
  return seekstart;
}


boolean jack_try_reconnect(void) {
  jack_audio_init();
  jack_audio_read_init();

  // TODO: create the reader also
  mainw->jackd = jack_get_driver(0, TRUE);
  if (!jack_create_client_writer(mainw->jackd)) goto err123;

  d_print(_("\nConnection to jack audio was reset.\n"));
  return TRUE;

err123:
  mainw->aplayer_broken = TRUE;
  mainw->jackd = NULL;
  do_jack_lost_conn_error();
  return FALSE;
}


void jack_pb_end(void) {
  cache_buffer = NULL;
}


void jack_aud_pb_ready(int fileno) {
  // TODO - can we merge with switch_audio_clip()

  // prepare to play file fileno
  // - set loop mode
  // - check if we need to reconnect
  // - set vals

  // called at pb start and rec stop (after rec_ext_audio)
  char *tmpfilename = NULL;
  lives_clip_t *sfile = mainw->files[fileno];
  int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
  int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

  if (mainw->jackd && mainw->aud_rec_fd == -1) {
    mainw->jackd->is_paused = FALSE;
    mainw->jackd->mute = mainw->mute;
    if ((mainw->loop_cont || mainw->whentostop != STOP_ON_AUD_END) && !mainw->preview) {
      if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && !mainw->multitrack)
        mainw->jackd->loop = AUDIO_LOOP_PINGPONG;
      else mainw->jackd->loop = AUDIO_LOOP_FORWARD;
    } else mainw->jackd->loop = AUDIO_LOOP_NONE;
    if (sfile->achans > 0 && (!mainw->preview || (mainw->preview && mainw->is_processing)) &&
        (sfile->laudio_time > 0. || sfile->opening ||
         (mainw->multitrack && mainw->multitrack->is_rendering &&
          lives_file_test((tmpfilename = lives_get_audio_file_name(fileno)), LIVES_FILE_TEST_EXISTS)))) {
      ticks_t timeout;
      lives_alarm_t alarm_handle;

      lives_freep((void **)&tmpfilename);
      mainw->jackd->num_input_channels = sfile->achans;
      mainw->jackd->bytes_per_channel = sfile->asampsize / 8;
      mainw->jackd->sample_in_rate = sfile->arate;
      mainw->jackd->usigned = !asigned;
      mainw->jackd->seek_end = sfile->afilesize;

      if ((aendian && (capable->byte_order == LIVES_BIG_ENDIAN)) || (!aendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
        mainw->jackd->reverse_endian = TRUE;
      else mainw->jackd->reverse_endian = FALSE;

      alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
      while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd)) {
        sched_yield(); // wait for seek
        lives_usleep(prefs->sleep_time);
      }
      lives_alarm_clear(alarm_handle);
      if (timeout == 0) jack_try_reconnect();

      if ((!mainw->multitrack || mainw->multitrack->is_rendering) &&
          (!mainw->event_list || mainw->record || (mainw->preview && mainw->is_processing))) {
        // tell jack server to open audio file and start playing it
        jack_message.command = ASERVER_CMD_FILE_OPEN;
        jack_message.data = lives_strdup_printf("%d", fileno);

        jack_message.next = NULL;
        mainw->jackd->msgq = &jack_message;

        jack_audio_seek_bytes(mainw->jackd, sfile->aseek_pos, sfile);
        if (seek_err) {
          if (jack_try_reconnect()) jack_audio_seek_bytes(mainw->jackd, sfile->aseek_pos, sfile);
        }

        //mainw->jackd->in_use = TRUE;
        mainw->rec_aclip = fileno;
        mainw->rec_avel = sfile->arate / sfile->arps;
        mainw->rec_aseek = (double)sfile->aseek_pos / (double)(sfile->arps * sfile->achans * (sfile->asampsize / 8));
      }
    }
    if ((mainw->agen_key != 0 || mainw->agen_needs_reinit)
        && !mainw->multitrack && !mainw->preview) mainw->jackd->in_use = TRUE; // audio generator is active
  }
}

#undef afile

#endif
