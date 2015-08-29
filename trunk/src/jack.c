// jack.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2014
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_JACK
#include "callbacks.h"
#include "effects.h"
#include "effects-weed.h"

#define afile mainw->files[jackd->playing_file]

static jack_client_t *jack_transport_client;

static unsigned char *zero_buff=NULL;
static size_t zero_buff_count=0;

static boolean seek_err;

static size_t audio_read_inner(jack_driver_t *jackd, float **in_buffer, int fileno,
                               int nframes, double out_scale, boolean rev_endian, boolean out_unsigned, size_t rbytes);

static boolean check_zero_buff(size_t check_size) {
  if (check_size>zero_buff_count) {
    zero_buff=(unsigned char *)lives_try_realloc(zero_buff,check_size);
    if (zero_buff) {
      memset(zero_buff+zero_buff_count,0,check_size-zero_buff_count);
      zero_buff_count=check_size;
      return TRUE;
    }
    zero_buff_count=0;
    return FALSE;
  }
  return TRUE;
}

boolean lives_jack_init(void) {
  char *jt_client=lives_strdup_printf("LiVES-%d",capable->mainpid);
  const char *server_name="default";
  jack_options_t options=JackServerName;
  jack_status_t status;

  jack_transport_client=NULL;

  if ((prefs->jack_opts&JACK_OPTS_START_TSERVER)||(prefs->jack_opts&JACK_OPTS_START_ASERVER)) {
    unsetenv("JACK_NO_START_SERVER");
    setenv("JACK_START_SERVER","1",0);

    if (!lives_file_test(prefs->jack_aserver,LIVES_FILE_TEST_EXISTS)) {
      char *com;
      char jackd_loc[PATH_MAX];
      get_location("jackd",jackd_loc,PATH_MAX);
      if (strlen(jackd_loc)) {

#ifndef IS_DARWIN
        com=lives_strdup_printf("echo \"%s -d alsa\">\"%s\"",jackd_loc,prefs->jack_aserver);
#else
#ifdef IS_SOLARIS
        // use OSS on Solaris
        com=lives_strdup_printf("echo \"%s -d oss\">\"%s\"",jackd_loc,prefs->jack_aserver);
#else
        // use coreaudio on Darwin
        com=lives_strdup_printf("echo \"%s -d coreaudio\">\"%s\"",jackd_loc,prefs->jack_aserver);
#endif
#endif
        lives_system(com,FALSE);
        lives_free(com);
        com=lives_strdup_printf("%s o+x \"%s\"",capable->chmod_cmd,prefs->jack_aserver);
        lives_system(com,FALSE);
        lives_free(com);
      }
    }

  } else {
    unsetenv("JACK_START_SERVER");
    setenv("JACK_NO_START_SERVER","1",0);
    options=(jack_options_t)((int)options | (int)JackNoStartServer);
  }

  // startup the server
  jack_transport_client=jack_client_open(jt_client, options, &status, server_name);
  lives_free(jt_client);

  if (jack_transport_client==NULL) return FALSE;

#ifdef ENABLE_JACK_TRANSPORT
  jack_activate(jack_transport_client);
  jack_set_sync_timeout(jack_transport_client,5000000); // seems to not work
  jack_set_sync_callback(jack_transport_client, lives_start_ready_callback, NULL);
  mainw->jack_trans_poll=TRUE;
#else
  jack_client_close(jack_transport_client);
  jack_transport_client=NULL;
#endif

  if (status&JackServerStarted) {
    d_print(_("JACK server started\n"));
  }

  return TRUE;

}

/////////////////////////////////////////////////////////////////
// transport handling



uint64_t jack_transport_get_time(void) {
#ifdef ENABLE_JACK_TRANSPORT
  uint64_t val;

  jack_nframes_t srate;
  jack_position_t pos;

  jack_transport_query(jack_transport_client, &pos);

  srate=jack_get_sample_rate(jack_transport_client);
  val=(double)pos.frame/(double)srate;
  if (val>0.) return val;
#endif
  return 0.;
}






#ifdef ENABLE_JACK_TRANSPORT
static void jack_transport_check_state(void) {
  jack_position_t pos;
  jack_transport_state_t jacktstate;

  // go away until the app has started up properly
  if (mainw->go_away) return;

  if (!(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)) return;

  if (jack_transport_client==NULL) return;

  jacktstate=jack_transport_query(jack_transport_client, &pos);

  if (mainw->jack_can_start&&(jacktstate==JackTransportRolling||jacktstate==JackTransportStarting)&&
      mainw->playing_file==-1&&mainw->current_file>0&&!mainw->is_processing) {
    mainw->jack_can_start=FALSE;
    mainw->jack_can_stop=TRUE;
    // re - add the timer, as we will hang here, and we want to receive messages still during playback
    lives_timer_remove(mainw->kb_timer);
    mainw->kb_timer=lives_timer_add(KEY_RPT_INTERVAL,&ext_triggers_poll,NULL);

    on_playall_activate(NULL,NULL);

    mainw->kb_timer_end=TRUE;

  }

  if (jacktstate==JackTransportStopped) {
    if (mainw->playing_file>-1&&mainw->jack_can_stop) {
      on_stop_activate(NULL,NULL);
    }
    mainw->jack_can_start=TRUE;
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
  jack_client_t *client=jack_transport_client;
#endif
  jack_transport_client=NULL; // stop polling transport
#ifdef ENABLE_JACK_TRANSPORT
  if (client!=NULL) {
    jack_deactivate(client);
    jack_client_close(client);
  }
#endif
}


void jack_pb_start(void) {
  // call this ASAP, then in load_frame_image; we will wait for sync from other clients (and ourself !)
#ifdef ENABLE_JACK_TRANSPORT
  if (prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER) jack_transport_start(jack_transport_client);
#endif
}

void jack_pb_stop(void) {
  // call this after pb stops
#ifdef ENABLE_JACK_TRANSPORT
  if (prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER) jack_transport_stop(jack_transport_client);
#endif
}

////////////////////////////////////////////
// audio

static jack_driver_t outdev[JACK_MAX_OUTDEVICES];
static jack_driver_t indev[JACK_MAX_OUTDEVICES];


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
  mainw->rec_aclip=jackd->playing_file;
  if (mainw->rec_aclip!=-1) {
    mainw->rec_aseek=jackd->seek_pos/(double)(afile->arate*afile->achans*afile->asampsize/8);
    mainw->rec_avel=afile->pb_fps/afile->fps;
  }
}

static void jack_set_rec_avals(jack_driver_t *jackd, boolean is_forward) {
  // record direction change
  mainw->rec_aclip=jackd->playing_file;
  if (mainw->rec_aclip!=-1) {
    mainw->rec_avel=ABS(afile->pb_fps/afile->fps);
    if (!is_forward) mainw->rec_avel=-mainw->rec_avel;
    mainw->rec_aseek=(double)jackd->seek_pos/(double)(afile->arate*afile->achans*afile->asampsize/8);
  }
}


static void push_cache_buffer(lives_audio_buf_t *cache_buffer, jack_driver_t *jackd,
                              size_t in_bytes, size_t nframes, double shrink_factor) {

  // push a cache_buffer for another thread to fill

  cache_buffer->fileno = jackd->playing_file;
  cache_buffer->seek = jackd->seek_pos;
  cache_buffer->bytesize = in_bytes;

  cache_buffer->in_achans = jackd->num_input_channels;
  cache_buffer->out_achans = jackd->num_output_channels;

  cache_buffer->in_asamps = afile->asampsize;
  cache_buffer->out_asamps = -32;  ///< 32 bit float

  cache_buffer->shrink_factor = shrink_factor;

  cache_buffer->swap_sign = jackd->usigned;
  cache_buffer->swap_endian = jackd->reverse_endian?SWAP_X_TO_L:0;

  cache_buffer->samp_space=nframes;

  cache_buffer->in_interleaf=TRUE;
  cache_buffer->out_interleaf=FALSE;

  cache_buffer->operation = LIVES_READ_OPERATION;
  cache_buffer->is_ready = FALSE;
}


