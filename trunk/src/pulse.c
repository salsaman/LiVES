// pulse.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2010
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_PULSE_AUDIO

#include "main.h"
#include "callbacks.h"
#include "support.h"

#define afile mainw->files[pulsed->playing_file]

//#define DEBUG_PULSE

static pulse_driver_t pulsed;
static pulse_driver_t pulsed_reader;

static pa_threaded_mainloop *pa_mloop=NULL;
static pa_context *pcon=NULL;

static pa_cvolume out_vol;

static guint pulse_server_rate=0;

#define PULSE_READ_BYTES 48000

static guchar prbuf[PULSE_READ_BYTES];

static size_t prb=0;

///////////////////////////////////////////////////////////////////


static void pulse_server_cb(pa_context *c,const pa_server_info *info, void *userdata) {
  pulse_server_rate=info->sample_spec.rate;
}

// wait 5 seconds to startup
#define PULSE_START_WAIT 5000000


gboolean lives_pulse_init (short startup_phase) {
  // startup pulse audio server
  gchar *msg,*msg2;

  struct timeval otv;
  int64_t ntime=0,stime;

  pa_context_state_t pa_state;

  if (pa_mloop!=NULL) return TRUE;

  pa_mloop=pa_threaded_mainloop_new();
  pcon=pa_context_new(pa_threaded_mainloop_get_api(pa_mloop),"LiVES");
  pa_context_connect(pcon,NULL,0,NULL);
  pa_threaded_mainloop_start(pa_mloop);

  pa_state=pa_context_get_state(pcon);

  gettimeofday(&otv, NULL);
  stime=otv.tv_sec*1000000000+otv.tv_usec;

  while (pa_state!=PA_CONTEXT_READY&&ntime<PULSE_START_WAIT) {
    g_usleep(prefs->sleep_time);
    sched_yield();
    pa_state=pa_context_get_state(pcon);
    gettimeofday(&otv, NULL);
    ntime=(otv.tv_sec*1000000000+otv.tv_usec-stime)>>10;
  }

  if (ntime>=PULSE_START_WAIT) {
    pa_context_unref(pcon);
    pcon=NULL;
    pulse_shutdown();

    g_printerr("%s",_("\nUnable to connect to pulse audio server\n"));
    if (startup_phase==0&&capable->has_sox) {
      do_error_dialog(_("\nUnable to connect to pulse audio server.\nFalling back to sox audio player.\nYou can change this in Preferences/Playback.\n"));
      switch_aud_to_sox();
    }
    else if (startup_phase==0&&capable->has_mplayer) {
      do_error_dialog(_("\nUnable to connect to pulse audio server.\nFalling back to mplayer audio player.\nYou can change this in Preferences/Playback.\n"));
      switch_aud_to_mplayer();
    }
    else {
      msg=g_strdup(_("\nUnable to connect to pulse audio server.\n"));
      if (startup_phase==0) {
	do_error_dialog(msg);
      }
      else {
	msg2=g_strdup_printf("%s%s",msg,_("LiVES will exit and you can choose another audio player.\n"));
	do_blocking_error_dialog(msg2);
	g_free(msg2);
      }
      g_free(msg);
    }
    return FALSE;
  }

  return TRUE;
}


void pulse_get_rec_avals(pulse_driver_t *pulsed) {
  mainw->rec_aclip=pulsed->playing_file;
  if (mainw->rec_aclip!=-1) {
    mainw->rec_aseek=pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
    mainw->rec_avel=afile->pb_fps/afile->fps;
  }
}


static void pulse_buff_free(void *ptr) {
  g_free(ptr);
}


static void sample_silence_pulse (pulse_driver_t *pdriver, size_t nbytes, size_t xbytes) {
  guchar *buff;
  while (nbytes>0) {
    if (nbytes<xbytes) xbytes=nbytes;
    buff=g_malloc0(xbytes);
    pa_stream_write(pdriver->pstream,buff,xbytes,pulse_buff_free,0,PA_SEEK_RELATIVE);
    nbytes-=xbytes;
  }
}


