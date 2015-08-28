// pulse.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2014
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO

#include "main.h"
#include "callbacks.h"
#include "support.h"
#include "effects.h"
#include "effects-weed.h"

#define afile mainw->files[pulsed->playing_file]

//#define DEBUG_PULSE

static pulse_driver_t pulsed;
static pulse_driver_t pulsed_reader;

static pa_threaded_mainloop *pa_mloop=NULL;
static pa_context *pcon=NULL;

static pa_cvolume out_vol;

static uint32_t pulse_server_rate=0;

#define PULSE_READ_BYTES 48000

static uint8_t prbuf[PULSE_READ_BYTES*2];

static size_t prb=0;

static boolean seek_err;

///////////////////////////////////////////////////////////////////


static void pulse_server_cb(pa_context *c,const pa_server_info *info, void *userdata) {
  if (info==NULL) {
    pulse_server_rate=0;
    return;
  }
  pulse_server_rate=info->sample_spec.rate;
}

// wait 5 seconds to startup
#define PULSE_START_WAIT 500000000


boolean lives_pulse_init(short startup_phase) {
  // startup pulse audio server
  char *msg,*msg2;

  int64_t ntime=0,stime;

  pa_context_state_t pa_state;

  if (pa_mloop!=NULL) return TRUE;

  pa_mloop=pa_threaded_mainloop_new();
  pcon=pa_context_new(pa_threaded_mainloop_get_api(pa_mloop),"LiVES");
  pa_context_connect(pcon,NULL,(pa_context_flags_t)0,NULL);
  pa_threaded_mainloop_start(pa_mloop);

  pa_state=pa_context_get_state(pcon);

  stime=lives_get_current_ticks();

  while (pa_state!=PA_CONTEXT_READY&&ntime<PULSE_START_WAIT) {
    lives_usleep(prefs->sleep_time);
    sched_yield();
    pa_state=pa_context_get_state(pcon);
    ntime=lives_get_current_ticks()-stime;
  }

  if (ntime>=PULSE_START_WAIT) {
    pa_context_unref(pcon);
    pcon=NULL;
    pulse_shutdown();

    LIVES_WARN("Unable to connect to pulseaudio server");

    if (!mainw->foreign) {
      if (startup_phase==0&&capable->has_sox_play) {
        do_error_dialog_with_check(
          _("\nUnable to connect to pulse audio server.\nFalling back to sox audio player.\nYou can change this in Preferences/Playback.\n"),
          WARN_MASK_NO_PULSE_CONNECT);
        switch_aud_to_sox(prefs->warning_mask&WARN_MASK_NO_PULSE_CONNECT);
      } else if (startup_phase==0) {
        if (capable->has_mplayer) {
          do_error_dialog_with_check(
            _("\nUnable to connect to pulse audio server.\nFalling back to mplayer audio player.\nYou can change this in Preferences/Playback.\n"),
            WARN_MASK_NO_PULSE_CONNECT);
          switch_aud_to_mplayer(prefs->warning_mask&WARN_MASK_NO_PULSE_CONNECT);
        } else if (capable->has_mplayer2) {
          do_error_dialog_with_check(
            _("\nUnable to connect to pulse audio server.\nFalling back to mplayer2 audio player.\nYou can change this in Preferences/Playback.\n"),
            WARN_MASK_NO_PULSE_CONNECT);
          switch_aud_to_mplayer2(prefs->warning_mask&WARN_MASK_NO_PULSE_CONNECT);
        }
      } else {
        msg=lives_strdup(_("\nUnable to connect to pulse audio server.\n"));
        if (startup_phase!=2) {
          do_blocking_error_dialog(msg);
          mainw->aplayer_broken=TRUE;
        } else {
          msg2=lives_strdup_printf("%s%s",msg,_("LiVES will exit and you can choose another audio player.\n"));
          do_blocking_error_dialog(msg2);
          lives_free(msg2);
        }
        lives_free(msg);
      }
    }
    return FALSE;
  }

  return TRUE;
}


void pulse_get_rec_avals(pulse_driver_t *pulsed) {
  mainw->rec_aclip=pulsed->playing_file;
  if (mainw->rec_aclip!=-1) {
    mainw->rec_aseek=pulsed->seek_pos/(double)(afile->arate*afile->achans*afile->asampsize/8);
    mainw->rec_avel=afile->pb_fps/afile->fps;
  }
}

static void pulse_set_rec_avals(pulse_driver_t *pulsed, boolean is_forward) {
  // record direction change
  mainw->rec_aclip=pulsed->playing_file;
  if (mainw->rec_aclip!=-1) {
    mainw->rec_avel=ABS(afile->pb_fps/afile->fps);
    if (!is_forward) mainw->rec_avel=-mainw->rec_avel;
    mainw->rec_aseek=(double)pulsed->seek_pos/(double)(afile->arate*afile->achans*afile->asampsize/8);
  }
}


static void pulse_buff_free(void *ptr) {
  lives_free(ptr);
}


static void sample_silence_pulse(pulse_driver_t *pdriver, size_t nbytes, size_t xbytes) {
  uint8_t *buff;
  if (xbytes<=0) return;
  if (mainw->aplayer_broken) return;
  while (nbytes>0) {
    if (nbytes<xbytes) xbytes=nbytes;
    buff=(uint8_t *)lives_try_malloc0(xbytes);
    if (!buff) return;
    if (pdriver->astream_fd!=-1) audio_stream(buff,xbytes,pdriver->astream_fd);
    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
      pthread_mutex_lock(&mainw->abuf_frame_mutex);
      append_to_audio_buffer16(mainw->audio_frame_buffer,buff,xbytes/2,0);
      mainw->audio_frame_buffer->samples_filled+=xbytes/2;
      pthread_mutex_unlock(&mainw->abuf_frame_mutex);
    }
    pa_stream_write(pdriver->pstream,buff,xbytes,pulse_buff_free,0,PA_SEEK_RELATIVE);
    nbytes-=xbytes;
  }
}


static short *shortbuffer=NULL;


static void pulse_audio_write_process_dummy(pa_stream *pstream, size_t nbytes, void *arg) {
  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  uint64_t nframes=nbytes/pulsed->out_achans/(pulsed->out_asamps>>3);
  size_t xbytes=pa_stream_writable_size(pstream);
  sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
  //g_print("pt a1 %ld\n",nframes);
  return;
}