static lives_audio_buf_t *pop_cache_buffer(void) {
  // get next available cache_buffer

  lives_audio_buf_t *cache_buffer=audio_cache_get_buffer();
  if (cache_buffer!=NULL) return cache_buffer;
  return NULL;
}



static int audio_process(nframes_t nframes, void *arg) {
  // JACK calls this periodically to get the next audio buffer
  float *out_buffer[JACK_MAX_OUTPUT_PORTS];
  jack_driver_t *jackd = (jack_driver_t *)arg;
  jack_position_t pos;
  register int i;
  aserver_message_t *msg;
  int64_t xseek;
  int new_file;
  boolean from_memory=FALSE;
  boolean wait_cache_buffer=FALSE;
  boolean pl_error=FALSE; ///< flag tells if we had an error during plugin processing
  lives_audio_buf_t *cache_buffer=NULL;
  size_t nbytes,rbytes;

#ifdef DEBUG_AJACK
  lives_printerr("nframes %ld, sizeof(float) == %d\n", (int64_t)nframes, sizeof(float));
#endif

  if (!mainw->is_ready||jackd==NULL||(mainw->playing_file==-1&&jackd->is_silent&&jackd->msgq==NULL)) return 0;

  /* process one message */
  while ((msg=(aserver_message_t *)jackd->msgq)!=NULL) {

    // TODO - set seek_pos after setting record params
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file=atoi((char *)msg->data);
      if (jackd->playing_file!=new_file) {
        jackd->playing_file=new_file;
      }
      // TODO - use afile
      jackd->num_input_channels=afile->achans;
      jackd->sample_in_rate=(afile->arate*afile->pb_fps/afile->fps+.5);
      jackd->bytes_per_channel=afile->asampsize/8;
      jackd->seek_pos=0;
      break;
    case ASERVER_CMD_FILE_CLOSE:
      jackd->playing_file=-1;
      break;
    case ASERVER_CMD_FILE_SEEK:
      if (jackd->playing_file<0) break;
      xseek=atol((char *)msg->data);
      if (xseek<0.) xseek=0.;
      jackd->seek_pos=xseek;
      jackd->audio_ticks=mainw->currticks;
      jackd->frames_written=0;
      break;
    default:
      msg->data=NULL;
    }
    if (msg->data!=NULL) lives_free((char *)msg->data);
    msg->command=ASERVER_CMD_PROCESSED;
    if (msg->next==NULL) jackd->msgq=NULL;
    else jackd->msgq = msg->next;
  }

  if (nframes==0) return 0;

  /* retrieve the buffers for the output ports */
  for (i = 0; i < jackd->num_output_channels; i++)
    out_buffer[i] = (float *) jack_port_get_buffer(jackd->output_port[i],
                    nframes);
  if (mainw->agen_key==0||mainw->agen_needs_reinit||mainw->multitrack!=NULL) {
    // if a plugin is generating audio we do not use cache_buffers, otherwise:
    if (jackd->read_abuf==-1) {
      // assign local copy from cache_buffers
      if (mainw->playing_file==-1 || (cache_buffer = pop_cache_buffer())==NULL || !cache_buffer->is_ready) {
        // audio buffer is not ready yet
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          pthread_mutex_lock(&mainw->abuf_frame_mutex);
        }
        for (i = 0; i < jackd->num_output_channels; i++) {
          if (!jackd->is_silent) {
            sample_silence_dS(out_buffer[i], nframes);
          }
          if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
          }
        }
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          mainw->audio_frame_buffer->samples_filled+=nframes;
          pthread_mutex_unlock(&mainw->abuf_frame_mutex);
        }
        jackd->is_silent=TRUE;
        return 0;
      }
      if (cache_buffer->fileno==-1) jackd->playing_file=-1;
    }

    if (cache_buffer!=NULL && cache_buffer->in_achans>0 && !cache_buffer->is_ready) wait_cache_buffer=TRUE;
  }

  jackd->state=jack_transport_query(jackd->client, &pos);


#ifdef DEBUG_AJACK
  lives_printerr("STATE is %d %d\n",jackd->state,jackd->play_when_stopped);