static void pulse_audio_write_process (pa_stream *pstream, size_t nbytes, void *arg) {
  // PULSE AUDIO calls this periodically to get the next audio buffer
  static float old_volume=-1.;
  pulse_driver_t* pulsed = (pulse_driver_t*)arg;

  gulong nframes=nbytes/pulsed->out_achans/(pulsed->out_asamps>>3);

  aserver_message_t *msg;

  long seek,xseek;
  int new_file;
  gchar *filename;
  gboolean from_memory=FALSE;

  guchar *buffer;
  size_t xbytes=pa_stream_writable_size(pstream);

  gboolean needs_free=FALSE;

  size_t offs=0;

  pa_volume_t pavol;

  if (xbytes>nbytes) xbytes=nbytes;

  if (!mainw->is_ready||pulsed==NULL||(mainw->playing_file==-1&&pulsed->msgq==NULL)) {
    sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
    //g_print("pt a1 %ld\n",nframes);
    return;
  }

  if ((msg=(aserver_message_t *)pulsed->msgq)!=NULL) {

    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file=atoi(msg->data);
      if (pulsed->playing_file!=new_file) {
	if (pulsed->is_opening) filename=g_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,mainw->files[new_file]->handle);
	else filename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,mainw->files[new_file]->handle);
	pulsed->fd=open(filename,O_RDONLY);
	if (pulsed->fd==-1) {
	  g_printerr("pulsed: error opening %s\n",filename);
	  pulsed->playing_file=-1;
	}
	if (pulsed->fd>0) {
	  if (pulsed->aPlayPtr->data!=NULL) g_free(pulsed->aPlayPtr->data);
	  if (nbytes>0) pulsed->aPlayPtr->data=g_malloc(nbytes*100);
	  else (pulsed->aPlayPtr->data)=NULL;
	  pulsed->seek_pos=0;
	  pulsed->playing_file=new_file;
	}
	g_free(filename);
      }
      break;
    case ASERVER_CMD_FILE_CLOSE:
      if (pulsed->fd>0) close(pulsed->fd);
      pulsed->fd=0;
      if (pulsed->aPlayPtr->data!=NULL) g_free(pulsed->aPlayPtr->data);
      pulsed->aPlayPtr->data=NULL;
      pulsed->aPlayPtr->size=0;
      pulsed->playing_file=-1;
      break;
    case ASERVER_CMD_FILE_SEEK:
      if (pulsed->fd<1) break;
      xseek=seek=atol(msg->data);
      if (seek<0.) xseek=0.;
      if (!pulsed->mute) {
	lseek(pulsed->fd,xseek,SEEK_SET);
      }
      pulsed->seek_pos=seek;
      gettimeofday(&tv, NULL);
      pulsed->audio_ticks=U_SECL*(tv.tv_sec-mainw->startsecs)+tv.tv_usec*U_SEC_RATIO;
      pulsed->frames_written=0;
      break;
    default:
      msg->data=NULL;
    }
    if (msg->data!=NULL) g_free(msg->data);
    msg->command=ASERVER_CMD_PROCESSED;
    pulsed->msgq = NULL;
  }

  if (pulsed->chunk_size!=nbytes) pulsed->chunk_size = nbytes;

  pulsed->state=pa_context_get_state(pulsed->con);

  if (pulsed->state==PA_CONTEXT_READY) {
    gulong pulseFramesAvailable = nframes;
    gulong inputFramesAvailable;
    gulong numFramesToWrite;
    glong in_frames=0;
    gulong in_bytes=0;
    gfloat shrink_factor=1.f;
    int swap_sign;

#ifdef DEBUG_PULSE
    g_printerr("playing... pulseFramesAvailable = %ld\n", pulseFramesAvailable);
#endif

    pulsed->num_calls++;

    if (!pulsed->in_use||((pulsed->fd<1||pulsed->seek_pos<0.)&&pulsed->read_abuf<0)||pulsed->is_paused) {

      sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);

      if (!pulsed->is_paused) pulsed->frames_written+=nframes;
      if (pulsed->seek_pos<0.&&pulsed->playing_file>-1&&afile!=NULL) {
	pulsed->seek_pos+=nframes*afile->achans*afile->asampsize/8;
	if (pulsed->seek_pos>=0) pulse_audio_seek_bytes(pulsed,pulsed->seek_pos);
      }
      //g_print("pt a3\n");
      return;
    }

    if (G_LIKELY(pulseFramesAvailable>0&&(pulsed->read_abuf>-1||(pulsed->aPlayPtr!=NULL&&pulsed->aPlayPtr->data!=NULL&&pulsed->in_achans>0)))) {

      if (mainw->playing_file>-1&&pulsed->read_abuf>-1) {
	// playing back from memory buffers instead of from file
	// this is used in multitrack
	from_memory=TRUE;

	numFramesToWrite=pulseFramesAvailable;
	pulsed->frames_written+=numFramesToWrite;
	pulseFramesAvailable-=numFramesToWrite;

      }
      else {
	if (G_LIKELY(pulsed->fd>0)) {
	  pulsed->aPlayPtr->size=0;
	  in_bytes=ABS((in_frames=((gdouble)pulsed->in_arate/(gdouble)pulsed->out_arate*(gdouble)pulseFramesAvailable+((gdouble)fastrand()/(gdouble)G_MAXUINT32))))*pulsed->in_achans*(pulsed->in_asamps>>3);
	  //g_print("in bytes=%ld %ld %ld %ld %d %d\n",in_bytes,pulsed->in_arate,pulsed->out_arate,pulseFramesAvailable,pulsed->in_achans,pulsed->in_asamps);
	  if ((shrink_factor=(gfloat)in_frames/(gfloat)pulseFramesAvailable)<0.f) {
	    // reverse playback
	    if ((pulsed->seek_pos-=in_bytes)<0) {
	      if (pulsed->loop==AUDIO_LOOP_NONE) {
		if (*pulsed->whentostop==STOP_ON_AUD_END) {
		  *pulsed->cancelled=CANCEL_AUD_END;
		}
		pulsed->in_use=FALSE;
		mainw->rec_aclip=pulsed->playing_file;
		mainw->rec_avel=-afile->pb_fps/afile->fps;
		mainw->rec_aseek=(gdouble)pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
	      }
	      else {
		if (pulsed->loop==AUDIO_LOOP_PINGPONG) {
		  pulsed->in_arate=-pulsed->in_arate;
		  shrink_factor=-shrink_factor;
		  pulsed->seek_pos=0;
		}
		else pulsed->seek_pos=pulsed->seek_end-in_bytes;
		mainw->rec_aclip=pulsed->playing_file;
		mainw->rec_avel=-afile->pb_fps/afile->fps;
		mainw->rec_aseek=(gdouble)pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
	      }
	    }
	    // rewind by in_bytes
	    lseek(pulsed->fd,pulsed->seek_pos,SEEK_SET);
	  }
	  if (pulsed->mute) {
	    if (shrink_factor>0.f) pulsed->seek_pos+=in_bytes;
	    if (pulsed->seek_pos>=pulsed->seek_end) {
	      if (*pulsed->whentostop==STOP_ON_AUD_END) {
		*pulsed->cancelled=CANCEL_AUD_END;
	      }
	      else {
		if (pulsed->loop==AUDIO_LOOP_PINGPONG) {
		  pulsed->in_arate=-pulsed->in_arate;
		  pulsed->seek_pos-=in_bytes;
		}
		else {
		  seek=0;
		  pulsed->seek_pos=seek;
		}
	      }
	    }
	    sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
	    pulsed->frames_written+=nframes;
	    //g_print("pt a4\n");
	    return;
	  }
	  else {
	    gboolean loop_restart;
	    do {
	      loop_restart=FALSE;
	      if (in_bytes>0) {
		// playing from a file
		if (!(*pulsed->cancelled)&&ABS(shrink_factor)<=100.f) {
		  if ((pulsed->aPlayPtr->size=read(pulsed->fd,pulsed->aPlayPtr->data,in_bytes))==0) {
		    if (*pulsed->whentostop==STOP_ON_AUD_END) {
		      *pulsed->cancelled=CANCEL_AUD_END;
		    }
		    else {
		      loop_restart=TRUE;
		      if (pulsed->loop==AUDIO_LOOP_PINGPONG) {
			pulsed->in_arate=-pulsed->in_arate;
			lseek(pulsed->fd,(pulsed->seek_pos-=in_bytes),SEEK_SET);
			mainw->rec_aclip=pulsed->playing_file;
			mainw->rec_avel=-afile->pb_fps/afile->fps;
			mainw->rec_aseek=(gdouble)pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
		      }
		      else {
			if (pulsed->loop!=AUDIO_LOOP_NONE) {
			  seek=0;
			  lseek(pulsed->fd,(pulsed->seek_pos=seek),SEEK_SET);
			  mainw->rec_aclip=pulsed->playing_file;
			  mainw->rec_avel=-afile->pb_fps/afile->fps;
			  mainw->rec_aseek=(gdouble)pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
			}
			else {
			  pulsed->in_use=FALSE;
			  loop_restart=FALSE;
			  mainw->rec_aclip=pulsed->playing_file;
			  mainw->rec_avel=-afile->pb_fps/afile->fps;
			  mainw->rec_aseek=(gdouble)pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
			}
		      }
		    }
		  }
		  else {
		    if (shrink_factor<0.f) {
		      // reverse play - rewind again by in_bytes
		      lseek(pulsed->fd,pulsed->seek_pos,SEEK_SET);
		    }
		    else pulsed->seek_pos+=pulsed->aPlayPtr->size;
		  }
		}
	      }
	    } while (loop_restart);
	  }
	}


	if (!pulsed->in_use||in_bytes==0) {
	  // reached end of audio with no looping
	  sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
	  if (!pulsed->is_paused) pulsed->frames_written+=nframes;
	  if (pulsed->seek_pos<0.&&pulsed->playing_file>-1&&afile!=NULL) {
	    pulsed->seek_pos+=nframes*afile->achans*afile->asampsize/8;
	    if (pulsed->seek_pos>=0) pulse_audio_seek_bytes(pulsed,pulsed->seek_pos);
	  }
	  //g_print("pt a5 %d %d\n",pulsed->in_use,in_bytes);
	  return;
	}

	inputFramesAvailable = pulsed->aPlayPtr->size / (pulsed->in_achans * (pulsed->in_asamps>>3));
#ifdef DEBUG_PULSE
	g_printerr("%ld inputFramesAvailable == %ld, %ld, %ld %ld,pulseFramesAvailable == %ld\n", pulsed->aPlayPtr->size, inputFramesAvailable, in_frames, pulsed->in_arate,pulsed->out_arate,pulseFramesAvailable);
#endif
	buffer = pulsed->aPlayPtr->data;
    
	numFramesToWrite = MIN(pulseFramesAvailable, inputFramesAvailable/ABS(shrink_factor));
    
#ifdef DEBUG_PULSE
    g_printerr("inputFramesAvailable after conversion %ld\n", (gulong)((gdouble)inputFramesAvailable/shrink_factor));
    g_printerr("nframes == %ld, pulseFramesAvailable == %ld,\n\tpulsed->num_input_channels == %ld, pulsed->out_achans == %ld\n",  nframes, pulseFramesAvailable, pulsed->in_achans, pulsed->out_achans);
#endif
    
    swap_sign=afile->signed_endian&AFORM_UNSIGNED;

    if (pulsed->in_asamps==pulsed->out_asamps&&shrink_factor==1.&&pulsed->in_achans==pulsed->out_achans&&!pulsed->reverse_endian&&!swap_sign) {
      // no transformation needed
      pulsed->sound_buffer=buffer;
    }
    else {
      pulsed->sound_buffer=g_malloc0(pulsed->chunk_size);

      if (pulsed->in_asamps==8) {
	sample_move_d8_d16 ((short *)(pulsed->sound_buffer),(guchar *)buffer, numFramesToWrite, in_bytes, shrink_factor, pulsed->out_achans, pulsed->in_achans, swap_sign?SWAP_U_TO_S:0);
      }
      else {
	sample_move_d16_d16((short*)pulsed->sound_buffer, (short*)buffer, numFramesToWrite, in_bytes, shrink_factor, pulsed->out_achans, pulsed->in_achans, pulsed->reverse_endian?SWAP_X_TO_L:0, swap_sign?SWAP_U_TO_S:0);
      }
    }

    pulsed->frames_written+=numFramesToWrite;
    pulseFramesAvailable-=numFramesToWrite;
    
#ifdef DEBUG_PULSE
    g_printerr("pulseFramesAvailable == %ld\n", pulseFramesAvailable);
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
      }
      else {
	buffer=g_malloc(xbytes);
	w_memcpy(buffer,pulsed->sound_buffer+offs,xbytes);
	offs+=xbytes;
	needs_free=TRUE;
      }
      pa_stream_write(pulsed->pstream,buffer,xbytes,buffer==pulsed->aPlayPtr->data?NULL:pulse_buff_free,0,PA_SEEK_RELATIVE);
    }
    else {
      if (pulsed->read_abuf>-1&&!pulsed->mute) {
	buffer=g_malloc(xbytes);
	sample_move_abuf_int16((short *)buffer,pulsed->out_achans,(xbytes>>1)/pulsed->out_achans,pulsed->out_arate);
	pa_stream_write(pulsed->pstream,buffer,xbytes,pulse_buff_free,0,PA_SEEK_RELATIVE);
      }
      else {
	sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
      }
    }
    nbytes-=xbytes;
  }

  if (needs_free&&pulsed->sound_buffer!=pulsed->aPlayPtr->data) g_free(pulsed->sound_buffer);

    }
       
    if(pulseFramesAvailable) {
#ifdef DEBUG_PULSE
      g_printerr("buffer underrun of %ld frames\n", pulseFramesAvailable);
#endif
      sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
    }
  }
  else {
#ifdef DEBUG_PULSE
    g_printerr("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif
    sample_silence_pulse(pulsed,nframes*pulsed->out_achans*(pulsed->out_asamps>>3),xbytes);
    //g_print("pt a6\n");
  }
  //g_print("pt a7\n");

#ifdef DEBUG_PULSE
  g_printerr("done\n");
#endif

}