static void pulse_audio_write_process(pa_stream *pstream, size_t nbytes, void *arg) {
  // PULSE AUDIO calls this periodically to get the next audio buffer

  // note: unlike with jack, we have to actually write something with pa_stream_write, even if silent
  // - otherwise the buffer size just increases

  // note also, the buffer size can, and does, change on each call, making it inefficient to use ringbuffers


  static float old_volume=-1.;
  pulse_driver_t *pulsed = (pulse_driver_t *)arg;

  uint64_t nframes=nbytes/pulsed->out_achans/(pulsed->out_asamps>>3);

  aserver_message_t *msg;

  int64_t seek,xseek;
  int new_file;
  char *filename;
  boolean from_memory=FALSE;

  uint8_t *buffer;
  size_t xbytes=pa_stream_writable_size(pstream);

  boolean needs_free=FALSE;

  size_t offs=0;

  pa_volume_t pavol;

  pulsed->pstream=pstream;

  if (xbytes>nbytes) xbytes=nbytes;

  if (!mainw->is_ready||pulsed==NULL||(mainw->playing_file==-1&&pulsed->msgq==NULL)||nbytes>1000000) {
    sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
    //g_print("pt a1 %ld\n",nframes);
    return;
  }

  while ((msg=(aserver_message_t *)pulsed->msgq)!=NULL) {
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file=atoi((char *)msg->data);
      if (pulsed->playing_file!=new_file) {
        if (pulsed->is_opening) filename=lives_build_filename(prefs->tmpdir,mainw->files[new_file]->handle,
                                           "audiodump.pcm",NULL);
        else filename=lives_build_filename(prefs->tmpdir,mainw->files[new_file]->handle,"audio",NULL);
        pulsed->fd=open(filename,O_RDONLY);
        if (pulsed->fd==-1) {
          // dont show gui errors - we are running in realtime thread
          LIVES_ERROR("pulsed: error opening");
          LIVES_ERROR(filename);
          pulsed->playing_file=-1;
        } else {
#ifdef IS_MINGW
          setmode(pulsed->fd, O_BINARY);
#endif
          pulsed->seek_pos=0;
          pulsed->playing_file=new_file;
          pulsed->audio_ticks=mainw->currticks;
          pulsed->frames_written=0;
          pulsed->usec_start=0;
        }
        lives_free(filename);
      }
      break;
    case ASERVER_CMD_FILE_CLOSE:
      if (pulsed->fd>=0) close(pulsed->fd);
      pulsed->fd=-1;
      if (pulsed->aPlayPtr->data!=NULL) lives_free(pulsed->aPlayPtr->data);
      pulsed->aPlayPtr->data=NULL;
      pulsed->aPlayPtr->max_size=0;
      pulsed->aPlayPtr->size=0;
      pulsed->playing_file=-1;
      break;
    case ASERVER_CMD_FILE_SEEK:
      if (pulsed->fd<0) break;
      xseek=seek=atol((char *)msg->data);
      if (seek<0.) xseek=0.;
      if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
        lseek(pulsed->fd,xseek,SEEK_SET);
      }
      pulsed->seek_pos=seek;
      pulsed->audio_ticks=mainw->currticks;
      pulsed->frames_written=0;
      pulsed->usec_start=0;
      break;
    default:
      msg->data=NULL;
    }
    if (msg->data!=NULL) lives_free((livespointer)msg->data);
    msg->command=ASERVER_CMD_PROCESSED;
    if (msg->next==NULL) pulsed->msgq=NULL;
    else pulsed->msgq = msg->next;
  }

  if (pulsed->chunk_size!=nbytes) pulsed->chunk_size = nbytes;

  pulsed->state=pa_context_get_state(pulsed->con);

  if (pulsed->state==PA_CONTEXT_READY) {

    uint64_t pulseFramesAvailable = nframes;
    uint64_t inputFramesAvailable;
    uint64_t numFramesToWrite;
    int64_t in_frames=0;
    uint64_t in_bytes=0,xin_bytes=0;
    float shrink_factor=1.f;
    int swap_sign;

    lives_clip_t *xfile=afile;

#ifdef DEBUG_PULSE
    lives_printerr("playing... pulseFramesAvailable = %ld\n", pulseFramesAvailable);
#endif

    pulsed->num_calls++;

    if (!pulsed->in_use||(((pulsed->fd<0||pulsed->seek_pos<0.)&&pulsed->read_abuf<0)&&
                          ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL))
        ||pulsed->is_paused) {

      sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);

      if (!pulsed->is_paused) pulsed->frames_written+=nframes;
      if (pulsed->seek_pos<0.&&pulsed->playing_file>-1&&afile!=NULL) {
        pulsed->seek_pos+=nframes*afile->achans*afile->asampsize/8;
        if (pulsed->seek_pos>=0) pulse_audio_seek_bytes(pulsed,pulsed->seek_pos);
      }
      //g_print("pt a3 %d\n",pulsed->in_use);
      return;
    }


    if (LIVES_LIKELY(pulseFramesAvailable>0&&(pulsed->read_abuf>-1||
                     (pulsed->aPlayPtr!=NULL
                      &&pulsed->in_achans>0)||
                     ((mainw->agen_key!=0||mainw->agen_needs_reinit)&&mainw->multitrack==NULL)
                                             ))) {

      if (mainw->playing_file>-1&&pulsed->read_abuf>-1) {
        // playing back from memory buffers instead of from file
        // this is used in multitrack
        from_memory=TRUE;

        numFramesToWrite=pulseFramesAvailable;
        pulsed->frames_written+=numFramesToWrite;
        pulseFramesAvailable-=numFramesToWrite;

      } else {
        if (LIVES_LIKELY(pulsed->fd>=0)) {

          if ((pulsed->playing_file==mainw->ascrap_file&&!mainw->preview)&&mainw->playing_file>=-1
              &&mainw->files[mainw->playing_file]->achans>0) {
            xfile=mainw->files[mainw->playing_file];
          }

          pulsed->aPlayPtr->size=0;
          in_bytes=ABS((in_frames=((double)pulsed->in_arate/(double)pulsed->out_arate*
                                   (double)pulseFramesAvailable+((double)fastrand()/(double)LIVES_MAXUINT32))))
                   *pulsed->in_achans*(pulsed->in_asamps>>3);
          //g_print("in bytes=%ld %ld %ld %ld %d %d\n",in_bytes,pulsed->in_arate,pulsed->out_arate,pulseFramesAvailable,pulsed->in_achans,pulsed->in_asamps);
          if ((shrink_factor=(float)in_frames/(float)pulseFramesAvailable)<0.f) {
            // reverse playback
            if ((pulsed->seek_pos-=in_bytes)<0) {
              if (pulsed->loop==AUDIO_LOOP_NONE) {
                if (*pulsed->whentostop==STOP_ON_AUD_END) {
                  *pulsed->cancelled=CANCEL_AUD_END;
                }
                pulsed->in_use=FALSE;
                pulse_set_rec_avals(pulsed,FALSE);
              } else {
                if (pulsed->loop==AUDIO_LOOP_PINGPONG) {
                  pulsed->in_arate=-pulsed->in_arate;
                  shrink_factor=-shrink_factor;
                  pulsed->seek_pos=0;
                  pulse_set_rec_avals(pulsed,TRUE);
                } else pulsed->seek_pos=pulsed->seek_end-in_bytes;
                pulse_set_rec_avals(pulsed,FALSE);
              }
            }
            // rewind by in_bytes
            if ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL)
              lseek(pulsed->fd,pulsed->seek_pos,SEEK_SET);
          }
          if (LIVES_UNLIKELY((in_bytes>pulsed->aPlayPtr->max_size && !(*pulsed->cancelled) && ABS(shrink_factor)<=100.f))) {
            pulsed->aPlayPtr->data=lives_try_realloc(pulsed->aPlayPtr->data,in_bytes);
            if (pulsed->aPlayPtr->data!=NULL) {
              memset(pulsed->aPlayPtr->data,0,in_bytes);
              pulsed->aPlayPtr->max_size=in_bytes;
            }
          } else {
            pulsed->aPlayPtr->max_size=0;
          }
          if (pulsed->mute||(pulsed->aPlayPtr->data==NULL&&((mainw->agen_key==0&&!mainw->agen_needs_reinit)||
                             mainw->multitrack!=NULL))) {
            if (shrink_factor>0.f) pulsed->seek_pos+=in_bytes;
            if (pulsed->seek_pos>=pulsed->seek_end) {
              if (*pulsed->whentostop==STOP_ON_AUD_END) {
                *pulsed->cancelled=CANCEL_AUD_END;
              } else {
                if (pulsed->loop==AUDIO_LOOP_PINGPONG) {
                  pulsed->in_arate=-pulsed->in_arate;
                  pulsed->seek_pos-=in_bytes;
                } else {
                  pulsed->seek_pos=0;
                }
              }
            }
            sample_silence_pulse(pulsed,nframes*pulsed->out_achans*
                                 (pulsed->out_asamps>>3),xbytes);
            pulsed->frames_written+=nframes;
            //g_print("pt a4\n");
            return;
          } else {
            boolean loop_restart;
            do {
              loop_restart=FALSE;
              if (in_bytes>0) {
                // playing from a file
                if (!(*pulsed->cancelled)&&ABS(shrink_factor)<=100.f) {
                  if (((((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL)&&
                        (pulsed->aPlayPtr->size=read(pulsed->fd,pulsed->aPlayPtr->data,in_bytes))==0))||
                      (((mainw->agen_key!=0||mainw->agen_needs_reinit)&&mainw->multitrack==NULL&&
                        pulsed->seek_pos+in_bytes>=xfile->afilesize))) {
                    if (*pulsed->whentostop==STOP_ON_AUD_END) {
                      *pulsed->cancelled=CANCEL_AUD_END;
                    } else {
                      loop_restart=TRUE;
                      if (pulsed->loop==AUDIO_LOOP_PINGPONG) {
                        pulsed->in_arate=-pulsed->in_arate;
                        if ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL)
                          lseek(pulsed->fd,(pulsed->seek_pos-=in_bytes),SEEK_SET);
                        else pulsed->seek_pos-=in_bytes;
                        pulse_set_rec_avals(pulsed,FALSE);
                      } else {
                        if (pulsed->loop!=AUDIO_LOOP_NONE) {
                          seek=0;
                          if ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL)
                            lseek(pulsed->fd,(pulsed->seek_pos=seek),SEEK_SET);
                          else pulsed->seek_pos=seek;
                          pulse_set_rec_avals(pulsed,TRUE);
                        } else {
                          if (mainw->agen_key==0&&!mainw->agen_needs_reinit) pulsed->in_use=FALSE;
                          loop_restart=FALSE;
                          pulse_set_rec_avals(pulsed,TRUE);
                        }
                      }
                    }
                  } else {
                    if (pulsed->aPlayPtr->size<0&&((mainw->agen_key==0&&!mainw->agen_needs_reinit)||
                                                   mainw->multitrack!=NULL)) {
                      // read error...output silence
                      sample_silence_pulse(pulsed,nframes*pulsed->out_achans*
                                           (pulsed->out_asamps>>3),xbytes);
                      if (!pulsed->is_paused) pulsed->frames_written+=nframes;
                      return;
                    }
                    if (shrink_factor<0.f) {
                      // reverse play - rewind again by in_bytes
                      if ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL)
                        lseek(pulsed->fd,pulsed->seek_pos,SEEK_SET);
                    } else {
                      if ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL)
                        pulsed->seek_pos+=pulsed->aPlayPtr->size;
                      else
                        pulsed->seek_pos+=in_bytes;
                    }
                  }
                }
              }
            } while (loop_restart);
          }
          xin_bytes=in_bytes;
        }

        if (mainw->agen_key!=0&&mainw->multitrack==NULL) {
          in_bytes=pulseFramesAvailable*pulsed->out_achans*2;
          if (xin_bytes==0) xin_bytes=in_bytes;
        }

        if (!pulsed->in_use||in_bytes==0||pulsed->mute) {
          // reached end of audio with no looping
          sample_silence_pulse(pulsed,nframes*pulsed->out_achans*
                               (pulsed->out_asamps>>3),xbytes);
          if (!pulsed->is_paused) pulsed->frames_written+=nframes;
          if (pulsed->seek_pos<0.&&pulsed->playing_file>-1&&afile!=NULL) {
            pulsed->seek_pos+=nframes*afile->achans*afile->asampsize/8;
            if (pulsed->seek_pos>=0) pulse_audio_seek_bytes(pulsed,pulsed->seek_pos);
          }
          //g_print("pt a5 %d %d\n",pulsed->in_use,in_bytes);
          return;
        }

        if ((mainw->agen_key==0&&!mainw->agen_needs_reinit)||mainw->multitrack!=NULL) {
          swap_sign=afile->signed_endian&AFORM_UNSIGNED;

          inputFramesAvailable = pulsed->aPlayPtr->size / (pulsed->in_achans * (pulsed->in_asamps>>3));
#ifdef DEBUG_PULSE
          lives_printerr("%ld inputFramesAvailable == %ld, %ld, %ld %ld,pulseFramesAvailable == %ld\n", pulsed->aPlayPtr->size, inputFramesAvailable,
                         in_frames, pulsed->in_arate,pulsed->out_arate,pulseFramesAvailable);
#endif
          buffer = (uint8_t *)pulsed->aPlayPtr->data;

          numFramesToWrite = MIN(pulseFramesAvailable, (inputFramesAvailable/ABS(shrink_factor)+.001));

#ifdef DEBUG_PULSE
          lives_printerr("inputFramesAvailable after conversion %d\n",(uint64_t)((double)inputFramesAvailable/shrink_factor+.001));
          lives_printerr("nframes == %ld, pulseFramesAvailable == %ld,\n\tpulsed->num_input_channels == %ld, pulsed->out_achans == %ld\n",  nframes,
                         pulseFramesAvailable, pulsed->in_achans, pulsed->out_achans);
#endif

          if (pulsed->in_asamps==pulsed->out_asamps&&shrink_factor==1.&&pulsed->in_achans==pulsed->out_achans&&
              !pulsed->reverse_endian&&!swap_sign) {
            // no transformation needed
            pulsed->sound_buffer=buffer;
          } else {
            pulsed->sound_buffer=(uint8_t *)lives_try_malloc0(pulsed->chunk_size);
            if (!pulsed->sound_buffer) {
              sample_silence_pulse(pulsed,nframes*pulsed->out_achans*
                                   (pulsed->out_asamps>>3),xbytes);
              if (!pulsed->is_paused) pulsed->frames_written+=nframes;
              return;
            }

            if (pulsed->in_asamps==8) {
              sample_move_d8_d16((short *)(pulsed->sound_buffer),(uint8_t *)buffer, numFramesToWrite, in_bytes,
                                 shrink_factor, pulsed->out_achans, pulsed->in_achans, swap_sign?SWAP_U_TO_S:0);
            } else {
              sample_move_d16_d16((short *)pulsed->sound_buffer, (short *)buffer, numFramesToWrite, in_bytes, shrink_factor,
                                  pulsed->out_achans, pulsed->in_achans, pulsed->reverse_endian?SWAP_X_TO_L:0,
                                  swap_sign?SWAP_U_TO_S:0);
            }
          }

          if (has_audio_filters(AF_TYPE_ANY)&&(pulsed->playing_file!=mainw->ascrap_file)) {
            boolean memok=TRUE;
            float **fltbuf=(float **)lives_malloc(pulsed->out_achans*sizeof(float *));
            register int i;

            // we have audio filters; convert to float, pass through any audio filters, then back to s16
            for (i=0; i<pulsed->out_achans; i++) {
              // convert s16 to non-interleaved float
              fltbuf[i]=(float *)lives_try_malloc(numFramesToWrite*sizeof(float));
              if (fltbuf[i]==NULL) {
                memok=FALSE;
                for (--i; i>=0; i--) {
                  lives_free(fltbuf[i]);
                }
                break;
              }

              sample_move_d16_float(fltbuf[i],(short *)pulsed->sound_buffer+i,numFramesToWrite,pulsed->out_achans,FALSE,FALSE,1.0);
            }

            if (memok) {
              int64_t tc=pulsed->audio_ticks+(int64_t)(pulsed->frames_written/(double)pulsed->out_arate*U_SEC);
              // apply any audio effects with in_channels

              weed_apply_audio_effects_rt(fltbuf,pulsed->out_achans,numFramesToWrite,pulsed->out_arate,tc,FALSE);

              // convert float audio back to s16
              sample_move_float_int(pulsed->sound_buffer,fltbuf,numFramesToWrite,1.0,pulsed->out_achans,16,0,
                                    (capable->byte_order==LIVES_LITTLE_ENDIAN),FALSE,1.0);

              for (i=0; i<pulsed->out_achans; i++) {
                lives_free(fltbuf[i]);
              }
            }

            lives_free(fltbuf);
          }

        } else {
          // audio generator
          // get float audio from gen, convert it to S16
          float *fbuffer=NULL;
          boolean pl_error=FALSE;
          xbytes=nbytes;
          numFramesToWrite=pulseFramesAvailable;

          if (mainw->agen_needs_reinit) pl_error=TRUE;
          else {
            fbuffer=(float *)lives_malloc(numFramesToWrite*pulsed->out_achans*sizeof(float));
            if (!get_audio_from_plugin(fbuffer,pulsed->out_achans,pulsed->out_arate,numFramesToWrite)) {
              pl_error=TRUE;
            }
          }

          if (!pl_error) {
            if (LIVES_UNLIKELY(nbytes>pulsed->aPlayPtr->max_size)) {
              pulsed->aPlayPtr->data=lives_try_realloc(pulsed->aPlayPtr->data,nbytes);
              if (pulsed->aPlayPtr->data!=NULL) {
                memset(pulsed->aPlayPtr->data,0,nbytes);
                pulsed->aPlayPtr->max_size=nbytes;
              } else pl_error=TRUE;
            }
            pulsed->aPlayPtr->size=nbytes;
          }

          // get back non-interleaved float fbuffer; rate and channels should match
          if (pl_error) nbytes=0;
          else {
            register int i;
            boolean memok=FALSE;
            float **fp=(float **)lives_malloc(pulsed->out_achans*sizeof(float *));
            void *buf;
            pulsed->sound_buffer=(uint8_t *)pulsed->aPlayPtr->data;
            buf=(void *)pulsed->sound_buffer;

            if (has_audio_filters(AF_TYPE_ANY)) {
              register int i;

              memok=TRUE;

              // we have audio filters; convert to float, pass through any audio filters, then back to s16
              for (i=0; i<pulsed->out_achans; i++) {
                // convert s16 to non-interleaved float
                fp[i]=(float *)lives_try_malloc(numFramesToWrite*sizeof(float));
                if (fp[i]==NULL) {
                  memok=FALSE;
                  for (--i; i>=0; i--) {
                    lives_free(fp[i]);
                  }
                  break;
                }

                lives_memcpy(fp[i],&fbuffer[i*numFramesToWrite],numFramesToWrite*sizeof(float));

              }

              if (memok) {
                int64_t tc=pulsed->audio_ticks+(int64_t)(pulsed->frames_written/(double)pulsed->out_arate*U_SEC);
                // apply any audio effects with in_channels

                weed_apply_audio_effects_rt(fp,pulsed->out_achans,numFramesToWrite,pulsed->out_arate,tc,FALSE);

                // convert float audio to s16
                sample_move_float_int(buf,fp,numFramesToWrite,1.0,pulsed->out_achans,16,FALSE,
                                      (capable->byte_order==LIVES_LITTLE_ENDIAN),FALSE,1.0);

                for (i=0; i<pulsed->out_achans; i++) {
                  lives_free(fp[i]);
                }
              }
            }

            if (!memok) {
              // no audio effects; or memory allocation error
              for (i=0; i<pulsed->out_achans; i++) {
                fp[i]=fbuffer+(i*numFramesToWrite);
              }
              sample_move_float_int(buf,fp,numFramesToWrite,1.0,
                                    pulsed->out_achans,16,0,(capable->byte_order==LIVES_LITTLE_ENDIAN),FALSE,1.0);
            }

            if (fbuffer!=NULL) lives_free(fbuffer);
            free(fp);

            if (mainw->record&&mainw->ascrap_file!=-1&&mainw->playing_file>0) {
              // write generated audio to ascrap_file
              size_t rbytes=numFramesToWrite*mainw->files[mainw->ascrap_file]->achans*
                            mainw->files[mainw->ascrap_file]->asampsize>>3;
              pulse_flush_read_data(pulsed,mainw->ascrap_file,nbytes,mainw->files[mainw->ascrap_file]->signed_endian&AFORM_BIG_ENDIAN,buf);
              mainw->files[mainw->ascrap_file]->aseek_pos+=rbytes;
            }
          }
        }

        pulsed->frames_written+=numFramesToWrite;
        pulseFramesAvailable-=numFramesToWrite;