#endif

  /* handle playing state */
  if (jackd->state==JackTransportRolling||jackd->play_when_stopped) {
    uint64_t jackFramesAvailable = nframes; /* frames we have left to write to jack */
    uint64_t inputFramesAvailable;          /* frames we have available this loop */
    uint64_t numFramesToWrite;              /* num frames we are writing this loop */
    int64_t in_frames=0;
    uint64_t in_bytes=0,xin_bytes=0;
    float shrink_factor=1.f;
    double vol;

    lives_clip_t *xfile=afile;

#ifdef DEBUG_AJACK
    lives_printerr("playing... jackFramesAvailable = %ld\n", jackFramesAvailable);
#endif

    jackd->num_calls++;


    if (!jackd->in_use||((jackd->playing_file<0||jackd->seek_pos<0.)&&jackd->read_abuf<0
                         &&(mainw->agen_key==0||mainw->multitrack!=NULL))
        ||jackd->is_paused) {

      /* output silence if nothing is being outputted */
      if (!jackd->is_silent) {
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          pthread_mutex_lock(&mainw->abuf_frame_mutex);
        }
        for (i = 0; i < jackd->num_output_channels; i++) {
          sample_silence_dS(out_buffer[i], nframes);
          if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
          }
        }
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          mainw->audio_frame_buffer->samples_filled+=nframes;
          pthread_mutex_unlock(&mainw->abuf_frame_mutex);
        }
      }

      jackd->is_silent=TRUE;

      // external streaming
      rbytes=nframes*jackd->num_output_channels*2;
      check_zero_buff(rbytes);
      audio_stream(zero_buff,rbytes,jackd->astream_fd);

      if (!jackd->is_paused) jackd->frames_written+=nframes;
      if (jackd->seek_pos<0.&&jackd->playing_file>-1&&afile!=NULL) {
        jackd->seek_pos+=nframes*afile->achans*afile->asampsize/8;
      }
      return 0;
    }

    jackd->is_silent=FALSE;

    if (LIVES_LIKELY(jackFramesAvailable>0)) {
      /* (bytes of data) / (2 bytes(16 bits) * X input channels) == frames */

      if (mainw->playing_file>-1&&jackd->read_abuf>-1) {
        // playing back from memory buffers instead of from file
        // this is used in multitrack
        from_memory=TRUE;

        numFramesToWrite=jackFramesAvailable;
        jackd->frames_written+=numFramesToWrite;
        jackFramesAvailable=0;

      } else {

        if (LIVES_LIKELY(jackd->playing_file>=0)) {

          if (jackd->playing_file==mainw->ascrap_file&&mainw->playing_file>=-1&&mainw->files[mainw->playing_file]->achans>0) {
            xfile=mainw->files[mainw->playing_file];
          }

          in_bytes=ABS((in_frames=((double)jackd->sample_in_rate/(double)jackd->sample_out_rate*
                                   (double)jackFramesAvailable+((double)fastrand()/(double)LIVES_MAXUINT32))))
                   *jackd->num_input_channels*jackd->bytes_per_channel;
          if ((shrink_factor=(float)in_frames/(float)jackFramesAvailable)<0.f) {
            // reverse playback
            if ((jackd->seek_pos-=in_bytes)<0) {
              // reached beginning backwards

              if (jackd->loop==AUDIO_LOOP_NONE) {
                if (*jackd->whentostop==STOP_ON_AUD_END) {
                  *jackd->cancelled=CANCEL_AUD_END;
                }
                jackd->in_use=FALSE;
                jack_set_rec_avals(jackd,FALSE);
              } else {
                if (jackd->loop==AUDIO_LOOP_PINGPONG) {
                  jackd->sample_in_rate=-jackd->sample_in_rate;
                  shrink_factor=-shrink_factor;
                  jackd->seek_pos=0;
                  jack_set_rec_avals(jackd,TRUE);
                } else jackd->seek_pos=jackd->seek_end-in_bytes;
                jack_set_rec_avals(jackd,FALSE);
              }
            }
          }

          // TODO - look into refactoring
          if (jackd->mute) {
            if (shrink_factor>0.f) jackd->seek_pos+=in_bytes;
            if (jackd->seek_pos>=jackd->seek_end) {
              if (*jackd->whentostop==STOP_ON_AUD_END) {
                *jackd->cancelled=CANCEL_AUD_END;
                jackd->in_use=FALSE;
              } else {
                if (jackd->loop==AUDIO_LOOP_PINGPONG) {
                  jackd->sample_in_rate=-jackd->sample_in_rate;
                  jackd->seek_pos-=(jackd->seek_pos - jackd->seek_end);
                } else {
                  jackd->seek_pos=0;
                }
              }
            }

            if (!wait_cache_buffer&&((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL))
              push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, shrink_factor);

            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              pthread_mutex_lock(&mainw->abuf_frame_mutex);
            }
            for (i = 0; i < jackd->num_output_channels; i++) {
              sample_silence_dS(out_buffer[i], nframes);
              if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
              }
            }
            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              mainw->audio_frame_buffer->samples_filled+=nframes;
              pthread_mutex_unlock(&mainw->abuf_frame_mutex);
            }

            // external streaming
            rbytes=nframes*jackd->num_output_channels*2;
            check_zero_buff(rbytes);
            audio_stream(zero_buff,rbytes,jackd->astream_fd);

            jackd->frames_written+=nframes;
            return 0;
          }

          else {
            if (shrink_factor>0.) {
              if (!(*jackd->cancelled)) {
                boolean eof=FALSE;

                if (mainw->agen_key==0) eof=cache_buffer->eof;
                else if (jackd->playing_file>-1 && xfile!=NULL && xfile->afilesize <=jackd->seek_pos) eof=TRUE;

                if (eof) {
                  // reached the end, forwards

                  if (*jackd->whentostop==STOP_ON_AUD_END) {
                    jackd->in_use=FALSE;
                    *jackd->cancelled=CANCEL_AUD_END;
                  } else {
                    if (jackd->loop==AUDIO_LOOP_PINGPONG) {
                      jackd->sample_in_rate=-jackd->sample_in_rate;
                      jackd->seek_pos-=in_bytes; // TODO
                      jack_set_rec_avals(jackd,FALSE);
                    } else {
                      if (jackd->loop!=AUDIO_LOOP_NONE) {
                        jackd->seek_pos=0;
                        jack_set_rec_avals(jackd,TRUE);
                      } else {
                        jackd->in_use=FALSE;
                        jack_set_rec_avals(jackd,TRUE);
                      }
                    }
                  }
                }
              }
            }
          }
          xin_bytes=in_bytes;
        }
        if (mainw->agen_key!=0&&mainw->multitrack==NULL) {
          in_bytes=jackFramesAvailable*jackd->num_output_channels*4;
          if (xin_bytes==0) xin_bytes=in_bytes;
        }

        if (!jackd->in_use||in_bytes==0) {
          // reached end of audio with no looping

          if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            pthread_mutex_lock(&mainw->abuf_frame_mutex);
          }
          for (i = 0; i < jackd->num_output_channels; i++) {
            sample_silence_dS(out_buffer[i], nframes);
            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
            }
          }
          if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            mainw->audio_frame_buffer->samples_filled+=nframes;
            pthread_mutex_unlock(&mainw->abuf_frame_mutex);
          }

          jackd->is_silent=TRUE;

          // external streaming
          rbytes=nframes*jackd->num_output_channels*2;
          check_zero_buff(rbytes);
          audio_stream(zero_buff,rbytes,jackd->astream_fd);

          if (!jackd->is_paused) jackd->frames_written+=nframes;
          if (jackd->seek_pos<0.&&jackd->playing_file>-1&&xfile!=NULL) {
            jackd->seek_pos+=nframes*xfile->achans*xfile->asampsize/8;
          }
          return 0;
        }

        if (mainw->multitrack!=NULL||(mainw->agen_key==0&&!mainw->agen_needs_reinit))
          inputFramesAvailable = cache_buffer->samp_space;
        else inputFramesAvailable=jackFramesAvailable;

#ifdef DEBUG_AJACK
        lives_printerr("%d inputFramesAvailable == %ld, %ld %ld,jackFramesAvailable == %ld\n", inputFramesAvailable,
                       in_frames,jackd->sample_in_rate,jackd->sample_out_rate,jackFramesAvailable);
#endif

        /* write as many bytes as we have space remaining, or as much as we have data to write */
        numFramesToWrite = MIN(jackFramesAvailable, inputFramesAvailable);

#ifdef DEBUG_AJACK
        lives_printerr("nframes == %d, jackFramesAvailable == %ld,\n\tjackd->num_input_channels == %ld, jackd->num_output_channels == %ld, nf2w %ld, in_bytes %d, sf %.8f\n",
                       nframes, jackFramesAvailable, jackd->num_input_channels, jackd->num_output_channels, numFramesToWrite, in_bytes, shrink_factor);
#endif
        jackd->frames_written+=numFramesToWrite;
        jackFramesAvailable-=numFramesToWrite; /* take away what was written */

#ifdef DEBUG_AJACK
        lives_printerr("jackFramesAvailable == %ld\n", jackFramesAvailable);
