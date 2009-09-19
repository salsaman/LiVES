// pulse.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2009
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

static pa_threaded_mainloop *pa_mloop;
static pa_context *pcon;

static guint pulse_server_rate=0;

extern gint weed_apply_audio_instance (weed_plant_t *init_event, float **abuf, int nbtracks, int nchans, long nsamps, gdouble arate, weed_timecode_t tc, double *vis);



///////////////////////////////////////////////////////////////////


static void pulse_server_cb(pa_context *c,const pa_server_info *info, void *userdata) {
  pulse_server_rate=info->sample_spec.rate;
}



void lives_pulse_init (void) {
  // startup pulse audio server
  pa_mloop=pa_threaded_mainloop_new();
  pcon=pa_context_new(pa_threaded_mainloop_get_api(pa_mloop),"LiVES");
  pa_context_connect(pcon,NULL,0,NULL);
  pa_threaded_mainloop_start(pa_mloop);

}


void pulse_get_rec_avals(pulse_driver_t *pulsed) {
  mainw->rec_aseek=pulsed->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
  mainw->rec_aclip=pulsed->playing_file;
  mainw->rec_avel=afile->pb_fps/afile->fps;
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


static void pulse_audio_process (pa_stream *pstream, size_t nbytes, void *arg) {
  // PULSE AUDIO calls this periodically to get the next audio buffer

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

  pa_cvolume myvol;
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
	  in_bytes=ABS((in_frames=((gdouble)pulsed->in_arate/(gdouble)pulsed->out_arate*(gdouble)pulseFramesAvailable+(fastrand()/G_MAXUINT32))))*pulsed->in_achans*(pulsed->in_asamps>>3);
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
    
    if (pulsed->in_asamps==pulsed->out_asamps&&shrink_factor==1.&&pulsed->in_achans==pulsed->out_achans&&!pulsed->reverse_endian) {
      // no transformation needed
      pulsed->sound_buffer=buffer;
    }
    else {
      pulsed->sound_buffer=g_malloc0(pulsed->chunk_size);

      if (pulsed->in_asamps==8) {
	sample_move_d8_d16 ((short *)(pulsed->sound_buffer),(guchar *)buffer, numFramesToWrite, in_bytes, shrink_factor, pulsed->out_achans, pulsed->in_achans);
      }
      else {
	sample_move_d16_d16((short*)pulsed->sound_buffer, (short*)buffer, numFramesToWrite, in_bytes, shrink_factor, pulsed->out_achans, pulsed->in_achans, pulsed->reverse_endian);
      }
    }

    pulsed->frames_written+=numFramesToWrite;
    pulseFramesAvailable-=numFramesToWrite;
    
#ifdef DEBUG_PULSE
    g_printerr("pulseFramesAvailable == %ld\n", pulseFramesAvailable);
#endif
   }
  
  // playback from memory or file

  pavol=pa_sw_volume_from_linear(mainw->volume);
  pa_cvolume_set(&myvol,pulsed->out_achans,pavol);

  pa_context_set_sink_input_volume(pulsed->con,pa_stream_get_index(pulsed->pstream),&myvol,NULL,NULL);
  
  while (nbytes>0) {
    if (nbytes<xbytes) xbytes=nbytes;
      
    if (!from_memory) {
      if (xbytes/pulsed->out_achans/(pulsed->out_asamps>>3)<=numFramesToWrite&&offs==0) buffer=pulsed->sound_buffer;
      else {
	buffer=g_malloc(xbytes);
	w_memcpy(buffer,pulsed->sound_buffer+offs,xbytes);
	offs+=xbytes;
	needs_free=TRUE;
      }
      pa_stream_write(pulsed->pstream,buffer,xbytes,needs_free?pulse_buff_free:NULL,0,PA_SEEK_RELATIVE);
    }
    else {
      if (pulsed->read_abuf>-1&&!pulsed->mute) {
	//sample_move_abuf_float(out_buffer,pulsed->out_achans,nframes,pulsed->out_arate,vol);
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



/*
static int pulse_audio_read (nframes_t nframes, void *arg) {
  // read nframes from pulse buffer, and then write to mainw->aud_rec_fd
  pulse_driver_t* pulsed = (pulse_driver_t*)arg;
  void *holding_buff;
  float *in_buffer[pulsed->num_input_channels];
  float out_scale=(float)pulsed->in_arate/(float)afile->arate;
  int out_unsigned=afile->signed_endian&AFORM_UNSIGNED;
  int i;
  long frames_out;

  if (mainw->effects_paused) return 0; // pause during record

  holding_buff=malloc(nframes*afile->achans*afile->asampsize/8);

  if (nframes != pulsed->chunk_size) pulsed->chunk_size = nframes;

  for (i=0;i<pulsed->num_input_channels;i++) {
    in_buffer[i] = (float *) jack_port_get_buffer(pulsed->input_port[i], nframes);
  }

  frames_out=sample_move_float_int((void *)holding_buff,in_buffer,nframes,out_scale,afile->achans,afile->asampsize,out_unsigned,pulsed->reverse_endian,1.);

  pulsed->frames_written+=nframes;

  if (mainw->rec_samples>0) {
    if (frames_out>mainw->rec_samples) frames_out=mainw->rec_samples;
    mainw->rec_samples-=frames_out;
  }

  dummyvar=write (mainw->aud_rec_fd,holding_buff,frames_out*(afile->asampsize/8)*afile->achans);

  free(holding_buff);

  if (mainw->rec_samples==0&&mainw->cancelled==CANCEL_NONE) mainw->cancelled=CANCEL_KEEP; // we wrote the required #

  return 0;
}

*/


void pulse_shutdown(void* arg) {


}


void pulse_close_client(pulse_driver_t *pdriver, gboolean shutdown) {
  pa_proplist_free(pdriver->pa_props);
  pa_stream_disconnect(pdriver->pstream);

  if (shutdown) {
    pa_context_disconnect(pdriver->con);
    pa_threaded_mainloop_stop(pdriver->mloop);
    pa_threaded_mainloop_free(pdriver->mloop);
  }
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
  pulsed_reader.out_achans=2;
  pulsed_reader.out_asamps=16;
  pulsed_reader.mute=FALSE;
  pulsed_reader.is_output=FALSE;
  pulsed_reader.is_paused=FALSE;

  return 0;
}



int pulse_driver_activate(pulse_driver_t *pdriver) {
// create a new client and connect it to pulse server
  gchar *pa_clientname="LiVES_audio_out";
  gchar *mypid=g_strdup_printf("%d",getpid());

  pa_sample_spec pa_spec;
  pa_channel_map pa_map;
  pa_buffer_attr pa_battr;

  pa_operation *pa_op;

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

  pa_battr.maxlength=(uint32_t)-1;
  pa_battr.tlength=2048;
  pa_battr.prebuf=(uint32_t)-1;
  pa_battr.minreq=(uint32_t)-1;

  pdriver->state=pa_context_get_state(pdriver->con);

  while (pdriver->state!=PA_CONTEXT_READY) {
    g_usleep(prefs->sleep_time);
    sched_yield();
    pdriver->state=pa_context_get_state(pdriver->con);
  }

  pa_op=pa_context_get_server_info(pdriver->con,pulse_server_cb,NULL);

  while (pa_operation_get_state(pa_op)!=PA_OPERATION_DONE) {
    g_usleep(prefs->sleep_time);
  }

  pa_spec.rate=pdriver->out_arate=pulse_server_rate;

  pdriver->pstream=pa_stream_new_with_proplist(pdriver->con,pa_clientname,&pa_spec,&pa_map,pdriver->pa_props);

#ifdef PA_STREAM_START_UNMUTED
    pa_stream_connect_playback(pdriver->pstream,NULL,&pa_battr,PA_STREAM_START_UNMUTED|PA_STREAM_ADJUST_LATENCY|PA_STREAM_INTERPOLATE_TIMING,NULL,NULL);
#else
    pa_stream_connect_playback(pdriver->pstream,NULL,&pa_battr,PA_STREAM_ADJUST_LATENCY|PA_STREAM_INTERPOLATE_TIMING,NULL,NULL);
#endif

  if (pdriver->is_output) {
    // set write callback
    pa_stream_set_write_callback(pdriver->pstream,pulse_audio_process,pdriver);
  }
  else {
    // set write callback
    //pa_stream_set_read_callback(pstream,pulse_audio_process,pdriver);
  }

  g_free(mypid);

  return 0;
}


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



gint64 lives_pulse_get_time(pulse_driver_t *pulsed) {
  volatile aserver_message_t *msg=pulsed->msgq;
  if (msg!=NULL&&msg->command==ASERVER_CMD_FILE_SEEK) while (pulse_get_msgq(pulsed)!=NULL); // wait for seek
  if (pulsed->is_output) return pulsed->audio_ticks+(gint64)(((gdouble)pulsed->frames_written/(gdouble)pulsed->out_arate)*U_SEC);
  return pulsed->audio_ticks+(gint64)(((gdouble)pulsed->frames_written/(gdouble)pulsed->in_arate)*U_SEC);

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