#ifdef DEBUG_PULSE
        lives_printerr("pulseFramesAvailable == %ld\n", pulseFramesAvailable);
#endif
      }

      // playback from memory or file

      if (mainw->volume!=old_volume) {
        pa_operation *pa_op;
        pavol=pa_sw_volume_from_linear(mainw->volume);
        pa_cvolume_set(&out_vol,pulsed->out_achans,pavol);
        pa_op=pa_context_set_sink_input_volume(pulsed->con,pa_stream_get_index(pulsed->pstream),&out_vol,NULL,NULL);
        pa_operation_unref(pa_op);
        old_volume=mainw->volume;
      }


      while (nbytes>0) {
        if (nbytes<xbytes) xbytes=nbytes;

        if (!from_memory) {
          if (xbytes/pulsed->out_achans/(pulsed->out_asamps>>3)<=numFramesToWrite&&offs==0) {
            buffer=pulsed->sound_buffer;
          } else {
            buffer=(uint8_t *)lives_try_malloc(xbytes);
            if (!buffer) {
              sample_silence_pulse(pulsed,nframes*pulsed->out_achans*
                                   (pulsed->out_asamps>>3),xbytes);
              if (!pulsed->is_paused) pulsed->frames_written+=nframes;
              return;
            }
            if (pulsed->sound_buffer) lives_memcpy(buffer,pulsed->sound_buffer+offs,xbytes);
            offs+=xbytes;
            needs_free=TRUE;
          }
          if (pulsed->astream_fd!=-1) audio_stream(buffer,xbytes,pulsed->astream_fd);
	  if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
            pthread_mutex_lock(&mainw->abuf_frame_mutex);
	    append_to_audio_buffer16(mainw->audio_frame_buffer,buffer,xbytes/2,0);
	    mainw->audio_frame_buffer->samples_filled+=xbytes/2;
            pthread_mutex_unlock(&mainw->abuf_frame_mutex);
          }
          pa_stream_write(pulsed->pstream,buffer,xbytes,buffer==pulsed->aPlayPtr->data?NULL:
                          pulse_buff_free,0,PA_SEEK_RELATIVE);
        } else {
          if (pulsed->read_abuf>-1&&!pulsed->mute) {
            shortbuffer=(short *)lives_try_malloc0(xbytes);
            if (!shortbuffer) return;
            sample_move_abuf_int16(shortbuffer,pulsed->out_achans,(xbytes>>1)/pulsed->out_achans,pulsed->out_arate);
            if (pulsed->astream_fd!=-1) audio_stream(shortbuffer,xbytes,pulsed->astream_fd);
	    if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src!=AUDIO_SRC_EXT) {
              pthread_mutex_lock(&mainw->abuf_frame_mutex);
	      append_to_audio_buffer16(mainw->audio_frame_buffer,shortbuffer,xbytes/2,0);
	      mainw->audio_frame_buffer->samples_filled+=xbytes/2;
              pthread_mutex_unlock(&mainw->abuf_frame_mutex);
            }
            pa_stream_write(pulsed->pstream,shortbuffer,xbytes,pulse_buff_free,0,PA_SEEK_RELATIVE);
          } else {
            sample_silence_pulse(pulsed,xbytes,xbytes);
          }
        }
        nbytes-=xbytes;
      }

      if (needs_free&&pulsed->sound_buffer!=pulsed->aPlayPtr->data&&pulsed->sound_buffer!=NULL) {
        lives_free(pulsed->sound_buffer);
        pulsed->sound_buffer=NULL;
      }
    }

    if (pulseFramesAvailable) {

#ifdef DEBUG_PULSE
      lives_printerr("buffer underrun of %ld frames\n", pulseFramesAvailable);
#endif
      xbytes=pa_stream_writable_size(pstream);
      sample_silence_pulse(pulsed,pulseFramesAvailable*pulsed->out_achans
                           *(pulsed->out_asamps>>3),xbytes);
    }
  } else {
#ifdef DEBUG_PULSE
    lives_printerr("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif
    sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
    //g_print("pt a6\n");
  }
  //g_print("pt a7\n");

#ifdef DEBUG_PULSE
  // lives_printerr("done\n");
#endif

}