#endif
      }

      // playback from memory or file
      vol=mainw->volume*mainw->volume; // TODO - we should really use a logarithmic scale

      if (numFramesToWrite) {

        if (!from_memory) {
          //	if (((int)(jackd->num_calls/100.))*100==jackd->num_calls) if (mainw->soft_debug) g_print("audio pip\n");
          if ((mainw->agen_key!=0||mainw->agen_needs_reinit||cache_buffer->bufferf!=NULL)&&!jackd->mute) {
            float *fbuffer=NULL;

            if (mainw->agen_key!=0||mainw->agen_needs_reinit) {
              // audio generated from plugin

              if (mainw->agen_needs_reinit) pl_error=TRUE;
              else {
                fbuffer=(float *)lives_malloc(numFramesToWrite*jackd->num_output_channels*4);

                if (!get_audio_from_plugin(fbuffer,jackd->num_output_channels,jackd->sample_out_rate,numFramesToWrite)) {
                  pl_error=TRUE;
                }
              }

              // get back non-interleaved float fbuffer; rate and channels should match

              for (i=0; i<jackd->num_output_channels; i++) {
                if (pl_error) {
                  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                    pthread_mutex_lock(&mainw->abuf_frame_mutex);
                  }
                  for (i = 0; i < jackd->num_output_channels; i++) {
                    sample_silence_dS(out_buffer[i], numFramesToWrite);
                    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                      append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
                    }
                  }
                  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                    mainw->audio_frame_buffer->samples_filled+=numFramesToWrite;
                    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
                  }
                } else {
                  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                    pthread_mutex_lock(&mainw->abuf_frame_mutex);
                  }
                  for (i = 0; i < jackd->num_output_channels; i++) {
                    lives_memcpy(out_buffer[i],fbuffer+(i*numFramesToWrite),numFramesToWrite*sizeof(float));
                    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                      append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
                    }
                  }
                  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                    mainw->audio_frame_buffer->samples_filled+=nframes;
                    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
                  }
                }
              }

              if (!pl_error&&has_audio_filters(AF_TYPE_ANY)) {
                uint64_t tc=jackd->audio_ticks+(uint64_t)(jackd->frames_written/(double)jackd->sample_out_rate*U_SEC);
                // apply any audio effects with in_channels
                weed_apply_audio_effects_rt(out_buffer,jackd->num_output_channels,numFramesToWrite,jackd->sample_out_rate,tc,FALSE);
              }

              if (mainw->record&&mainw->ascrap_file!=-1&&mainw->playing_file>0) {
                int out_unsigned=mainw->files[mainw->ascrap_file]->signed_endian&AFORM_UNSIGNED;
                rbytes=numFramesToWrite*mainw->files[mainw->ascrap_file]->achans*
                       mainw->files[mainw->ascrap_file]->asampsize>>3;

                rbytes=audio_read_inner(jackd,out_buffer,mainw->ascrap_file,numFramesToWrite,1.0,
                                        !(mainw->files[mainw->ascrap_file]->signed_endian&AFORM_BIG_ENDIAN),out_unsigned,rbytes);
                mainw->files[mainw->ascrap_file]->aseek_pos+=rbytes;
              }

            } else {
              for (i=0; i<jackd->num_output_channels; i++) {
                sample_move_d16_float(out_buffer[i], cache_buffer->buffer16[0] + i, numFramesToWrite,
                                      jackd->num_output_channels, afile->signed_endian&AFORM_UNSIGNED, FALSE, vol);
              }

              if (has_audio_filters(AF_TYPE_ANY)&&jackd->playing_file!=mainw->ascrap_file) {
                uint64_t tc=jackd->audio_ticks+(uint64_t)(jackd->frames_written/(double)jackd->sample_out_rate*U_SEC);
                // apply any audio effects with in_channels
                weed_apply_audio_effects_rt(out_buffer,jackd->num_output_channels,numFramesToWrite,jackd->sample_out_rate,tc,FALSE);
              }

            }

            if (jackd->astream_fd!=-1) {
              // audio streaming if enabled
              unsigned char *xbuf;

              nbytes=numFramesToWrite*jackd->num_output_channels*4;
              rbytes=numFramesToWrite*jackd->num_output_channels*2;

              if (pl_error) {
                check_zero_buff(rbytes);
                audio_stream(zero_buff,rbytes,jackd->astream_fd);
              } else {
                if (mainw->agen_key==0)
                  xbuf=(unsigned char *)cache_buffer->buffer16[0];
                else {
                  // plugin is generating and we are streaming: convert fbuffer to s16
                  float **fp=(float **)lives_malloc(jackd->num_output_channels*sizeof(float *));
                  for (i=0; i<jackd->num_output_channels; i++) {
                    fp[i]=fbuffer+i;
                  }

                  xbuf=(unsigned char *)lives_malloc(nbytes*jackd->num_output_channels);

                  sample_move_float_int((void *)xbuf,fp,numFramesToWrite,1.0,jackd->num_output_channels,16,0,TRUE,TRUE,1.0);

                }

                if (jackd->num_output_channels!=2) {
                  // need to remap channels to stereo (assumed for now)
                  size_t bysize=4,tsize=0;
                  unsigned char *inbuf,*oinbuf=NULL;

                  if (mainw->agen_key!=0) inbuf=(unsigned char *)cache_buffer->buffer16[0];
                  else oinbuf=inbuf=xbuf;

                  xbuf=(unsigned char *)lives_try_malloc(nbytes);
                  if (!xbuf) {
                    // external streaming
                    rbytes=numFramesToWrite*jackd->num_output_channels*2;
                    if (check_zero_buff(rbytes))
                      audio_stream(zero_buff,rbytes,jackd->astream_fd);
                    return 0;
                  }
                  if (jackd->num_output_channels==1) bysize=2;
                  while (nbytes>0) {
                    lives_memcpy(xbuf+tsize,inbuf,bysize);
                    tsize+=bysize;
                    nbytes-=bysize;
                    if (bysize==2) {
                      // duplicate mono channel
                      lives_memcpy(xbuf+tsize,inbuf,bysize);
                      tsize+=bysize;
                      nbytes-=bysize;
                      inbuf+=bysize;
                    } else {
                      // or skip extra channels
                      inbuf+=jackd->num_output_channels*4;
                    }
                  }
                  nbytes=numFramesToWrite*jackd->num_output_channels*4;
                  if (oinbuf!=NULL) lives_free(oinbuf);
                }
                rbytes=numFramesToWrite*jackd->num_output_channels*2;
                audio_stream(xbuf,rbytes,jackd->astream_fd);
                if (mainw->agen_key!=0||xbuf!=(unsigned char *)cache_buffer->buffer16[0]) lives_free(xbuf);
              }
            }

            if (fbuffer!=NULL) lives_free(fbuffer);

          } else {
            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              pthread_mutex_lock(&mainw->abuf_frame_mutex);
            }
            for (i = 0; i < jackd->num_output_channels; i++) {
              sample_silence_dS(out_buffer[i], numFramesToWrite);
              if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
              }
            }
            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              mainw->audio_frame_buffer->samples_filled+=numFramesToWrite;
              pthread_mutex_unlock(&mainw->abuf_frame_mutex);
            }

            // external streaming
            if (jackd->astream_fd!=-1) {
              rbytes=numFramesToWrite*jackd->num_output_channels*2;
              check_zero_buff(rbytes);
              audio_stream(zero_buff,rbytes,jackd->astream_fd);
            }
          }
        } else {
          if (jackd->read_abuf>-1&&!jackd->mute) {
            sample_move_abuf_float(out_buffer,jackd->num_output_channels,nframes,jackd->sample_out_rate,vol);

            if (jackd->astream_fd!=-1) {
              // audio streaming if enabled
              unsigned char *xbuf=(unsigned char *)out_buffer;
              nbytes=numFramesToWrite*jackd->num_output_channels*4;

              if (jackd->num_output_channels!=2) {
                // need to remap channels to stereo (assumed for now)
                size_t bysize=4,tsize=0;
                unsigned char *inbuf=(unsigned char *)out_buffer;
                xbuf=(unsigned char *)lives_try_malloc(nbytes);
                if (!xbuf) {
                  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                    pthread_mutex_lock(&mainw->abuf_frame_mutex);
                  }
                  for (i = 0; i < jackd->num_output_channels; i++) {
                    sample_silence_dS(out_buffer[i], numFramesToWrite);
                    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                      append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
                    }
                  }
                  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                    mainw->audio_frame_buffer->samples_filled+=numFramesToWrite;
                    pthread_mutex_unlock(&mainw->abuf_frame_mutex);
                  }

                  // external streaming
                  rbytes=numFramesToWrite*jackd->num_output_channels*2;
                  if (check_zero_buff(rbytes))
                    audio_stream(zero_buff,rbytes,jackd->astream_fd);
                  return 0;
                }

                if (jackd->num_output_channels==1) bysize=2;
                while (nbytes>0) {
                  lives_memcpy(xbuf+tsize,inbuf,bysize);
                  tsize+=bysize;
                  nbytes-=bysize;
                  if (bysize==2) {
                    // duplicate mono channel
                    lives_memcpy(xbuf+tsize,inbuf,bysize);
                    tsize+=bysize;
                    nbytes-=bysize;
                    inbuf+=bysize;
                  } else {
                    // or skip extra channels
                    inbuf+=jackd->num_output_channels*4;
                  }
                }
                nbytes=numFramesToWrite*jackd->num_output_channels*2;
              }
              rbytes=numFramesToWrite*jackd->num_output_channels*2;
              audio_stream(xbuf,rbytes,jackd->astream_fd);
              if (xbuf!=(unsigned char *)out_buffer) lives_free(xbuf);
            }
          } else {
            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              pthread_mutex_lock(&mainw->abuf_frame_mutex);
            }
            for (i = 0; i < jackd->num_output_channels; i++) {
              sample_silence_dS(out_buffer[i], numFramesToWrite);
              if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
                append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
              }
            }
            if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              mainw->audio_frame_buffer->samples_filled+=numFramesToWrite;
              pthread_mutex_unlock(&mainw->abuf_frame_mutex);
            }

            // external streaming
            rbytes=numFramesToWrite*jackd->num_output_channels*2;
            check_zero_buff(rbytes);
            audio_stream(zero_buff,rbytes,jackd->astream_fd);
          }
        }
      } else {
        // no input frames
        nframes=jackFramesAvailable;
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          pthread_mutex_lock(&mainw->abuf_frame_mutex);
        }
        for (i = 0; i < jackd->num_output_channels; i++) {
          sample_silence_dS(out_buffer[i], numFramesToWrite);
          if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
          }
        }
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          mainw->audio_frame_buffer->samples_filled+=numFramesToWrite;
          pthread_mutex_unlock(&mainw->abuf_frame_mutex);
        }

        // external streaming
        rbytes=numFramesToWrite*jackd->num_output_channels*2;
        check_zero_buff(rbytes);
        audio_stream(zero_buff,rbytes,jackd->astream_fd);
      }

    }


    if (!from_memory) {
      if (!wait_cache_buffer&&mainw->agen_key==0)
        push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, shrink_factor);
      if (shrink_factor>0.) jackd->seek_pos+=xin_bytes;
    }

    /*    jackd->jack_pulse[0]=set_pulse(out_buffer[0],jack->buffer_size,8);
    if (jackd->num_output_channels>1) {
    jackd->jack_pulse[1]=set_pulse(out_buffer[1],jackd->buffer_size,8);
    }
    else jackd->jack_pulse[1]=jackd->jack_pulse[0];
    */

    if (jackFramesAvailable) {
#ifdef DEBUG_AJACK
      lives_printerr("buffer underrun of %ld frames\n", jackFramesAvailable);
#endif
      for (i = 0 ; i < jackd->num_output_channels; i++) {
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          pthread_mutex_lock(&mainw->abuf_frame_mutex);
        }
        for (i = 0; i < jackd->num_output_channels; i++) {
          sample_silence_dS(out_buffer[i] + (nframes - jackFramesAvailable), jackFramesAvailable);
          if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
          }
        }
        if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
          mainw->audio_frame_buffer->samples_filled+=jackFramesAvailable;
          pthread_mutex_unlock(&mainw->abuf_frame_mutex);
        }
      }

      // external streaming
      rbytes=jackFramesAvailable*jackd->num_output_channels*2;

      check_zero_buff(rbytes);
      audio_stream(zero_buff,rbytes,jackd->astream_fd);
    }

  } else if (jackd->state == JackTransportStarting || jackd->state == JackTransportStopped ||
             jackd->state == JackTClosed || jackd->state == JackTReset) {
#ifdef DEBUG_AJACK
    lives_printerr("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif

    /* output silence if nothing is being outputted */
    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
      pthread_mutex_lock(&mainw->abuf_frame_mutex);
    }
    for (i = 0; i < jackd->num_output_channels; i++) {
      sample_silence_dS(out_buffer[i], nframes);
      if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
        append_to_audio_bufferf(mainw->audio_frame_buffer,out_buffer[i],nframes,i);
      }
    }
    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
      mainw->audio_frame_buffer->samples_filled+=nframes;
      pthread_mutex_unlock(&mainw->abuf_frame_mutex);
    }

    jackd->is_silent=TRUE;

    // external streaming
    rbytes=nframes*jackd->num_output_channels*2;
    check_zero_buff(rbytes);
    audio_stream(zero_buff,rbytes,jackd->astream_fd);

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
  boolean seek_ready;

  // mainw->video_seek_ready is generally FALSE
  // if we are not playing, the transport poll should start playing which will set set
  // mainw->video_seek_ready to true, as soon as the audio seeks to the right place

  // if we are playing, we set mainw->scratch
  // this will either force a resync of audio in free playback
  // or reset the event_list position in multitrack playback

  // go away until the app has started up properly
  if (mainw->go_away) {
    if (state==JackTransportStopped) mainw->jack_can_start=TRUE;
    else mainw->jack_can_start=mainw->jack_can_stop=FALSE;
    return TRUE;
  }

  if (!(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)) return TRUE;

  if (jack_transport_client==NULL) return TRUE;

  if (state!=JackTransportStarting) return TRUE;

  // work around a bug in jack
  //seek_ready=mainw->video_seek_ready;
  seek_ready=TRUE;

  if (mainw->playing_file!=-1&&prefs->jack_opts&JACK_OPTS_TIMEBASE_CLIENT) {
    // trigger audio resync
    if (mainw->scratch==SCRATCH_NONE) mainw->scratch=SCRATCH_JUMP;
  }

  // reset for next seek
  if (seek_ready) mainw->video_seek_ready=FALSE;

  return seek_ready;

}