void pulse_flush_read_data(pulse_driver_t *pulsed, size_t rbytes, void *data) {
  short *gbuf;
  size_t bytes_out,frames_out;
  void *holding_buff;
  float out_scale=(float)pulsed->in_arate/(float)afile->arate;

  int swap_sign=afile->signed_endian&AFORM_UNSIGNED;

  if (prb==0) return;

  gbuf=(short *)g_malloc(prb);

  w_memcpy(gbuf,prbuf,prb-rbytes);
  if (rbytes>0) w_memcpy(gbuf+(prb-rbytes)/sizeof(short),data,rbytes);

  frames_out=(size_t)((gdouble)((prb/(pulsed->in_asamps>>3)/pulsed->in_achans))/out_scale+.5);

  bytes_out=frames_out*afile->achans*(afile->asampsize>>3);

  holding_buff=malloc(bytes_out);

  if (frames_out != pulsed->chunk_size) pulsed->chunk_size = frames_out;

  if (afile->asampsize==16) {
    sample_move_d16_d16((short *)holding_buff,gbuf,frames_out,prb,out_scale,afile->achans,pulsed->in_achans,pulsed->reverse_endian?SWAP_L_TO_X:0,swap_sign?SWAP_S_TO_U:0);
  }
  else {
    sample_move_d16_d8((uint8_t *)holding_buff,gbuf,frames_out,prb,out_scale,afile->achans,pulsed->in_achans,swap_sign?SWAP_S_TO_U:0);
  }
    
  pulsed->frames_written+=frames_out;

  g_free(gbuf);
  prb=0;

  if (mainw->rec_samples>0) {
    if (frames_out>mainw->rec_samples) frames_out=mainw->rec_samples;
    mainw->rec_samples-=frames_out;
  }

  dummyvar=write (mainw->aud_rec_fd,holding_buff,frames_out*(afile->asampsize/8)*afile->achans);

  g_free(holding_buff);

}