size_t pulse_flush_read_data(pulse_driver_t *pulsed, int fileno, size_t rbytes, boolean rev_endian, void *data) {
  short *gbuf;
  size_t bytes_out,frames_out,bytes=0;
  void *holding_buff;

  float out_scale;
  int swap_sign;

  lives_clip_t *ofile;


  if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
    if (prb==0||mainw->rec_samples==0) return 0;
    if (prb<=PULSE_READ_BYTES*2&&rbytes>0) {
      gbuf=(short *)data;
    } else {
      gbuf=(short *)lives_try_malloc(prb);
      if (!gbuf) return 0;
      lives_memcpy(gbuf,prbuf,prb-rbytes);
      if (rbytes>0) lives_memcpy(gbuf+(prb-rbytes)/sizeof(short),data,rbytes);
    }
    ofile=afile;
  } else {
    if (rbytes==0) return 0;
    if (fileno==-1) return 0;
    gbuf=(short *)data;
    prb=rbytes;
    ofile=mainw->files[fileno];
  }

  out_scale=(float)pulsed->in_arate/(float)ofile->arate;
  swap_sign=ofile->signed_endian&AFORM_UNSIGNED;

  frames_out=(size_t)((double)((prb/(ofile->asampsize>>3)/ofile->achans))/out_scale+.5);

  if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
    if (frames_out != pulsed->chunk_size) pulsed->chunk_size = frames_out;
  }

  bytes_out=frames_out*ofile->achans*(ofile->asampsize>>3);

  holding_buff=lives_try_malloc(bytes_out);

  if (!holding_buff) return 0;

  if (ofile->asampsize==16) {
    sample_move_d16_d16((short *)holding_buff,gbuf,frames_out,prb,out_scale,ofile->achans,pulsed->in_achans,
                        pulsed->reverse_endian?SWAP_L_TO_X:0,swap_sign?SWAP_S_TO_U:0);
  } else {
    sample_move_d16_d8((uint8_t *)holding_buff,gbuf,frames_out,prb,out_scale,ofile->achans,pulsed->in_achans,swap_sign?SWAP_S_TO_U:0);
  }

  if (gbuf!=(short *)data) {
    lives_free(gbuf);
  }

  prb=0;

  if (mainw->rec_samples>0) {
    if (frames_out>mainw->rec_samples) frames_out=mainw->rec_samples;
    mainw->rec_samples-=frames_out;
  }

  if (mainw->bad_aud_file==NULL) {
    size_t target=frames_out*(ofile->asampsize/8)*ofile->achans,bytes;
    // use write not lives_write - because of potential threading issues
    bytes=write(mainw->aud_rec_fd,holding_buff,target);
    if (bytes<target) mainw->bad_aud_file=filename_from_fd(NULL,mainw->aud_rec_fd);
  }

  lives_free(holding_buff);

  return bytes;
}