static size_t audio_read_inner(jack_driver_t *jackd, float **in_buffer, int ofileno, int nframes,
                               double out_scale, boolean rev_endian, boolean out_unsigned, size_t rbytes) {
  int frames_out;

  void *holding_buff=lives_try_malloc(rbytes);

  lives_clip_t *ofile=mainw->files[ofileno];

  size_t bytes=0;

  if (!holding_buff) return 0;

  frames_out=sample_move_float_int((void *)holding_buff,in_buffer,nframes,out_scale,ofile->achans,
                                   ofile->asampsize,out_unsigned,rev_endian,FALSE,1.);

  if (mainw->rec_samples>0) {
    if (frames_out>mainw->rec_samples) frames_out=mainw->rec_samples;
    mainw->rec_samples-=frames_out;
  }

  if (mainw->bad_aud_file==NULL) {
    size_t target=frames_out*(ofile->asampsize/8)*ofile->achans;
    // use write not lives_write - because of potential threading issues
    bytes=write(mainw->aud_rec_fd,holding_buff,target);

    if (bytes<target) mainw->bad_aud_file=filename_from_fd(NULL,mainw->aud_rec_fd);
    if (bytes<0) bytes=0;
  }

  lives_free(holding_buff);

  return bytes;
}


static int audio_read(nframes_t nframes, void *arg) {
  // read nframes from jack buffer, and then write to mainw->aud_rec_fd

  // this is the jack callback for when we are recording audio

  jack_driver_t *jackd = (jack_driver_t *)arg;
  float *in_buffer[jackd->num_input_channels];
  float out_scale;
  int out_unsigned;
  int i;
  int64_t frames_out;

  size_t rbytes;

  if (!jackd->in_use) return 0;

  if (mainw->playing_file<0&&prefs->audio_src==AUDIO_SRC_EXT) return 0;

  if (mainw->effects_paused) return 0; // pause during record

  if (mainw->rec_samples==0) return 0; // wrote enough already, return until main thread stop

  for (i=0; i<jackd->num_input_channels; i++) {
    in_buffer[i] = (float *) jack_port_get_buffer(jackd->input_port[i], nframes);
  }

  jackd->frames_written+=nframes;

  if (prefs->audio_src==AUDIO_SRC_EXT&&(jackd->playing_file==-1||jackd->playing_file==mainw->ascrap_file)) {
    // TODO - dont apply filters when doing ext window grab, or voiceover

    // in this case we read external audio, but maybe not record it
    // we may wish to analyse the audio for example, or push it to a video generator

    if (has_audio_filters(AF_TYPE_A)) {
      uint64_t tc=jackd->audio_ticks+(uint64_t)(jackd->frames_written/(double)jackd->sample_in_rate*U_SEC);

      if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src==AUDIO_SRC_EXT) {
        // if we have audio triggered gens., push audio to it
        pthread_mutex_lock(&mainw->abuf_frame_mutex);
        for (i=0; i<jackd->num_input_channels; i++) {
          append_to_audio_bufferf(mainw->audio_frame_buffer,in_buffer[i],nframes,i);
        }
        mainw->audio_frame_buffer->samples_filled+=nframes;
        pthread_mutex_unlock(&mainw->abuf_frame_mutex);
      }

      // apply any audio effects with in_channels and no out_channels
      weed_apply_audio_effects_rt(in_buffer,jackd->num_input_channels,nframes,jackd->sample_in_rate,tc,TRUE);
    }


  }

  if (jackd->playing_file==-1||(mainw->record&&mainw->record_paused)) {
    return 0;
  }

  if (jackd->playing_file==-1) out_scale=1.0; // just listening
  else out_scale=(float)jackd->sample_in_rate/(float)afile->arate;

  out_unsigned=afile->signed_endian&AFORM_UNSIGNED;

  frames_out=(int64_t)((double)nframes/out_scale+1.);
  rbytes=frames_out*afile->achans*afile->asampsize/8;

  rbytes=audio_read_inner(jackd,in_buffer,jackd->playing_file,nframes,out_scale,jackd->reverse_endian,
                          out_unsigned,rbytes);


  if (mainw->record&&prefs->audio_src==AUDIO_SRC_EXT&&mainw->ascrap_file!=-1&&mainw->playing_file>0) {
    mainw->files[mainw->playing_file]->aseek_pos+=rbytes;
    if (!mainw->record_paused&&mainw->ascrap_file!=mainw->playing_file) mainw->files[mainw->ascrap_file]->aseek_pos+=rbytes;
    jackd->seek_pos+=rbytes;
  }

  if (mainw->rec_samples==0&&mainw->cancelled==CANCEL_NONE) mainw->cancelled=CANCEL_KEEP; // we wrote the required #

  return 0;
}