static void pulse_audio_read_process (pa_stream *pstream, size_t nbytes, void *arg) {
  // read nframes from pulse buffer, and then write to mainw->aud_rec_fd
  pulse_driver_t* pulsed = (pulse_driver_t*)arg;
  float out_scale=(float)pulsed->in_arate/(float)afile->arate;
  size_t frames_out;
  void *data;
  size_t rbytes;

  if (mainw->effects_paused) return; // pause during record

  pa_stream_peek(pulsed->pstream,(const void**)&data,&rbytes);

  prb+=rbytes;

  frames_out=(size_t)((gdouble)((prb/(pulsed->in_asamps>>3)/pulsed->in_achans))/out_scale+.5);
  
  if (prb<PULSE_READ_BYTES&&frames_out<mainw->rec_samples) {
    // buffer until we have enough
    w_memcpy(&prbuf[prb-rbytes],data,rbytes);
    pa_stream_drop(pulsed->pstream);
    return;
  }

  pulse_flush_read_data(pulsed,rbytes,data);

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
  pa_threaded_mainloop_lock(pa_mloop);
  pa_stream_set_write_callback(pdriver->pstream,NULL,NULL);
  pa_stream_set_read_callback(pdriver->pstream,NULL,NULL);
  if (pdriver->pstream!=NULL) pa_stream_disconnect(pdriver->pstream);
  pa_threaded_mainloop_unlock(pa_mloop);
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

  for (j=0;j<PULSE_MAX_OUTPUT_CHANS;j++) pulsed.volume[j]=1.0f;
  pulsed.state=PA_STREAM_UNCONNECTED;
  pulsed.in_arate=44100;
  pulsed.fd=0;
  pulsed.seek_pos=pulsed.seek_end=0;
  pulsed.msgq=NULL;
  pulsed.num_calls=0;
  pulsed.chunk_size=0;
  pulsed.pulsed_died=FALSE;
  pulsed.aPlayPtr=(audio_buffer_t *)g_malloc(sizeof(audio_buffer_t));
  pulsed.aPlayPtr->data=NULL;
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
  return 0;
}