static void pulse_audio_read_process(pa_stream *pstream, size_t nbytes, void *arg) {
  // read nframes from pulse buffer, and then possibly write to mainw->aud_rec_fd

  // this is the callback from pulse when we are recording or playing external audio

  pulse_driver_t *pulsed = (pulse_driver_t *)arg;
  float out_scale;
  size_t frames_out,nframes;
  void *data;
  size_t rbytes=nbytes;

  if (!pulsed->in_use) return;

  if (mainw->playing_file<0&&prefs->audio_src==AUDIO_SRC_EXT) return;

  if (mainw->effects_paused) return; // pause during record ???

  pa_stream_peek(pulsed->pstream,(const void **)&data,&rbytes);

  if (data==NULL) return;

  if (pulsed->playing_file==-1) {
    out_scale=1.0; // just listening, no recording
  } else {
    out_scale=(float)pulsed->in_arate/(float)afile->arate; // recording to ascrap_file
    prb+=rbytes;
  }

  if (pulsed->playing_file==-1||(mainw->record&&mainw->record_paused)) prb=0;

  frames_out=(size_t)((double)((prb/(pulsed->in_asamps>>3)/pulsed->in_achans))/out_scale+.5);

  nframes=(size_t)((double)((rbytes/(pulsed->in_asamps>>3)/pulsed->in_achans))/out_scale+.5);

  // should really be frames_read here
  pulsed->frames_written+=nframes;


  if (prefs->audio_src==AUDIO_SRC_EXT&&(pulsed->playing_file==-1||pulsed->playing_file==mainw->ascrap_file)) {
    // - (do not call this when recording ext window or voiceover)

    // in this case we read external audio, but maybe not record it
    // we may wish to analyse the audio for example, or push it to a video generator

    if (has_audio_filters(AF_TYPE_A)) {
      // convert to float, apply any analysers
      boolean memok=TRUE;
      float **fltbuf=(float **)lives_malloc(pulsed->in_achans*sizeof(float *));
      register int i;

      size_t xnframes=(size_t)(rbytes/(pulsed->in_asamps>>3)/pulsed->in_achans);

      for (i=0; i<pulsed->in_achans; i++) {
        // convert s16 to non-interleaved float
        fltbuf[i]=(float *)lives_try_malloc(xnframes*sizeof(float));
        if (fltbuf[i]==NULL) {
          memok=FALSE;
          for (--i; i>=0; i--) {
            lives_free(fltbuf[i]);
          }
          break;
        }

        sample_move_d16_float(fltbuf[i],(short *)data+i,xnframes,pulsed->in_achans,FALSE,FALSE,1.0);

	if (mainw->audio_frame_buffer!=NULL&&prefs->audio_src==AUDIO_SRC_EXT) {
          // if we have audio triggered gens., push audio to it
          pthread_mutex_lock(&mainw->abuf_frame_mutex);
	  append_to_audio_bufferf(mainw->audio_frame_buffer,fltbuf[i],xnframes,i);
	  mainw->audio_frame_buffer->samples_filled+=xnframes;
          pthread_mutex_unlock(&mainw->abuf_frame_mutex);
        }
      }

      if (memok) {
        int64_t tc=pulsed->audio_ticks+(int64_t)(pulsed->frames_written/(double)pulsed->in_arate*U_SEC);
        // apply any audio effects with in channels but no out channels

        weed_apply_audio_effects_rt(fltbuf,pulsed->in_achans,xnframes,pulsed->in_arate,tc,TRUE);

        for (i=0; i<pulsed->in_achans; i++) {
          lives_free(fltbuf[i]);
        }
      }

      lives_free(fltbuf);

    }

  }

  if (pulsed->playing_file==-1||(mainw->record&&mainw->record_paused)) {
    pa_stream_drop(pulsed->pstream);
    if (pulsed->playing_file==-1) {
      pa_stream_flush(pulsed->pstream,NULL,NULL); // if not recording, flush the rest of audio (to reduce latency)
    }
    return;
  }

  if (mainw->playing_file!=mainw->ascrap_file) {
    if (mainw->playing_file!=-1) mainw->files[mainw->playing_file]->aseek_pos+=rbytes;
    if (mainw->ascrap_file!=-1&&!mainw->record_paused) mainw->files[mainw->ascrap_file]->aseek_pos+=rbytes;
  } else if (mainw->ascrap_file!=-1&&!mainw->record_paused) mainw->files[mainw->ascrap_file]->aseek_pos+=rbytes;

  pulsed->seek_pos+=rbytes;


  if (prb<PULSE_READ_BYTES&&(mainw->rec_samples==-1||frames_out<mainw->rec_samples)) {
    // buffer until we have enough
    lives_memcpy(&prbuf[prb-rbytes],data,rbytes);
    pa_stream_drop(pulsed->pstream);
    return;
  }

  if (prb<=PULSE_READ_BYTES*2) {
    lives_memcpy(&prbuf[prb-rbytes],data,rbytes);
    pulse_flush_read_data(pulsed,mainw->playing_file,prb,pulsed->reverse_endian,prbuf);
  } else pulse_flush_read_data(pulsed,mainw->playing_file,rbytes,pulsed->reverse_endian,data);

  pa_stream_drop(pulsed->pstream);

  if (mainw->rec_samples==0&&mainw->cancelled==CANCEL_NONE) {
    mainw->cancelled=CANCEL_KEEP; // we wrote the required #
  }

}