int jack_get_srate(nframes_t nframes, void *arg) {
  //lives_printerr("the sample rate is now %ld/sec\n", (int64_t)nframes);
  return 0;
}



void jack_shutdown(void *arg) {
  jack_driver_t *jackd = (jack_driver_t *)arg;

  jackd->client = NULL; /* reset client */
  jackd->jackd_died = TRUE;
  jackd->msgq=NULL;

  lives_printerr("jack shutdown, setting client to 0 and jackd_died to true\n");
  lives_printerr("trying to reconnect right now\n");

  /////////////////////

  jack_audio_init();

  mainw->jackd=jack_get_driver(0,TRUE);
  mainw->jackd->msgq=NULL;

  if (mainw->jackd->playing_file!=-1&&afile!=NULL)
    jack_audio_seek_bytes(mainw->jackd,mainw->jackd->seek_pos); // at least re-seek to the right place
}


static void jack_reset_driver(jack_driver_t *jackd) {
  //lives_printerr("resetting jackd->dev_idx(%d)\n", jackd->dev_idx);
  /* tell the callback that we are to reset, the callback will transition this to STOPPED */
  jackd->state=(jack_transport_state_t)JackTReset;
}


void jack_close_device(jack_driver_t *jackd) {
  int i;

  //lives_printerr("closing the jack client thread\n");
  if (jackd->client) {
    jack_deactivate(jackd->client); /* supposed to help the jack_client_close() to succeed */
    //lives_printerr("after jack_deactivate()\n");
    jack_client_close(jackd->client);
  }

  jack_reset_driver(jackd);
  jackd->client=NULL;

  jackd->is_active=FALSE;


  /* free up the port strings */
  //lives_printerr("freeing up port strings\n");
  if (jackd->jack_port_name_count>1) {
    for (i=0; i<jackd->jack_port_name_count; i++) free(jackd->jack_port_name[i]);
    free(jackd->jack_port_name);
  }
}




static void jack_error_func(const char *desc) {
  lives_printerr("Jack audio error %s\n",desc);
}


// wait 5 seconds to startup
#define JACK_START_WAIT 500000000


// create a new client and connect it to jack, connect the ports
int jack_open_device(jack_driver_t *jackd) {
  const char *client_name="LiVES_audio_out";
  const char *server_name="default";
  jack_options_t options=(jack_options_t)((int)JackServerName|(int)JackNoStartServer);
  jack_status_t status;
  int i;

  int64_t ntime=0,stime;

  if (mainw->aplayer_broken) return 2;

  jackd->is_active=FALSE;

  /* set up an error handler */
  jack_set_error_function(jack_error_func);
  jackd->client=NULL;

  // TODO - use alarm

  stime=lives_get_current_ticks();

  while (jackd->client==NULL&&ntime<JACK_START_WAIT) {
    jackd->client = jack_client_open(client_name, options, &status, server_name);

    lives_usleep(prefs->sleep_time);

    ntime=lives_get_current_ticks()-stime;
  }


  if (jackd->client==NULL) {
    lives_printerr("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status&JackServerFailed) {
      d_print(_("Unable to connect to JACK server\n"));
    }
    return 1;
  }

  if (status&JackNameNotUnique) {
    client_name= jack_get_client_name(jackd->client);
    lives_printerr("unique name `%s' assigned\n", client_name);
  }

  jackd->sample_out_rate=jack_get_sample_rate(jackd->client);

  //lives_printerr (lives_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  for (i=0; i<jackd->num_output_channels; i++) {
    char portname[32];
    lives_snprintf(portname, 32, "out_%d", i);

#ifdef DEBUG_JACK_PORTS
    lives_printerr("output port %d is named '%s'\n", i, portname);
#endif
    jackd->output_port[i] = jack_port_register(jackd->client, portname,
                            JACK_DEFAULT_AUDIO_TYPE,
                            JackPortIsOutput,
                            0);
    if (jackd->output_port[i]==NULL) {
      lives_printerr("no more JACK output ports available\n");
      return 1;
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

  return 0;


}




int jack_open_device_read(jack_driver_t *jackd) {
  // open a device to read audio from jack
  const char *client_name="LiVES_audio_in";
  const char *server_name="default";
  jack_options_t options=(jack_options_t)((int)JackServerName|(int)JackNoStartServer);
  jack_status_t status;
  int i;

  /* set up an error handler */
  jack_set_error_function(jack_error_func);
  jackd->client=NULL;
  while (jackd->client==NULL)
    jackd->client = jack_client_open(client_name, options, &status, server_name);

  if (jackd->client==NULL) {
    lives_printerr("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status&JackServerFailed) {
      d_print(_("Unable to connect to JACK server\n"));
    }
    return 1;
  }

  if (status&JackNameNotUnique) {
    client_name= jack_get_client_name(jackd->client);
    lives_printerr("unique name `%s' assigned\n", client_name);
  }

  jackd->sample_in_rate=jack_get_sample_rate(jackd->client);

  //lives_printerr (lives_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  for (i=0; i<jackd->num_input_channels; i++) {
    char portname[32];
    lives_snprintf(portname, 32, "in_%d", i);

#ifdef DEBUG_JACK_PORTS
    lives_printerr("input port %d is named '%s'\n", i, portname);
#endif
    jackd->input_port[i] = jack_port_register(jackd->client, portname,
                           JACK_DEFAULT_AUDIO_TYPE,
                           JackPortIsInput,
                           0);
    if (jackd->input_port[i]==NULL) {
      lives_printerr("no more JACK input ports available\n");
      return 1;
    }
    jackd->in_chans_available++;
  }

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(jackd->client, jack_get_srate, jackd);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(jackd->client, jack_shutdown, jackd);

  // set process callback and start
  jack_set_process_callback(jackd->client, audio_read, jackd);

  return 0;


}



int jack_driver_activate(jack_driver_t *jackd) {
  // activate client and connect it

  // TODO *** - handle errors here !

  int i;
  const char **ports;
  boolean failed=FALSE;

  if (jackd->is_active) return 0; // already running

  /* tell the JACK server that we are ready to roll */
  if (jack_activate(jackd->client)) {
    //ERR( "cannot activate client\n");
    return 1;
  }

  // we are looking for input ports to connect to
  jackd->jack_port_flags|=JackPortIsInput;

  /* determine how we are to acquire port names */
  if ((jackd->jack_port_name_count==0)||(jackd->jack_port_name_count==1)) {
    if (jackd->jack_port_name_count==0) {
      //lives_printerr("jack_get_ports() passing in NULL/NULL\n");
      ports=jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
    } else {
      //lives_printerr("jack_get_ports() passing in port of '%s'\n", jackd->jack_port_name[0]);
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[0], NULL, jackd->jack_port_flags);
    }

    if (ports==NULL) {
      lives_printerr("No jack ports available !\n");
      return 1;
    }

    /* display a trace of the output ports we found */
#ifdef DEBUG_JACK_PORTS
    for (i=0; ports[i]; i++) lives_printerr("ports[%d] = '%s'\n",i,ports[i]);
#endif

    /* see if we have enough ports */
    if (jackd->out_chans_available<jackd->num_output_channels) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
      return ERR_PORT_NOT_FOUND;
    }

    /* connect the ports. Note: you can't do this before
       the client is activated (this may change in the future). */
    for (i=0; i<jackd->num_output_channels; i++) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("jack_connect() to port %d('%p')\n", i, jackd->output_port[i]);