int pulse_audio_read_init(void) {
  // initialise variables
  int j;

  pulsed_reader.in_use=FALSE;
  pulsed_reader.mloop=pa_mloop;
  pulsed_reader.con=pcon;

  for (j=0;j<PULSE_MAX_OUTPUT_CHANS;j++) pulsed_reader.volume[j]=1.0f;
  pulsed_reader.state=PA_STREAM_UNCONNECTED;
  pulsed_reader.fd=0;
  pulsed_reader.seek_pos=pulsed_reader.seek_end=0;
  pulsed_reader.msgq=NULL;
  pulsed_reader.num_calls=0;
  pulsed_reader.chunk_size=0;
  pulsed_reader.pulsed_died=FALSE;
  gettimeofday(&pulsed_reader.last_reconnect_attempt, 0);
  pulsed_reader.in_achans=2;
  pulsed_reader.in_asamps=16;
  pulsed_reader.mute=FALSE;
  pulsed_reader.is_output=FALSE;
  pulsed_reader.is_paused=FALSE;
  pulsed_reader.pstream=NULL;
  pulsed_reader.pa_props=NULL;

  return 0;
}




int pulse_driver_activate(pulse_driver_t *pdriver) {
// create a new client and connect it to pulse server
  gchar *pa_clientname="LiVES_audio_out";
  gchar *mypid;

  pa_sample_spec pa_spec;
  pa_channel_map pa_map;
  pa_buffer_attr pa_battr;

  pa_operation *pa_op;

  pa_volume_t pavol;

  if (pdriver->pstream!=NULL) return 0;

  mypid=g_strdup_printf("%d",getpid());

  pdriver->pa_props=pa_proplist_new();

  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_ICON_NAME, "LiVES");
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_ID, "LiVES");
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_NAME, "LiVES");
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_PROCESS_BINARY, capable->myname);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_PROCESS_ID, mypid);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_VERSION, LiVES_VERSION);
  pa_proplist_sets(pdriver->pa_props, PA_PROP_APPLICATION_LANGUAGE, pango_language_to_string(gtk_get_default_language()));

  pa_channel_map_init_stereo(&pa_map);

  pa_spec.format=PA_SAMPLE_S16NE;

  pa_spec.channels=pdriver->out_achans=pdriver->in_achans;

  pdriver->in_asamps=pdriver->out_asamps=16;
  pdriver->out_signed=0;

  if (G_BYTE_ORDER==G_BIG_ENDIAN) pdriver->out_endian=AFORM_BIG_ENDIAN;
  else pdriver->out_endian=AFORM_LITTLE_ENDIAN;

  pa_battr.maxlength=LIVES_PA_BUFF_MAXLEN;
  pa_battr.minreq=(uint32_t)-1;
  pa_battr.tlength=LIVES_PA_BUFF_TARGET;
  pa_battr.prebuf=0;

  if (pulse_server_rate==0) {
    pa_op=pa_context_get_server_info(pdriver->con,pulse_server_cb,NULL);
    
    while (pa_operation_get_state(pa_op)!=PA_OPERATION_DONE) {
      g_usleep(prefs->sleep_time);
    }
    pa_operation_unref(pa_op);
  }


  pa_spec.rate=pdriver->out_arate=pdriver->in_arate=pulse_server_rate;

  pdriver->pstream=pa_stream_new_with_proplist(pdriver->con,pa_clientname,&pa_spec,&pa_map,pdriver->pa_props);

  if (pdriver->is_output) {

    pavol=pa_sw_volume_from_linear(mainw->volume);
    pa_cvolume_set(&out_vol,pdriver->out_achans,pavol);

    // set write callback
    pa_stream_set_write_callback(pdriver->pstream,pulse_audio_write_process,pdriver);

#ifdef PA_STREAM_START_UNMUTED
    pa_stream_connect_playback(pdriver->pstream,NULL,&pa_battr,PA_STREAM_START_UNMUTED|PA_STREAM_ADJUST_LATENCY,&out_vol,NULL);
#else
    pa_stream_connect_playback(pdriver->pstream,NULL,&pa_battr,PA_STREAM_ADJUST_LATENCY,&out_vol,NULL);
#endif

    while (pa_stream_get_state(pdriver->pstream)!=PA_STREAM_READY) {
      g_usleep(prefs->sleep_time);
    }
  }
  else {
    // set read callback
    pa_stream_set_read_callback(pdriver->pstream,pulse_audio_read_process,pdriver);

    pa_stream_connect_record(pdriver->pstream,NULL,&pa_battr,0);

    prb=0;

    while (pa_stream_get_state(pdriver->pstream)!=PA_STREAM_READY) {
      g_usleep(prefs->sleep_time);
    }

  }

  g_free(mypid);

  return 0;
}