void pulse_shutdown(void) {
  if (pa_mloop!=NULL) pa_threaded_mainloop_stop(pa_mloop);
  if (pcon!=NULL) {
    pa_context_disconnect(pcon);
    pa_context_unref(pcon);
  }
  if (pa_mloop!=NULL) pa_threaded_mainloop_free(pa_mloop);
  pcon=NULL;
  pa_mloop=NULL;
}



void pulse_close_client(pulse_driver_t *pdriver) {
  if (pdriver->pstream!=NULL) {
    pa_threaded_mainloop_lock(pa_mloop);
    pa_stream_set_write_callback(pdriver->pstream,NULL,NULL);
    pa_stream_set_read_callback(pdriver->pstream,NULL,NULL);
    pa_stream_disconnect(pdriver->pstream);
    pa_threaded_mainloop_unlock(pa_mloop);
  }
  if (pdriver->pa_props!=NULL) pa_proplist_free(pdriver->pa_props);
  pdriver->pa_props=NULL;
  pdriver->pstream=NULL;
}




int pulse_audio_init(void) {
  // initialise variables
  int j;

  pulsed.in_use=FALSE;
  pulsed.mloop=pa_mloop;
  pulsed.con=pcon;

  for (j=0; j<PULSE_MAX_OUTPUT_CHANS; j++) pulsed.volume[j]=1.0f;
  pulsed.state=(pa_context_state_t)PA_STREAM_UNCONNECTED;
  pulsed.in_arate=44100;
  pulsed.fd=-1;
  pulsed.seek_pos=pulsed.seek_end=0;
  pulsed.msgq=NULL;
  pulsed.num_calls=0;
  pulsed.chunk_size=0;
  pulsed.astream_fd=-1;
  pulsed.pulsed_died=FALSE;
  pulsed.aPlayPtr=(audio_buffer_t *)lives_malloc(sizeof(audio_buffer_t));
  pulsed.aPlayPtr->data=NULL;
  pulsed.aPlayPtr->size=0;
  pulsed.aPlayPtr->max_size=0;
  gettimeofday(&pulsed.last_reconnect_attempt, 0);
  pulsed.in_achans=2;
  pulsed.out_achans=2;
  pulsed.out_asamps=16;
  pulsed.mute=FALSE;
  pulsed.out_chans_available=PULSE_MAX_OUTPUT_CHANS;
  pulsed.is_output=TRUE;
  pulsed.read_abuf=-1;
  pulsed.is_paused=FALSE;
  pulsed.pstream=NULL;
  pulsed.pa_props=NULL;
  pulsed.playing_file=-1;
  pulsed.sound_buffer=NULL;
  return 0;
}