#endif
      if (jack_connect(jackd->client, jack_port_name(jackd->output_port[i]), ports[i])) {
#ifdef DEBUG_JACK_PORTS
        lives_printerr("cannot connect to output port %d('%s')\n", i, ports[i]);
#endif
        failed=TRUE;
      }
    }
    free(ports); /* free the returned array of ports */
  } else {
    for (i=0; i<jackd->jack_port_name_count; i++) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("jack_get_ports() portname %d of '%s\n", i, jackd->jack_port_name[i]);
#endif
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[i], NULL, jackd->jack_port_flags);
#ifdef DEBUG_JACK_PORTS
      lives_printerr("ports[%d] = '%s'\n", 0, ports[0]);       /* display a trace of the output port we found */
#endif
      if (!ports) {
#ifdef DEBUG_JACK_PORTS
        lives_printerr("jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
        return ERR_PORT_NOT_FOUND;
      }

      /* connect the port */
#ifdef DEBUG_JACK_PORTS
      lives_printerr("jack_connect() to port %d('%p')\n", i, jackd->output_port[i]);
#endif
      if (jack_connect(jackd->client, jack_port_name(jackd->output_port[i]), ports[0])) {
        //ERR("cannot connect to output port %d('%s')\n", 0, ports[0]);
        failed=TRUE;
      }
      free(ports); /* free the returned array of ports */
    }
  }

  jackd->is_active=TRUE;

  /* if something failed we need to shut the client down and return 0 */
  if (failed) {
    lives_printerr("failed, closing and returning error\n");
    jack_close_device(jackd);
    return 1;
  }


  jackd->jackd_died = FALSE;

  jackd->in_use=FALSE;

  jackd->is_paused=FALSE;

  d_print(_("Started jack audio subsystem.\n"));

  return 0;
}






int jack_read_driver_activate(jack_driver_t *jackd, boolean autocon) {
  // connect driver for reading
  int i;
  const char **ports;
  boolean failed=FALSE;

  /* tell the JACK server that we are ready to roll */
  if (jack_activate(jackd->client)) {
    //ERR( "cannot activate client\n");
    return 1;
  }

  if (!autocon && (prefs->jack_opts&JACK_OPTS_NO_READ_AUTOCON)) goto jackreadactive;

  // we are looking for input ports to connect to
  jackd->jack_port_flags|=JackPortIsOutput;

  /* determine how we are to acquire port names */
  if ((jackd->jack_port_name_count==0)||(jackd->jack_port_name_count==1)) {
    if (jackd->jack_port_name_count==0) {
      //lives_printerr("jack_get_ports() passing in NULL/NULL\n");
      ports=jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
    } else {
      //lives_printerr("jack_get_ports() passing in port of '%s'\n", jackd->jack_port_name[0]);
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[0], NULL, jackd->jack_port_flags);
    }

    /* display a trace of the output ports we found */
#ifdef DEBUG_JACK_PORTS
    for (i=0; ports[i]; i++) lives_printerr("ports[%d] = '%s'\n",i,ports[i]);
#endif

    /* see if we have enough ports */
    if (jackd->in_chans_available<jackd->num_input_channels) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
      return ERR_PORT_NOT_FOUND;
    }

    /* connect the ports. Note: you can't do this before
       the client is activated (this may change in the future). */
    for (i=0; i<jackd->num_input_channels; i++) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("jack_connect() to port name %d('%p')\n", i, jackd->input_port[i]);
#endif
      if (jack_connect(jackd->client, ports[i], jack_port_name(jackd->input_port[i]))) {
#ifdef DEBUG_JACK_PORTS
        lives_printerr("cannot connect to input port %d('%s')\n", i, ports[i]);
#endif
        failed=TRUE;
      }
    }
    free(ports); /* free the returned array of ports */
  } else {
    for (i=0; i<jackd->jack_port_name_count; i++) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("jack_get_ports() portname %d of '%s\n", i, jackd->jack_port_name[i]);
#endif
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[i], NULL, jackd->jack_port_flags);
#ifdef DEBUG_JACK_PORTS
      lives_printerr("ports[%d] = '%s'\n", 0, ports[0]);       /* display a trace of the output port we found */
#endif
      if (!ports) {
#ifdef DEBUG_JACK_PORTS
        lives_printerr("jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
        return ERR_PORT_NOT_FOUND;
      }

      /* connect the port */
#ifdef DEBUG_JACK_PORTS
      lives_printerr("jack_connect() to port %d('%p')\n", i, jackd->input_port[i]);
#endif
      if (jack_connect(jackd->client, ports[0], jack_port_name(jackd->input_port[i]))) {
        //ERR("cannot connect to output port %d('%s')\n", 0, ports[0]);
        failed=TRUE;
      }
      free(ports); /* free the returned array of ports */
    }
  }

  /* if something failed we need to shut the client down and return 0 */
  if (failed) {
    lives_printerr("failed, closing and returning error\n");
    jack_close_device(jackd);
    return 1;
  }

jackreadactive:

  jackd->jackd_died = FALSE;

  jackd->in_use=FALSE;

  jackd->is_paused=FALSE;

  jackd->audio_ticks=0;

  d_print(_("Started jack audio reader.\n"));

  return 0;
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



static void jack_reset_dev(int dev_idx, boolean is_output) {
  jack_driver_t *jackd = jack_get_driver(dev_idx,is_output);
  //lives_printerr("resetting dev %d\n", dev_idx);
  jack_reset_driver(jackd);
}


int jack_audio_init(void) {
  // initialise variables
  int i,j;
  jack_driver_t *jackd;

  for (i=0; i<JACK_MAX_OUTDEVICES; i++) {
    jackd = &outdev[i];
    jack_reset_dev(i,TRUE);
    jackd->dev_idx=i;
    jackd->client=NULL;
    jackd->in_use=FALSE;
    for (j=0; j<JACK_MAX_OUTPUT_PORTS; j++) jackd->volume[j]=1.0f;
    jackd->state=(jack_transport_state_t)JackTClosed;
    jackd->sample_out_rate=jackd->sample_in_rate=0;
    jackd->seek_pos=jackd->seek_end=0;
    jackd->msgq=NULL;
    jackd->num_calls=0;
    jackd->astream_fd=-1;
    jackd->jackd_died=FALSE;
    gettimeofday(&jackd->last_reconnect_attempt, 0);
    jackd->num_output_channels=2;
    jackd->play_when_stopped=FALSE;
    jackd->mute=FALSE;
    jackd->is_silent=FALSE;
    jackd->out_chans_available=0;
    jackd->is_output=TRUE;
    jackd->read_abuf=-1;
    jackd->playing_file=-1;
    jackd->frames_written=0;
  }
  return 0;
}



int jack_audio_read_init(void) {
  int i,j;
  jack_driver_t *jackd;

  for (i=0; i<JACK_MAX_INDEVICES; i++) {
    jackd = &indev[i];
    jack_reset_dev(i,FALSE);
    jackd->dev_idx=i;
    jackd->client=NULL;
    jackd->in_use=FALSE;
    for (j=0; j<JACK_MAX_INPUT_PORTS; j++) jackd->volume[j]=1.0f;
    jackd->state=(jack_transport_state_t)JackTClosed;
    jackd->sample_out_rate=jackd->sample_in_rate=0;
    jackd->seek_pos=jackd->seek_end=0;
    jackd->msgq=NULL;
    jackd->num_calls=0;
    jackd->astream_fd=-1;
    jackd->jackd_died=FALSE;
    gettimeofday(&jackd->last_reconnect_attempt, 0);
    jackd->num_input_channels=2;
    jackd->play_when_stopped=FALSE;
    jackd->mute=FALSE;
    jackd->in_chans_available=0;
    jackd->is_output=FALSE;
    jackd->frames_written=0;
  }
  return 0;
}