///////////////////////////////////////////////////////////////


pulse_driver_t *pulse_get_driver(gboolean is_output) {
  if (is_output) return &pulsed;
  return &pulsed_reader;
}



volatile aserver_message_t *pulse_get_msgq(pulse_driver_t *pulsed) {
  // force update - "volatile" doesn't seem to work...
  gchar *tmp=g_strdup_printf("%p %d",pulsed->msgq,pulsed->pulsed_died);
  g_free(tmp);
  if (pulsed->pulsed_died) return NULL;
  return pulsed->msgq;
}



gint64 lives_pulse_get_time(pulse_driver_t *pulsed, gboolean absolute) {
  volatile aserver_message_t *msg=pulsed->msgq;
  gdouble frames_written;
  if (msg!=NULL&&msg->command==ASERVER_CMD_FILE_SEEK) while (pulse_get_msgq(pulsed)!=NULL); // wait for seek
  frames_written=pulsed->frames_written;
  if (frames_written<0.) frames_written=0.;
  if (pulsed->is_output) return pulsed->audio_ticks*absolute+(gint64)(frames_written/(gdouble)pulsed->out_arate*U_SEC);
  return pulsed->audio_ticks*absolute+(gint64)(frames_written/(gdouble)pulsed->in_arate*U_SEC);

}