int pulse_audio_read_init(void) {
  // initialise variables
  int j;

  pulsed_reader.in_use=FALSE;
  pulsed_reader.mloop=pa_mloop;
  pulsed_reader.con=pcon;

  for (j=0; j<PULSE_MAX_OUTPUT_CHANS; j++) pulsed_reader.volume[j]=1.0f;
  pulsed_reader.state=(pa_context_state_t)PA_STREAM_UNCONNECTED;
  pulsed_reader.fd=-1;
  pulsed_reader.seek_pos=pulsed_reader.seek_end=0;
  pulsed_reader.msgq=NULL;
  pulsed_reader.num_calls=0;
  pulsed_reader.chunk_size=0;
  pulsed_reader.astream_fd=-1;
  pulsed_reader.pulsed_died=FALSE;
  gettimeofday(&pulsed_reader.last_reconnect_attempt, 0);
  pulsed_reader.in_achans=2;
  pulsed_reader.in_asamps=16;
  pulsed_reader.mute=FALSE;
  pulsed_reader.is_output=FALSE;
  pulsed_reader.is_paused=FALSE;
  pulsed_reader.pstream=NULL;
  pulsed_reader.pa_props=NULL;
  pulsed_reader.sound_buffer=NULL;

  return 0;
}


void set_process_callback_pulse(pulse_driver_t *pdriver, boolean activate) {
  if (activate) pa_stream_set_write_callback(pdriver->pstream,pulse_audio_write_process,pdriver);
  else pa_stream_set_write_callback(pdriver->pstream,pulse_audio_write_process_dummy,pdriver);
}


int pulse_driver_activate(pulse_driver_t *pdriver) {
  // create a new client and connect it to pulse server
  char *pa_clientname;
  char *mypid;

  pa_sample_spec pa_spec;
  pa_channel_map pa_map;
  pa_buffer_attr pa_battr;

  pa_operation *pa_op;

  pa_volume_t pavol;

  if (pdriver->pstream!=NULL) return 0;

  if (mainw->aplayer_broken) return 2;


  if (pdriver->is_output) {
    pa_clientname="LiVES_audio_out";
  } else {
    pa_clientname="LiVES_audio_in";
  }

  mypid=lives_strdup_printf("%d",capable->mainpid);

  pdriver->pa_props=pa_proplist_new();

  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_ICON_NAME, lives_get_application_name());
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_ID, lives_get_application_name());
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_NAME, lives_get_application_name());

  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_PROCESS_BINARY, capable->myname);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_PROCESS_ID, mypid);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_VERSION, LiVES_VERSION);

#ifdef GUI_GTK
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_LANGUAGE, pango_language_to_string(gtk_get_default_language()));
#endif

#ifdef GUI_QT
  QLocale ql;
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_LANGUAGE, (QLocale::languageToString(ql.language())).toLocal8Bit().constData());
#endif

  pa_channel_map_init_stereo(&pa_map);

  pa_spec.format=PA_SAMPLE_S16NE;

  pa_spec.channels=pdriver->out_achans=pdriver->in_achans;

  pdriver->in_asamps=pdriver->out_asamps=16;
  pdriver->out_signed=0;

  if (capable->byte_order==LIVES_BIG_ENDIAN) pdriver->out_endian=AFORM_BIG_ENDIAN;
  else pdriver->out_endian=AFORM_LITTLE_ENDIAN;

  if (pdriver->is_output) {
    pa_battr.maxlength=LIVES_PA_BUFF_MAXLEN;
    pa_battr.tlength=LIVES_PA_BUFF_TARGET;
  } else {
    pa_battr.maxlength=(uint32_t)-1;
    pa_battr.tlength=(uint32_t)-1;
    pa_battr.fragsize=LIVES_PA_BUFF_FRAGSIZE;
  }

  pa_battr.minreq=(uint32_t)-1;
  pa_battr.prebuf=0;

  if (pulse_server_rate==0) {
    pa_op=pa_context_get_server_info(pdriver->con,pulse_server_cb,NULL);

    while (pa_operation_get_state(pa_op)!=PA_OPERATION_DONE) {
      lives_usleep(prefs->sleep_time);
    }
    pa_operation_unref(pa_op);
  }

  if (pulse_server_rate==0) {
    LIVES_WARN("Problem getting pulseaudio rate...expect more problems ahead.");
    return 1;
  }

  pa_spec.rate=pdriver->out_arate=pdriver->in_arate=pulse_server_rate;

  pdriver->pstream=pa_stream_new_with_proplist(pdriver->con,pa_clientname,&pa_spec,&pa_map,pdriver->pa_props);

  if (pdriver->is_output) {

    pavol=pa_sw_volume_from_linear(mainw->volume);
    pa_cvolume_set(&out_vol,pdriver->out_achans,pavol);

    // set write callback
    set_process_callback_pulse(pdriver,TRUE);

#ifdef PA_STREAM_START_UNMUTED
    pa_stream_connect_playback(pdriver->pstream,NULL,&pa_battr,
                               (pa_stream_flags_t)(PA_STREAM_START_UNMUTED|PA_STREAM_ADJUST_LATENCY|
                                   PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE),
                               &out_vol,NULL);
#else
    pa_stream_connect_playback(pdriver->pstream,NULL,&pa_battr,(pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY|
                               PA_STREAM_INTERPOLATE_TIMING|
                               PA_STREAM_AUTO_TIMING_UPDATE),
                               &out_vol,NULL);
#endif

    while (pa_stream_get_state(pdriver->pstream)!=PA_STREAM_READY) {
      lives_usleep(prefs->sleep_time);
    }
  } else {
    // set read callback
    pdriver->audio_ticks=0;
    pdriver->frames_written=0;
    pdriver->usec_start=0;
    pdriver->in_use=FALSE;
    prb=0;

    pa_stream_set_read_callback(pdriver->pstream,pulse_audio_read_process,pdriver);

    pa_stream_connect_record(pdriver->pstream,NULL,&pa_battr,
                             (pa_stream_flags_t)(PA_STREAM_START_CORKED|PA_STREAM_EARLY_REQUESTS));

    while (pa_stream_get_state(pdriver->pstream)!=PA_STREAM_READY) {
      lives_usleep(prefs->sleep_time);
    }

  }

  lives_free(mypid);

  return 0;
}


void pulse_driver_uncork(pulse_driver_t *pdriver) {
  pa_stream_cork(pdriver->pstream,0,NULL,NULL);
}


///////////////////////////////////////////////////////////////


pulse_driver_t *pulse_get_driver(boolean is_output) {
  if (is_output) return &pulsed;
  return &pulsed_reader;
}



volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *pulsed) {
  // force update - "volatile" doesn't seem to work...
  char *tmp=lives_strdup_printf("%p %d",pulsed->msgq,pulsed->pulsed_died);
  lives_free(tmp);
  if (pulsed->pulsed_died||mainw->aplayer_broken) return NULL;
  return pulsed->msgq;
}



int64_t lives_pulse_get_time(pulse_driver_t *pulsed, boolean absolute) {
  // get the time in ticks since either playback started or since last seek

  volatile aserver_message_t *msg=pulsed->msgq;
  double frames_written;

  int64_t xtime;

  if (msg!=NULL&&msg->command==ASERVER_CMD_FILE_SEEK) {
    boolean timeout;
    int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
    while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(pulsed)!=NULL) {
      sched_yield(); // wait for seek
    }
    if (timeout) return -1;
    lives_alarm_clear(alarm_handle);
  }

  frames_written=pulsed->frames_written;
  if (frames_written<0.) frames_written=0.;