volatile aserver_message_t *jack_get_msgq(jack_driver_t *jackd) {
  // force update - "volatile" doesn't seem to work...
  char *tmp=lives_strdup_printf("%p %d",jackd->msgq,jackd->jackd_died);
  lives_free(tmp);
  if (jackd->jackd_died||mainw->aplayer_broken) return NULL;
  return jackd->msgq;
}

uint64_t lives_jack_get_time(jack_driver_t *jackd, boolean absolute) {
  // get the time in ticks since either playback started or since last seek

  volatile aserver_message_t *msg=jackd->msgq;
  double frames_written;

  int64_t xtime;

  if (msg!=NULL&&msg->command==ASERVER_CMD_FILE_SEEK) {
    boolean timeout;
    int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
    while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(jackd)!=NULL) {
      sched_yield(); // wait for seek
    }
    if (timeout) return -1;
    lives_alarm_clear(alarm_handle);
  }

  frames_written=jackd->frames_written;
  if (frames_written<0.) frames_written=0.;

  if (jackd->is_output) xtime = jackd->audio_ticks*absolute+(uint64_t)(frames_written/(double)jackd->sample_out_rate*U_SEC);
  else xtime = jackd->audio_ticks*absolute+(uint64_t)(frames_written/(double)jackd->sample_in_rate*U_SEC);
  return xtime;
}


double lives_jack_get_pos(jack_driver_t *jackd) {
  // get current time position (seconds) in audio file
  return jackd->seek_pos/(double)(afile->arate*afile->achans*afile->asampsize/8);
}



boolean jack_audio_seek_frame(jack_driver_t *jackd, int frame) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample

  volatile aserver_message_t *jmsg;
  int64_t seekstart;
  int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
  boolean timeout;

  if (alarm_handle==-1) return FALSE;

  if (frame<1) frame=1;

  do {
    jmsg=jack_get_msgq(jackd);
  } while (!(timeout=lives_alarm_get(alarm_handle))&&jmsg!=NULL&&jmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (timeout||jackd->playing_file==-1) {
    lives_alarm_clear(alarm_handle);
    return FALSE;
  }
  lives_alarm_clear(alarm_handle);
  if (frame>afile->frames) frame=afile->frames;
  seekstart=(int64_t)((double)(frame-1.)/afile->fps*afile->arate)*afile->achans*(afile->asampsize/8);
  jack_audio_seek_bytes(jackd,seekstart);
  return TRUE;
}


int64_t jack_audio_seek_bytes(jack_driver_t *jackd, int64_t bytes) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file

  volatile aserver_message_t *jmsg;
  int64_t seekstart;

  boolean timeout;
  int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);

  seek_err=FALSE;

  if (alarm_handle==-1) {
    LIVES_WARN("Invalid alarm handle");
    return 0;
  }

  do {
    jmsg=jack_get_msgq(jackd);
  } while (!(timeout=lives_alarm_get(alarm_handle))&&jmsg!=NULL&&jmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (timeout||jackd->playing_file==-1) {
    lives_alarm_clear(alarm_handle);
    if (timeout) LIVES_WARN("PA connect timed out");
    seek_err=TRUE;
    return 0;
  }
  lives_alarm_clear(alarm_handle);

  seekstart=((int64_t)(bytes/afile->achans/(afile->asampsize/8)))*afile->achans*(afile->asampsize/8);

  if (seekstart<0) seekstart=0;
  if (seekstart>afile->afilesize) seekstart=afile->afilesize;
  jack_message.command=ASERVER_CMD_FILE_SEEK;
  jack_message.next=NULL;
  jack_message.data=lives_strdup_printf("%ld",seekstart);
  jackd->msgq=&jack_message;
  return seekstart;
}

boolean jack_try_reconnect(void) {

  if (!lives_jack_init()) goto err123;

  jack_audio_init();
  jack_audio_read_init();

  mainw->jackd=jack_get_driver(0,TRUE);

  if (jack_open_device(mainw->jackd)) goto err123;

  d_print(_("\nConnection to jack audio was reset.\n"));
  return TRUE;

err123:
  mainw->aplayer_broken=TRUE;
  mainw->jackd=NULL;
  do_jack_lost_conn_error();
  return FALSE;
}




void jack_aud_pb_ready(int fileno) {
  // TODO - can we merge with switch_audio_clip()

  // prepare to play file fileno
  // - set loop mode
  // - check if we need to reconnect
  // - set vals

  // called at pb start and rec stop (after rec_ext_audio)
  char *tmpfilename=NULL;
  lives_clip_t *sfile=mainw->files[fileno];
  int asigned=!(sfile->signed_endian&AFORM_UNSIGNED);
  int aendian=!(sfile->signed_endian&AFORM_BIG_ENDIAN);

  if (mainw->jackd!=NULL&&mainw->aud_rec_fd==-1) {
    mainw->jackd->is_paused=FALSE;
    mainw->jackd->mute=mainw->mute;
    if (mainw->loop_cont&&!mainw->preview) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS&&mainw->multitrack==NULL)
        mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
      else mainw->jackd->loop=AUDIO_LOOP_FORWARD;
    } else mainw->jackd->loop=AUDIO_LOOP_NONE;
    if (sfile->achans>0&&(!mainw->preview||(mainw->preview&&mainw->is_processing))&&
        (sfile->laudio_time>0.||sfile->opening||
         (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering&&
          lives_file_test((tmpfilename=lives_build_filename(prefs->tmpdir,sfile->handle,"audio",NULL)),
                          LIVES_FILE_TEST_EXISTS)))) {
      boolean timeout;
      int alarm_handle;

      if (tmpfilename!=NULL) lives_free(tmpfilename);
      mainw->jackd->num_input_channels=sfile->achans;
      mainw->jackd->bytes_per_channel=sfile->asampsize/8;
      mainw->jackd->sample_in_rate=sfile->arate;
      mainw->jackd->usigned=!asigned;
      mainw->jackd->seek_end=sfile->afilesize;

      if ((aendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||(!aendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN)))
        mainw->jackd->reverse_endian=TRUE;
      else mainw->jackd->reverse_endian=FALSE;

      alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
        sched_yield(); // wait for seek
      }
      if (timeout) jack_try_reconnect();
      lives_alarm_clear(alarm_handle);

      if ((mainw->multitrack==NULL||mainw->multitrack->is_rendering)&&
          (mainw->event_list==NULL||mainw->record||(mainw->preview&&mainw->is_processing))) {
        // tell jack server to open audio file and start playing it
        jack_message.command=ASERVER_CMD_FILE_OPEN;
        jack_message.data=lives_strdup_printf("%d",fileno);

        jack_message.next=NULL;
        mainw->jackd->msgq=&jack_message;

        jack_audio_seek_bytes(mainw->jackd,sfile->aseek_pos);
        if (seek_err) {
          if (jack_try_reconnect()) jack_audio_seek_bytes(mainw->jackd,sfile->aseek_pos);
        }

        mainw->jackd->in_use=TRUE;
        mainw->rec_aclip=fileno;
        mainw->rec_avel=sfile->pb_fps/sfile->fps;
        mainw->rec_aseek=(double)sfile->aseek_pos/(double)(sfile->arate*sfile->achans*(sfile->asampsize/8));
      }
    }
    if (mainw->agen_key!=0&&mainw->multitrack==NULL) mainw->jackd->in_use=TRUE; // audio generator is active
  }
}









#undef afile

#endif