static aserver_message_t pulse_message;

void pulse_audio_seek_frame (pulse_driver_t *pulsed, gint frame) {
  // seek to frame "frame" in current file
  volatile aserver_message_t *pmsg;
  if (frame<1) frame=1;
  long seekstart;
  do {
    pmsg=pulse_get_msgq(pulsed);
  } while ((pmsg!=NULL)&&pmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (pulsed->playing_file==-1) return;
  if (frame>afile->frames) frame=afile->frames;
  seekstart=(long)((gdouble)(frame-1.)/afile->fps*afile->arate)*afile->achans*(afile->asampsize/8);
  afile->aseek_pos=pulse_audio_seek_bytes(pulsed,seekstart);
}


long pulse_audio_seek_bytes (pulse_driver_t *pulsed, long bytes) {
  // seek to relative "secs" in current file
  volatile aserver_message_t *pmsg;
  long seekstart;
  do {
    pmsg=pulse_get_msgq(pulsed);
  } while ((pmsg!=NULL)&&pmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (pulsed->playing_file==-1) return 0;
  seekstart=((long)(bytes/afile->achans/(afile->asampsize/8)))*afile->achans*(afile->asampsize/8);

  if (seekstart<0) seekstart=0;
  if (seekstart>afile->afilesize) seekstart=afile->afilesize;
  pulse_message.command=ASERVER_CMD_FILE_SEEK;
  pulse_message.next=NULL;
  pulse_message.data=g_strdup_printf("%ld",seekstart);
  pulsed->msgq=&pulse_message;
  return seekstart;
}


#undef afile

#endif