#ifdef USE_PA_INTERP_TIME
  {
    pa_usec_t usec;
    pa_stream_get_time(pulsed->pstream,&usec);
    return pulsed->audio_ticks*absolute+(int64_t)((usec-pulsed->usec_start)*U_SEC_RATIO);
  }
#else
  if (pulsed->is_output) xtime = pulsed->audio_ticks*absolute+(int64_t)(frames_written/(double)pulsed->out_arate*U_SEC);
  else xtime = pulsed->audio_ticks*absolute+(int64_t)(frames_written/(double)afile->arate*U_SEC);
  return xtime;
#endif

}

double lives_pulse_get_pos(pulse_driver_t *pulsed) {
  // get current time position (seconds) in audio file
  return pulsed->seek_pos/(double)(afile->arate*afile->achans*afile->asampsize/8);
}



boolean pulse_audio_seek_frame(pulse_driver_t *pulsed, int frame) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample
  int64_t seekstart;
  volatile aserver_message_t *pmsg;
  int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
  boolean timeout;

  if (alarm_handle==-1) {
    LIVES_WARN("Invalid alarm handle");
    return FALSE;
  }

  if (frame<1) frame=1;

  do {
    pmsg=pulse_get_msgq(pulsed);
  } while (!(timeout=lives_alarm_get(alarm_handle))&&pmsg!=NULL&&pmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (timeout||pulsed->playing_file==-1) {
    if (timeout) LIVES_WARN("PA connect timed out");
    lives_alarm_clear(alarm_handle);
    return FALSE;
  }
  lives_alarm_clear(alarm_handle);
  if (frame>afile->frames) frame=afile->frames;
  seekstart=(int64_t)((double)(frame-1.)/afile->fps*afile->arate)*afile->achans*(afile->asampsize/8);
  pulse_audio_seek_bytes(pulsed,seekstart);
  return TRUE;
}


int64_t pulse_audio_seek_bytes(pulse_driver_t *pulsed, int64_t bytes) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file
  volatile aserver_message_t *pmsg;

  boolean timeout;
  int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);

  int64_t seekstart;

  if (alarm_handle==-1) {
    LIVES_WARN("Invalid alarm handle");
    return 0;
  }

  do {
    pmsg=pulse_get_msgq(pulsed);
  } while (!(timeout=lives_alarm_get(alarm_handle))&&pmsg!=NULL&&pmsg->command!=ASERVER_CMD_FILE_SEEK);

  if (timeout||pulsed->playing_file==-1) {
    lives_alarm_clear(alarm_handle);
    if (timeout) LIVES_WARN("PA connect timed out");
    return 0;
  }
  lives_alarm_clear(alarm_handle);

  seekstart=((int64_t)(bytes/afile->achans/(afile->asampsize/8)))*afile->achans*(afile->asampsize/8);

  if (seekstart<0) seekstart=0;
  if (seekstart>afile->afilesize) seekstart=afile->afilesize;
  pulse_message.command=ASERVER_CMD_FILE_SEEK;
  pulse_message.next=NULL;
  pulse_message.data=lives_strdup_printf("%"PRId64,seekstart);
  pulsed->msgq=&pulse_message;
  return seekstart;
}

boolean pulse_try_reconnect(void) {
  mainw->pulsed=NULL;
  pa_mloop=NULL;
  if (!lives_pulse_init(9999)) goto err123; // init server
  pulse_audio_init(); // reset vars
  pulse_audio_read_init(); // reset vars
  mainw->pulsed=pulse_get_driver(TRUE);
  if (pulse_driver_activate(mainw->pulsed)) { // activate driver
    goto err123;
  }
  d_print(_("\nConnection to pulse audio was reset.\n"));
  return TRUE;

err123:
  mainw->aplayer_broken=TRUE;
  mainw->pulsed=NULL;
  do_pulse_lost_conn_error();
  return FALSE;
}



void pulse_aud_pb_ready(int fileno) {
  // TODO - can we merge with switch_audio_clip() ?

  // prepare to play file fileno
  // - set loop mode
  // - check if we need to reconnect
  // - set vals
  char *tmpfilename=NULL;
  lives_clip_t *sfile=mainw->files[fileno];
  int asigned=!(sfile->signed_endian&AFORM_UNSIGNED);
  int aendian=!(sfile->signed_endian&AFORM_BIG_ENDIAN);


  // called at pb start and rec stop (after rec_ext_audio)
  if (mainw->pulsed!=NULL&&mainw->aud_rec_fd==-1) {
    mainw->pulsed->is_paused=FALSE;
    mainw->pulsed->mute=mainw->mute;
    if (mainw->loop_cont&&!mainw->preview) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS&&mainw->multitrack==NULL)
        mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
      else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;
    } else mainw->pulsed->loop=AUDIO_LOOP_NONE;
    if (sfile->achans>0&&(!mainw->preview||(mainw->preview&&mainw->is_processing))&&
        (sfile->laudio_time>0.||sfile->opening||
         (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering&&
          lives_file_test((tmpfilename=lives_build_filename(prefs->tmpdir,sfile->handle,"audio",NULL)),
                          LIVES_FILE_TEST_EXISTS)))) {

      boolean timeout;
      int alarm_handle;

      if (tmpfilename!=NULL) lives_free(tmpfilename);
      mainw->pulsed->in_achans=sfile->achans;
      mainw->pulsed->in_asamps=sfile->asampsize;
      mainw->pulsed->in_arate=sfile->arate;
      mainw->pulsed->usigned=!asigned;
      mainw->pulsed->seek_end=sfile->afilesize;
      if (sfile->opening) mainw->pulsed->is_opening=TRUE;
      else mainw->pulsed->is_opening=FALSE;

      if ((aendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||(!aendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN)))
        mainw->pulsed->reverse_endian=TRUE;
      else mainw->pulsed->reverse_endian=FALSE;

      alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
        sched_yield(); // wait for seek
      }

      if (timeout) pulse_try_reconnect();

      lives_alarm_clear(alarm_handle);

      if ((mainw->multitrack==NULL||mainw->multitrack->is_rendering||
           sfile->opening)&&(mainw->event_list==NULL||mainw->record||(mainw->preview&&mainw->is_processing))) {
        // tell pulse server to open audio file and start playing it
        pulse_message.command=ASERVER_CMD_FILE_OPEN;
        pulse_message.data=lives_strdup_printf("%d",fileno);
        pulse_message.next=NULL;
        mainw->pulsed->msgq=&pulse_message;

        pulse_audio_seek_bytes(mainw->pulsed,sfile->aseek_pos);
        if (seek_err) {
          if (pulse_try_reconnect()) pulse_audio_seek_bytes(mainw->pulsed,sfile->aseek_pos);
        }
        mainw->pulsed->in_use=TRUE;
        mainw->rec_aclip=fileno;
        mainw->rec_avel=sfile->pb_fps/sfile->fps;
        mainw->rec_aseek=(double)sfile->aseek_pos/(double)(sfile->arate*sfile->achans*(sfile->asampsize/8));
      }
    }
    if (mainw->agen_key!=0&&mainw->multitrack==NULL) mainw->pulsed->in_use=TRUE; // audio generator is active
  }
}

#undef afile

#endif

