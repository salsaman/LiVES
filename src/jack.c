// jack.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2011
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_JACK
#include "callbacks.h"
#include "support.h"

#define afile mainw->files[jackd->playing_file]

static jack_client_t *jack_transport_client;

static unsigned char *zero_buff=NULL;
static size_t zero_buff_count=0;

static void check_zero_buff(size_t check_size) {
  if (check_size>zero_buff_count) {
    zero_buff=g_realloc(zero_buff,check_size);
    memset(zero_buff+zero_buff_count,0,check_size-zero_buff_count);
    zero_buff_count=check_size;
  }
}

gboolean lives_jack_init (void) {
  gchar *jt_client=g_strdup_printf("LiVES-%d",getpid());
  const char *server_name="default";
  jack_options_t options=JackServerName;
  jack_status_t status;

  jack_transport_client=NULL;

  if ((prefs->jack_opts&JACK_OPTS_START_TSERVER)||(prefs->jack_opts&JACK_OPTS_START_ASERVER)) {
    unsetenv ("JACK_NO_START_SERVER");
    setenv ("JACK_START_SERVER","1",0);

    if (!g_file_test(prefs->jack_aserver,G_FILE_TEST_EXISTS)) {
      gchar *com;
      gchar jackd_loc[512];
      get_location("jackd",jackd_loc,512);
      if (strlen(jackd_loc)) {
	/*
#ifndef IS_DARWIN
	com=g_strdup_printf("echo \"%s -Z -r -d alsa\">%s",jackd_loc,prefs->jack_aserver);
#else
#ifdef IS_SOLARIS
	// use OSS on Solaris
	com=g_strdup_printf("echo \"%s -Z -d oss\">%s",jackd_loc,prefs->jack_aserver);
#else
	// use coreaudio on Darwin
	com=g_strdup_printf("echo \"%s -Z -d coreaudio\">%s",jackd_loc,prefs->jack_aserver);
#endif
#endif
	*/

	// oops - jack removed the -Z option from jackd
	// and broke backwards compatibility for existing apps...

#ifndef IS_DARWIN
	com=g_strdup_printf("echo \"%s -d alsa\">%s",jackd_loc,prefs->jack_aserver);
#else
#ifdef IS_SOLARIS
	// use OSS on Solaris
	com=g_strdup_printf("echo \"%s -d oss\">%s",jackd_loc,prefs->jack_aserver);
#else
	// use coreaudio on Darwin
	com=g_strdup_printf("echo \"%s -d coreaudio\">%s",jackd_loc,prefs->jack_aserver);
#endif
#endif
	dummyvar=system(com);
	g_free(com);
	com=g_strdup_printf("/bin/chmod o+x %s",prefs->jack_aserver);
	dummyvar=system(com);
	g_free(com);
      }
    }

  }
  else {
    unsetenv ("JACK_START_SERVER");
    setenv ("JACK_NO_START_SERVER","1",0);
    options|=JackNoStartServer;
  }

  // startup the server
  jack_transport_client=jack_client_open (jt_client, options, &status, server_name);
  g_free(jt_client);

  if (jack_transport_client==NULL) return FALSE;

#ifdef ENABLE_JACK_TRANSPORT
  jack_activate(jack_transport_client);
  jack_set_sync_timeout(jack_transport_client,5000000); // seems to not work
  jack_set_sync_callback (jack_transport_client, lives_start_ready_callback, NULL);
  gtk_timeout_add(KEY_RPT_INTERVAL,&lives_jack_poll,NULL);
#else
  jack_client_close (jack_transport_client);
  jack_transport_client=NULL;
#endif

  if (status&JackServerStarted) {
    d_print (_("JACK server started\n"));
  }

  return TRUE;

}

/////////////////////////////////////////////////////////////////
// transport handling



gdouble jack_transport_get_time(void) {
#ifdef ENABLE_JACK_TRANSPORT
  jack_nframes_t srate; 
  jack_position_t pos;

  srate=jack_get_sample_rate(jack_transport_client);
  return (gdouble)pos.frame/(gdouble)srate;
#endif
  return 0.;
}






#ifdef ENABLE_JACK_TRANSPORT
static void jack_transport_check_state (void) {
  jack_position_t pos;
  jack_transport_state_t jacktstate;

  // go away until the app has started up properly
  if (mainw->go_away) return;

  if (!(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)) return;

  if (jack_transport_client==NULL) return;

  jacktstate=jack_transport_query (jack_transport_client, &pos);

  if (mainw->jack_can_start&&(jacktstate==JackTransportRolling||jacktstate==JackTransportStarting)&&mainw->playing_file==-1&&mainw->current_file>0&&!mainw->is_processing) {
    mainw->jack_can_start=FALSE;
    mainw->jack_can_stop=TRUE;
    on_playall_activate(NULL,NULL);
  }

  if (jacktstate==JackTransportStopped) {
    if (mainw->playing_file>-1&&mainw->jack_can_stop) {
      on_stop_activate (NULL,NULL);
    }
    mainw->jack_can_start=TRUE;
  }
}
#endif

gboolean lives_jack_poll(gpointer data) {
  // data is always NULL
  // must return TRUE
#ifdef ENABLE_JACK_TRANSPORT
  jack_transport_check_state();
#endif
  return TRUE;
}

void lives_jack_end (void) {
#ifdef ENABLE_JACK_TRANSPORT
  jack_client_t *client=jack_transport_client;
#endif
  jack_transport_client=NULL; // stop polling transport
#ifdef ENABLE_JACK_TRANSPORT
  if (client!=NULL) {
    jack_deactivate (client);
    jack_client_close (client);
  }
#endif
}


void jack_pb_start (void) {
  // call this ASAP, then in load_frame_image; we will wait for sync from other clients (and ourself !)
#ifdef ENABLE_JACK_TRANSPORT
  if (prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER) jack_transport_start (jack_transport_client);
#endif
}

void jack_pb_stop (void) {
  // call this after pb stops
#ifdef ENABLE_JACK_TRANSPORT
  if (prefs->jack_opts&JACK_OPTS_TRANSPORT_MASTER) jack_transport_stop (jack_transport_client);
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
    mainw->rec_aseek=jackd->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
    mainw->rec_avel=afile->pb_fps/afile->fps;
  }
}

static void jack_set_rec_avals(jack_driver_t *jackd, gboolean is_forward) {
  // record direction change
  mainw->rec_aclip=jackd->playing_file;
  if (mainw->rec_aclip!=-1) {
    mainw->rec_avel=ABS(afile->pb_fps/afile->fps);
    if (!is_forward) mainw->rec_avel=-mainw->rec_avel;
    mainw->rec_aseek=(gdouble)jackd->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
  }
}


static void push_cache_buffer (lives_audio_buf_t *cache_buffer, jack_driver_t *jackd, 
			       size_t in_bytes, size_t nframes, double shrink_factor) {

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


static lives_audio_buf_t *pop_cache_buffer (void) {
  lives_audio_buf_t *cache_buffer=audio_cache_get_buffer();
  if (cache_buffer!=NULL) return cache_buffer;
  return NULL;
}



static int audio_process (nframes_t nframes, void *arg) {
  // JACK calls this periodically to get the next audio buffer
  float* out_buffer[JACK_MAX_OUTPUT_PORTS];
  jack_driver_t* jackd = (jack_driver_t*)arg;
  jack_position_t pos;
  register int i;
  aserver_message_t *msg;
  long xseek;
  int new_file;
  gboolean from_memory=FALSE;
  gboolean wait_cache_buffer=FALSE;
  lives_audio_buf_t *cache_buffer=NULL;
  size_t nbytes;

#ifdef DEBUG_AJACK
  g_printerr("nframes %ld, sizeof(float) == %d\n", (long)nframes, sizeof(float));
#endif

  if (!mainw->is_ready||jackd==NULL||(mainw->playing_file==-1&&jackd->is_silent&&jackd->msgq==NULL)) return 0;

  /* process one message */
  while ((msg=(aserver_message_t *)jackd->msgq)!=NULL) {

    // TODO - set seek_pos after setting record params
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file=atoi(msg->data);
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
      xseek=atol(msg->data);
      if (xseek<0.) xseek=0.;
      jackd->seek_pos=xseek;
      jackd->audio_ticks=mainw->currticks;
      jackd->frames_written=0;
      break;
    default:
      msg->data=NULL;
    }
    if (msg->data!=NULL) g_free(msg->data);
    msg->command=ASERVER_CMD_PROCESSED;
    if (msg->next==NULL) jackd->msgq=NULL; 
    else jackd->msgq = msg->next;
  }

  if (nframes==0) return 0;

  /* retrieve the buffers for the output ports */
  for (i = 0; i < jackd->num_output_channels; i++) 
    out_buffer[i] = (float *) jack_port_get_buffer(jackd->output_port[i], 
						   nframes);

  if (jackd->read_abuf==-1) {
    // assign local copy from 
    if (mainw->playing_file==-1 || ( cache_buffer = pop_cache_buffer() )==NULL || !cache_buffer->is_ready ) {
      // audio buffer is not ready yet
      if (!jackd->is_silent) {
	for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], nframes);
	jackd->is_silent=TRUE;
      }
      return 0;
    }
    if (cache_buffer->fileno==-1) jackd->playing_file=-1;
  }

  if (cache_buffer!=NULL && cache_buffer->in_achans>0 && !cache_buffer->is_ready ) wait_cache_buffer=TRUE;

  jackd->state=jack_transport_query (jackd->client, &pos);

#ifdef DEBUG_AJACK
  g_printerr("STATE is %d %d\n",jackd->state,jackd->play_when_stopped);
#endif

  /* handle playing state */
  if (jackd->state==JackTransportRolling||jackd->play_when_stopped) {
    gulong jackFramesAvailable = nframes; /* frames we have left to write to jack */
    gulong inputFramesAvailable;          /* frames we have available this loop */
    gulong numFramesToWrite;              /* num frames we are writing this loop */
    glong in_frames=0;
    gulong in_bytes=0;
    gfloat shrink_factor=1.f;
    gdouble vol;

#ifdef DEBUG_AJACK
    g_printerr("playing... jackFramesAvailable = %ld\n", jackFramesAvailable);
#endif

    jackd->num_calls++;

    if (!jackd->in_use||((jackd->playing_file<0||jackd->seek_pos<0.)&&jackd->read_abuf<0)||jackd->is_paused) {
      /* output silence if nothing is being outputted */
      if (!jackd->is_silent) for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], nframes);
      jackd->is_silent=TRUE;

      // external streaming
      nbytes=nframes*4;
      check_zero_buff(nbytes);
      audio_stream(zero_buff,nbytes,jackd->astream_fd);

      if (!jackd->is_paused) jackd->frames_written+=nframes;
      if (jackd->seek_pos<0.&&jackd->playing_file>-1&&afile!=NULL) {
	jackd->seek_pos+=nframes*afile->achans*afile->asampsize/8;
      }
      return 0;
    }

    jackd->is_silent=FALSE;

    if (G_LIKELY(jackFramesAvailable>0)) {
      /* (bytes of data) / (2 bytes(16 bits) * X input channels) == frames */

      if (mainw->playing_file>-1&&jackd->read_abuf>-1) {
	// playing back from memory buffers instead of from file
	// this is used in multitrack
	from_memory=TRUE;

	numFramesToWrite=jackFramesAvailable;
	jackd->frames_written+=numFramesToWrite;
	jackFramesAvailable=0;

      }
      else {
	if (G_LIKELY(jackd->playing_file>=0)) {
	  in_bytes=ABS((in_frames=((gdouble)jackd->sample_in_rate/(gdouble)jackd->sample_out_rate*
				   (gdouble)jackFramesAvailable+((gdouble)fastrand()/(gdouble)G_MAXUINT32))))
	    *jackd->num_input_channels*jackd->bytes_per_channel;
	  if ((shrink_factor=(gfloat)in_frames/(gfloat)jackFramesAvailable)<0.f) {
	    // reverse playback
	    if ((jackd->seek_pos-=in_bytes)<0) {
	      // reached beginning backwards

	      if (jackd->loop==AUDIO_LOOP_NONE) {
		if (*jackd->whentostop==STOP_ON_AUD_END) {
		  *jackd->cancelled=CANCEL_AUD_END;
		}
		jackd->in_use=FALSE;
		jack_set_rec_avals(jackd,FALSE);
	      }
	      else {
		if (jackd->loop==AUDIO_LOOP_PINGPONG) {
		  jackd->sample_in_rate=-jackd->sample_in_rate;
		  shrink_factor=-shrink_factor;
		  jackd->seek_pos=0;
		  jack_set_rec_avals(jackd,TRUE);
		}
		else jackd->seek_pos=jackd->seek_end-in_bytes;
		jack_set_rec_avals(jackd,FALSE);
	      }
	    }
	  }

	  // TODO - why is this in the middle ?
	  if (jackd->mute) {
	    if (shrink_factor>0.f) jackd->seek_pos+=in_bytes;
	    if (jackd->seek_pos>=jackd->seek_end) {
	      if (*jackd->whentostop==STOP_ON_AUD_END) {
		*jackd->cancelled=CANCEL_AUD_END;
		jackd->in_use=FALSE;
	      }
	      else {
		if (jackd->loop==AUDIO_LOOP_PINGPONG) {
		  jackd->sample_in_rate=-jackd->sample_in_rate;
		  jackd->seek_pos-=(jackd->seek_pos - jackd->seek_end);
		}
		else {
		  jackd->seek_pos=0;
		}
	      }
	    }

	    if (!wait_cache_buffer) push_cache_buffer( cache_buffer, jackd, in_bytes, nframes, shrink_factor );

	    for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], nframes);

	    // external streaming
	    nbytes=nframes*4;
	    check_zero_buff(nbytes);
	    audio_stream(zero_buff,nbytes,jackd->astream_fd);

	    jackd->frames_written+=nframes;
	    return 0;
	  }

	  else {
	    if (shrink_factor>0.) {
	      if (!(*jackd->cancelled)) {
		gboolean eof=cache_buffer->eof;

		if (eof) {
		  // reached the end, forwards

		  if (*jackd->whentostop==STOP_ON_AUD_END) {
		    jackd->in_use=FALSE;
		    *jackd->cancelled=CANCEL_AUD_END;
		  }
		  else {
		    if (jackd->loop==AUDIO_LOOP_PINGPONG) {
		      jackd->sample_in_rate=-jackd->sample_in_rate;
		      jackd->seek_pos-=in_bytes; // TODO
		      jack_set_rec_avals(jackd,FALSE);
		    }
		    else {
		      if (jackd->loop!=AUDIO_LOOP_NONE) {
			jackd->seek_pos=0;
			jack_set_rec_avals(jackd,TRUE);
		      }
		      else {
			jackd->in_use=FALSE;
			jack_set_rec_avals(jackd,TRUE);
		      }
		    }
		  }
		}
	      }
	    }
	  }

	}

	if (!jackd->in_use||in_bytes==0) {
	  // reached end of audio with no looping
	  for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], nframes);
	  jackd->is_silent=TRUE;

	  // external streaming
	  nbytes=nframes*4;
	  check_zero_buff(nbytes);
	  audio_stream(zero_buff,nbytes,jackd->astream_fd);

	  if (!jackd->is_paused) jackd->frames_written+=nframes;
	  if (jackd->seek_pos<0.&&jackd->playing_file>-1&&afile!=NULL) {
	    jackd->seek_pos+=nframes*afile->achans*afile->asampsize/8;
	  }
	  return 0;
	}
	
	inputFramesAvailable = cache_buffer->samp_space;
#ifdef DEBUG_AJACK
	g_printerr("%d inputFramesAvailable == %ld, %ld %ld,jackFramesAvailable == %ld\n", inputFramesAvailable, in_frames,jackd->sample_in_rate,jackd->sample_out_rate,jackFramesAvailable);
#endif
	
	/* write as many bytes as we have space remaining, or as much as we have data to write */
	numFramesToWrite = MIN(jackFramesAvailable, inputFramesAvailable);

#ifdef DEBUG_AJACK
	g_printerr("nframes == %d, jackFramesAvailable == %ld,\n\tjackd->num_input_channels == %ld, jackd->num_output_channels == %ld, nf2w %ld, in_bytes %d, sf %.8f\n",  nframes, jackFramesAvailable, jackd->num_input_channels, jackd->num_output_channels, numFramesToWrite, in_bytes, shrink_factor);
#endif
	
	jackd->frames_written+=numFramesToWrite;
	jackFramesAvailable-=numFramesToWrite; /* take away what was written */
	
#ifdef DEBUG_AJACK
	g_printerr("jackFramesAvailable == %ld\n", jackFramesAvailable);
#endif
      }
      // playback from memory or file
      vol=mainw->volume*mainw->volume; // TODO - we should really use a logarithmic scale
      
      if ( !from_memory && numFramesToWrite > 0 ) {
	if (((gint)(jackd->num_calls/100.))*100==jackd->num_calls) if (mainw->soft_debug) g_print("audio pip\n");
	if (cache_buffer->bufferf!=NULL) {
	  for (i=0;i<jackd->num_output_channels;i++) {
	    sample_move_d16_float(out_buffer[i], cache_buffer->buffer16[0] + i, numFramesToWrite, jackd->num_output_channels, afile->signed_endian&AFORM_UNSIGNED, vol);
	  }

	  if (jackd->astream_fd!=-1) {
	    unsigned char *xbuf=(unsigned char *)cache_buffer->buffer16[0];
	    // audio streaming if enabled
	    nbytes=numFramesToWrite*4;

	    if (jackd->num_output_channels!=2) {
	      // need to remap channels to stereo (assumed for now)
	      size_t bysize=4,tsize=0;
	      unsigned char *inbuf=(unsigned char *)cache_buffer->buffer16[0];
	      xbuf=g_malloc(nbytes);
	      
	      if (jackd->num_output_channels==1) bysize=2;
	      while (nbytes>0) {
		memcpy(xbuf+tsize,inbuf,bysize);
		tsize+=bysize;
		nbytes-=bysize;
		if (bysize==2) {
		  // duplicate mono channel
		  memcpy(xbuf+tsize,inbuf,bysize);
		  tsize+=bysize;
		  nbytes-=bysize;
		  inbuf+=bysize;
		}
		else {
		  // or skip extra channels
		  inbuf+=jackd->num_output_channels*4;
		}
	      }
	      nbytes=numFramesToWrite*4;
	    }
	    audio_stream(xbuf,nbytes,jackd->astream_fd);
	    if (xbuf!=(unsigned char *)cache_buffer->buffer16[0]) g_free(xbuf);
	  }

	}
	else {
	  for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], numFramesToWrite);
	  
	  // external streaming
	  nbytes=numFramesToWrite*4;
	  check_zero_buff(nbytes);
	  audio_stream(zero_buff,nbytes,jackd->astream_fd);
	}
      }
      else {
	if (jackd->read_abuf>-1&&!jackd->mute) {
	  sample_move_abuf_float(out_buffer,jackd->num_output_channels,nframes,jackd->sample_out_rate,vol);

	  if (jackd->astream_fd!=-1) {
	    // audio streaming if enabled
	    unsigned char *xbuf=(unsigned char *)out_buffer;
	    nbytes=numFramesToWrite*4;

	    if (jackd->num_output_channels!=2) {
	      // need to remap channels to stereo (assumed for now)
	      size_t bysize=4,tsize=0;
	      unsigned char *inbuf=(unsigned char *)out_buffer;
	      xbuf=g_malloc(nbytes);
	      
	      if (jackd->num_output_channels==1) bysize=2;
	      while (nbytes>0) {
		memcpy(xbuf+tsize,inbuf,bysize);
		tsize+=bysize;
		nbytes-=bysize;
		if (bysize==2) {
		  // duplicate mono channel
		  memcpy(xbuf+tsize,inbuf,bysize);
		  tsize+=bysize;
		  nbytes-=bysize;
		  inbuf+=bysize;
		}
		else {
		  // or skip extra channels
		  inbuf+=jackd->num_output_channels*4;
		}
	      }
	      nbytes=numFramesToWrite*4;
	    }
	    audio_stream(xbuf,nbytes,jackd->astream_fd);
	    if (xbuf!=(unsigned char *)out_buffer) g_free(xbuf);
	  }
	}
	else {
	  for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], nframes);
	  
	  // external streaming
	  nbytes=nframes*4;
	  check_zero_buff(nbytes);
	  audio_stream(zero_buff,nbytes,jackd->astream_fd);
	}
      }
    }

    if (!from_memory) {
      if (!wait_cache_buffer) push_cache_buffer( cache_buffer, jackd, in_bytes, nframes, shrink_factor );
      if (shrink_factor>0.) jackd->seek_pos+=in_bytes;
    }

    /*    jackd->jack_pulse[0]=set_pulse(out_buffer[0],jack->buffer_size,8);
	  if (jackd->num_output_channels>1) {
	  jackd->jack_pulse[1]=set_pulse(out_buffer[1],jackd->buffer_size,8);
	  }
	  else jackd->jack_pulse[1]=jackd->jack_pulse[0];
    */
    
    if(jackFramesAvailable) {
#ifdef DEBUG_AJACK
      g_printerr("buffer underrun of %ld frames\n", jackFramesAvailable);
#endif
      for(i = 0 ; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i] + (nframes - jackFramesAvailable), jackFramesAvailable);

      // external streaming
      nbytes=jackFramesAvailable*4;
      check_zero_buff(nbytes);
      audio_stream(zero_buff,nbytes,jackd->astream_fd);
    }

  }
  else if(jackd->state == JackTransportStarting || jackd->state == JackTransportStopped || jackd->state == JackTClosed || jackd->state == JackTReset) {
#ifdef DEBUG_AJACK
    g_printerr("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif
    
    /* output silence if nothing is being outputted */
    for(i = 0; i < jackd->num_output_channels; i++) sample_silence_dS(out_buffer[i], nframes);
    jackd->is_silent=TRUE;

    // external streaming
    nbytes=nframes*4;
    check_zero_buff(nbytes);
    audio_stream(zero_buff,nbytes,jackd->astream_fd);

    /* if we were told to reset then zero out some variables */
    /* and transition to STOPPED */
    if(jackd->state == JackTReset) {
      jackd->state = JackTStopped; /* transition to STOPPED */
    }
  }

#ifdef DEBUG_AJACK
  g_printerr("done\n");
#endif

  return 0;
}



int lives_start_ready_callback (jack_transport_state_t state, jack_position_t *pos, void *arg) {
  gboolean seek_ready;

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




static int audio_read (nframes_t nframes, void *arg) {
  // read nframes from jack buffer, and then write to mainw->aud_rec_fd

  // this is the jack callback for when we are recording audio

  jack_driver_t* jackd = (jack_driver_t*)arg;
  void *holding_buff;
  float *in_buffer[jackd->num_input_channels];
  float out_scale=(float)jackd->sample_in_rate/(float)afile->arate;
  int out_unsigned=afile->signed_endian&AFORM_UNSIGNED;
  int i;
  long frames_out;

  if (mainw->effects_paused) return 0; // pause during record

  if (mainw->rec_samples==0) return 0; // wrote enough already, return until main thread stop

  frames_out=(long)((gdouble)nframes/out_scale+1.);

  holding_buff=malloc(frames_out*afile->achans*afile->asampsize/8);

  for (i=0;i<jackd->num_input_channels;i++) {
    in_buffer[i] = (float *) jack_port_get_buffer(jackd->input_port[i], nframes);
  }

  frames_out=sample_move_float_int((void *)holding_buff,in_buffer,nframes,out_scale,afile->achans,afile->asampsize,out_unsigned,jackd->reverse_endian,1.);

  jackd->frames_written+=nframes;

  if (mainw->rec_samples>0) {
    if (frames_out>mainw->rec_samples) frames_out=mainw->rec_samples;
    mainw->rec_samples-=frames_out;
  }

  dummyvar=write (mainw->aud_rec_fd,holding_buff,frames_out*(afile->asampsize/8)*afile->achans);

  free(holding_buff);

  if (mainw->rec_samples==0&&mainw->cancelled==CANCEL_NONE) mainw->cancelled=CANCEL_KEEP; // we wrote the required #

  return 0;
}



int jack_get_srate (nframes_t nframes, void *arg) {
  //g_printerr("the sample rate is now %ld/sec\n", (long)nframes);
  return 0;
}



void jack_shutdown(void* arg) {
  jack_driver_t* jackd = (jack_driver_t*)arg;

  jackd->client = NULL; /* reset client */
  jackd->jackd_died = TRUE;
  jackd->msgq=NULL;

  g_printerr("jack shutdown, setting client to 0 and jackd_died to true\n");
  g_printerr("trying to reconnect right now\n");

  /////////////////////

  jack_audio_init();

  mainw->jackd=jack_get_driver(0,TRUE);
  mainw->jackd->msgq=NULL;

  if (mainw->jackd->playing_file!=-1&&afile!=NULL) jack_audio_seek_bytes(mainw->jackd,mainw->jackd->seek_pos); // at least re-seek to the right place
}

/* Return the difference between two timeval structures in terms of milliseconds */
inline long TimeValDifference(struct timeval *start, struct timeval *end) {
  return (long)((gdouble)(end->tv_sec-start->tv_sec)*(gdouble)1000.+(gdouble)(end->tv_usec-start->tv_usec)/(gdouble)1000.);
}


static void jack_reset_driver(jack_driver_t *jackd) {
  //g_printerr("resetting jackd->dev_idx(%d)\n", jackd->dev_idx);
  /* tell the callback that we are to reset, the callback will transition this to STOPPED */
  jackd->state=JackTReset;
}


void jack_close_device(jack_driver_t* jackd) {
  int i;

  //g_printerr("closing the jack client thread\n");
  if (jackd->client) {
    jack_deactivate(jackd->client); /* supposed to help the jack_client_close() to succeed */
    //g_printerr("after jack_deactivate()\n");
    jack_client_close(jackd->client);
  }
  
  jack_reset_driver(jackd);
  jackd->client=NULL;
  
  jackd->is_active=FALSE;


  /* free up the port strings */
  //g_printerr("freeing up port strings\n");
  if (jackd->jack_port_name_count>1) {
    for (i=0;i<jackd->jack_port_name_count;i++) free(jackd->jack_port_name[i]);
    free(jackd->jack_port_name);
  }
}




static void jack_error_func(const char *desc) {
  g_printerr("Jack audio error %s\n",desc);
}


// wait 5 seconds to startup
#define JACK_START_WAIT 5000000


// create a new client and connect it to jack, connect the ports
int jack_open_device(jack_driver_t *jackd) {
  const char *client_name="LiVES_audio_out";
  const char *server_name="default";
  jack_options_t options=JackServerName|JackNoStartServer;
  jack_status_t status;
  int i;
  
  struct timeval otv;
  int64_t ntime=0,stime;

  jackd->is_active=FALSE;

  /* set up an error handler */
  jack_set_error_function(jack_error_func);
  jackd->client=NULL;

  gettimeofday(&otv, NULL);
  stime=otv.tv_sec*1000000+otv.tv_usec;

  while (jackd->client==NULL&&ntime<JACK_START_WAIT) {
    jackd->client = jack_client_open (client_name, options, &status, server_name);
    
    g_usleep(prefs->sleep_time);

    gettimeofday(&otv, NULL);
    ntime=(otv.tv_sec*1000000+otv.tv_usec-stime);
  }


  if (jackd->client==NULL) {
    g_printerr ("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status&JackServerFailed) {
      d_print (_("Unable to connect to JACK server\n"));
    }
    return 1;
  }
  
  if (status&JackNameNotUnique) {
    client_name= jack_get_client_name(jackd->client);
    g_printerr ("unique name `%s' assigned\n", client_name);
  }
  
  jackd->sample_out_rate=jack_get_sample_rate(jackd->client);
  
  //g_printerr (g_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  for (i=0;i<jackd->num_output_channels;i++) {
    gchar portname[32];
    g_snprintf(portname, 32, "out_%d", i);

#ifdef DEBUG_JACK_PORTS
    g_printerr("output port %d is named '%s'\n", i, portname);
#endif
    jackd->output_port[i] = jack_port_register(jackd->client, portname,
					       JACK_DEFAULT_AUDIO_TYPE,
					       JackPortIsOutput,
					       0);
    if (jackd->output_port[i]==NULL) {
      g_printerr("no more JACK output ports available\n");
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

  // set process callback and start
  jack_set_process_callback (jackd->client, audio_process, jackd);  


  return 0;


}




int jack_open_device_read(jack_driver_t *jackd) {
  // open a device to read audio from jack
  const char *client_name="LiVES_audio_in";
  const char *server_name="default";
  jack_options_t options=JackServerName|JackNoStartServer;
  jack_status_t status;
  int i;

  /* set up an error handler */
  jack_set_error_function(jack_error_func);
  jackd->client=NULL;
  while (jackd->client==NULL) 
    jackd->client = jack_client_open (client_name, options, &status, server_name);
    
  if (jackd->client==NULL) {
    g_printerr ("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status&JackServerFailed) {
      d_print (_("Unable to connect to JACK server\n"));
    }
    return 1;
  }
  
  if (status&JackNameNotUnique) {
    client_name= jack_get_client_name(jackd->client);
    g_printerr ("unique name `%s' assigned\n", client_name);
  }
  
  jackd->sample_in_rate=jack_get_sample_rate(jackd->client);
  
  //g_printerr (g_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  for (i=0;i<jackd->num_input_channels;i++) {
    gchar portname[32];
    g_snprintf(portname, 32, "in_%d", i);

#ifdef DEBUG_JACK_PORTS
    g_printerr("input port %d is named '%s'\n", i, portname);
#endif
    jackd->input_port[i] = jack_port_register(jackd->client, portname,
					      JACK_DEFAULT_AUDIO_TYPE,
					      JackPortIsInput,
					      0);
    if (jackd->input_port[i]==NULL) {
      g_printerr("no more JACK input ports available\n");
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
  jack_set_process_callback (jackd->client, audio_read, jackd);  

  return 0;


}



int jack_driver_activate (jack_driver_t *jackd) {
  // activate client and connect it

  // TODO *** - handle errors here !

  int i;
  const char** ports;
  gboolean failed=FALSE;

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
    if(jackd->jack_port_name_count==0) {
      //g_printerr("jack_get_ports() passing in NULL/NULL\n");
      ports=jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
    }
    else {
      //g_printerr("jack_get_ports() passing in port of '%s'\n", jackd->jack_port_name[0]);
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[0], NULL, jackd->jack_port_flags);
    }

    if (ports==NULL) {
      g_printerr("No jack ports available !\n");
      return 1;
    }

    /* display a trace of the output ports we found */
#ifdef DEBUG_JACK_PORTS
    for (i=0;ports[i];i++) g_printerr("ports[%d] = '%s'\n",i,ports[i]);
#endif

    /* see if we have enough ports */
    if(jackd->out_chans_available<jackd->num_output_channels) {
#ifdef DEBUG_JACK_PORTS
      g_printerr("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
      return ERR_PORT_NOT_FOUND;
    }

    /* connect the ports. Note: you can't do this before
       the client is activated (this may change in the future). */
    for(i=0;i<jackd->num_output_channels;i++) {
#ifdef DEBUG_JACK_PORTS
      g_printerr("jack_connect() to port %d('%p')\n", i, jackd->output_port[i]);
#endif
      if(jack_connect(jackd->client, jack_port_name(jackd->output_port[i]), ports[i])) {
#ifdef DEBUG_JACK_PORTS
          g_printerr("cannot connect to output port %d('%s')\n", i, ports[i]);
#endif
          failed=TRUE;
      }
    } 
    free(ports); /* free the returned array of ports */
  }
  else {
    for(i=0;i<jackd->jack_port_name_count;i++) {
#ifdef DEBUG_JACK_PORTS
      g_printerr("jack_get_ports() portname %d of '%s\n", i, jackd->jack_port_name[i]);
#endif
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[i], NULL, jackd->jack_port_flags);
#ifdef DEBUG_JACK_PORTS
      g_printerr ("ports[%d] = '%s'\n", 0, ports[0]);      /* display a trace of the output port we found */
#endif
      if(!ports) {
#ifdef DEBUG_JACK_PORTS
	g_printerr("jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
	return ERR_PORT_NOT_FOUND;
      }

      /* connect the port */
#ifdef DEBUG_JACK_PORTS
      g_printerr ("jack_connect() to port %d('%p')\n", i, jackd->output_port[i]);
#endif
      if(jack_connect(jackd->client, jack_port_name(jackd->output_port[i]), ports[0])) {
	//ERR("cannot connect to output port %d('%s')\n", 0, ports[0]);
	failed=TRUE;
      }
      free(ports); /* free the returned array of ports */
    }
  }

  jackd->is_active=TRUE;

  /* if something failed we need to shut the client down and return 0 */
  if (failed) {
    g_printerr("failed, closing and returning error\n");
    jack_close_device(jackd);
    return 1;
  }


  jackd->jackd_died = FALSE;

  jackd->in_use=FALSE;

  jackd->is_paused=FALSE;

  d_print(_("Started jack audio subsystem.\n"));

  return 0;
}






int jack_read_driver_activate (jack_driver_t *jackd) {
  // connect driver for reading
  int i;
  const char** ports;
  gboolean failed=FALSE;

  /* tell the JACK server that we are ready to roll */
  if (jack_activate(jackd->client)) {
    //ERR( "cannot activate client\n");
    return 1;
  }

  // we are looking for input ports to connect to
  jackd->jack_port_flags|=JackPortIsOutput;

  /* determine how we are to acquire port names */
  if ((jackd->jack_port_name_count==0)||(jackd->jack_port_name_count==1)) {
    if(jackd->jack_port_name_count==0) {
      //g_printerr("jack_get_ports() passing in NULL/NULL\n");
      ports=jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
    }
    else {
      //g_printerr("jack_get_ports() passing in port of '%s'\n", jackd->jack_port_name[0]);
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[0], NULL, jackd->jack_port_flags);
    }

    /* display a trace of the output ports we found */
#ifdef DEBUG_JACK_PORTS
    for (i=0;ports[i];i++) g_printerr("ports[%d] = '%s'\n",i,ports[i]);
#endif

    /* see if we have enough ports */
    if(jackd->in_chans_available<jackd->num_input_channels) {
#ifdef DEBUG_JACK_PORTS
      g_printerr("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
      return ERR_PORT_NOT_FOUND;
    }

    /* connect the ports. Note: you can't do this before
       the client is activated (this may change in the future). */
    for(i=0;i<jackd->num_input_channels;i++) {
#ifdef DEBUG_JACK_PORTS
      g_printerr("jack_connect() to port name %d('%p')\n", i, jackd->input_port[i]);
#endif
      if(jack_connect(jackd->client, ports[i], jack_port_name(jackd->input_port[i]))) {
#ifdef DEBUG_JACK_PORTS
          g_printerr("cannot connect to input port %d('%s')\n", i, ports[i]);
#endif
          failed=TRUE;
      }
    } 
    free(ports); /* free the returned array of ports */
  }
  else {
    for(i=0;i<jackd->jack_port_name_count;i++) {
#ifdef DEBUG_JACK_PORTS
      g_printerr("jack_get_ports() portname %d of '%s\n", i, jackd->jack_port_name[i]);
#endif
      ports=jack_get_ports(jackd->client, jackd->jack_port_name[i], NULL, jackd->jack_port_flags);
#ifdef DEBUG_JACK_PORTS
      g_printerr ("ports[%d] = '%s'\n", 0, ports[0]);      /* display a trace of the output port we found */
#endif
      if(!ports) {
#ifdef DEBUG_JACK_PORTS
	g_printerr("jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n", jackd->jack_port_flags);
#endif
	return ERR_PORT_NOT_FOUND;
      }

      /* connect the port */
#ifdef DEBUG_JACK_PORTS
      g_printerr ("jack_connect() to port %d('%p')\n", i, jackd->input_port[i]);
#endif
      if(jack_connect(jackd->client, ports[0], jack_port_name(jackd->input_port[i]))) {
	//ERR("cannot connect to output port %d('%s')\n", 0, ports[0]);
	failed=TRUE;
      }
      free(ports); /* free the returned array of ports */
    }
  }

  /* if something failed we need to shut the client down and return 0 */
  if (failed) {
    g_printerr("failed, closing and returning error\n");
    jack_close_device(jackd);
    return 1;
  }


  jackd->jackd_died = FALSE;

  jackd->in_use=FALSE;

  jackd->is_paused=FALSE;

  jackd->audio_ticks=0;

  d_print(_("Started jack audio reader.\n"));

  return 0;
}



jack_driver_t *jack_get_driver(gint dev_idx, gboolean is_output) {
  jack_driver_t *jackd;

  if (is_output) jackd = &outdev[dev_idx];
  else jackd = &indev[dev_idx];
#ifdef TRACE_getReleaseDevice
  g_printerr("dev_idx is %d\n", dev_idx);
#endif
  
  /* should we try to restart the jack server? */
  if (jackd->jackd_died&&jackd->client==NULL) {
    struct timeval now;
    gettimeofday(&now, 0);
    
    /* wait 250ms before trying again */
    if(TimeValDifference(&jackd->last_reconnect_attempt, &now)>=250) {
      if (is_output) jack_open_device(jackd);
      else jack_open_device_read(jackd);
      jackd->last_reconnect_attempt=now;
    }
  }
  
  return jackd;
}



static void jack_reset_dev(gint dev_idx, gboolean is_output) {
  jack_driver_t *jackd = jack_get_driver(dev_idx,is_output);
  //g_printerr("resetting dev %d\n", dev_idx);
  jack_reset_driver(jackd);
}


int jack_audio_init(void) {
  // initialise variables
  int i,j;
  jack_driver_t *jackd;

  for (i=0;i<JACK_MAX_OUTDEVICES;i++) {
    jackd = &outdev[i];
    jack_reset_dev(i,TRUE);
    jackd->dev_idx=i;
    jackd->client=NULL;
    jackd->in_use=FALSE;
    for (j=0;j<JACK_MAX_OUTPUT_PORTS;j++) jackd->volume[j]=1.0f;
    jackd->state=JackTClosed;
    jackd->sample_out_rate=jackd->sample_in_rate=0;
    jackd->seek_pos=jackd->seek_end=0;
    jackd->msgq=NULL;
    jackd->num_calls=0;
    //jackd->astream_file=NULL;
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
  }
  return 0;
}



int jack_audio_read_init(void) {
  int i,j;
  jack_driver_t *jackd;

  for (i=0;i<JACK_MAX_INDEVICES;i++) {
    jackd = &indev[i];
    jack_reset_dev(i,FALSE);
    jackd->dev_idx=i;
    jackd->client=NULL;
    jackd->in_use=FALSE;
    for (j=0;j<JACK_MAX_INPUT_PORTS;j++) jackd->volume[j]=1.0f;
    jackd->state=JackTClosed;
    jackd->sample_out_rate=jackd->sample_in_rate=0;
    jackd->seek_pos=jackd->seek_end=0;
    jackd->msgq=NULL;
    jackd->num_calls=0;
    //jackd->astream_file=NULL;
    jackd->jackd_died=FALSE;
    gettimeofday(&jackd->last_reconnect_attempt, 0);
    jackd->num_input_channels=2;
    jackd->play_when_stopped=FALSE;
    jackd->mute=FALSE;
    jackd->in_chans_available=0;
    jackd->is_output=FALSE;
  }
  return 0;
}


volatile aserver_message_t *jack_get_msgq(jack_driver_t *jackd) {
  // force update - "volatile" doesn't seem to work...
  gchar *tmp=g_strdup_printf("%p %d",jackd->msgq,jackd->jackd_died);
  g_free(tmp);
  if (jackd->jackd_died) return NULL;
  return jackd->msgq;
}

gint64 lives_jack_get_time(jack_driver_t *jackd, gboolean absolute) {
  // get the time in ticks since either playback started or since last seek

  volatile aserver_message_t *msg=jackd->msgq;
  gdouble frames_written=jackd->frames_written;
  if (frames_written<0.) frames_written=0.;
  if (msg!=NULL&&msg->command==ASERVER_CMD_FILE_SEEK) while (jack_get_msgq(jackd)!=NULL)
							sched_yield(); // wait for seek

  if (jackd->is_output) return jackd->audio_ticks*absolute+(gint64)(frames_written/(gdouble)jackd->sample_out_rate*U_SEC);
  return jackd->audio_ticks*absolute+(gint64)(frames_written/(gdouble)jackd->sample_in_rate*U_SEC);
}


gdouble lives_jack_get_pos(jack_driver_t *jackd) {
  // get current time position (seconds) in audio file
  return jackd->seek_pos/(gdouble)(afile->arate*afile->achans*afile->asampsize/8);
}



void jack_audio_seek_frame (jack_driver_t *jackd, gint frame) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample

  volatile aserver_message_t *jmsg;
  if (frame<1) frame=1;
  long seekstart;
  do {
    jmsg=jack_get_msgq(jackd);
  } while ((jmsg!=NULL)&&jmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (jackd->playing_file==-1) return;
  if (frame>afile->frames) frame=afile->frames;
  seekstart=(long)((gdouble)(frame-1.)/afile->fps*afile->arate)*afile->achans*(afile->asampsize/8);
  jack_audio_seek_bytes(jackd,seekstart);
}


long jack_audio_seek_bytes (jack_driver_t *jackd, long bytes) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file

  volatile aserver_message_t *jmsg;
  long seekstart;
  do {
    jmsg=jack_get_msgq(jackd);
  } while ((jmsg!=NULL)&&jmsg->command!=ASERVER_CMD_FILE_SEEK);
  if (jackd->playing_file==-1) return 0;
  seekstart=((long)(bytes/afile->achans/(afile->asampsize/8)))*afile->achans*(afile->asampsize/8);

  if (seekstart<0) seekstart=0;
  if (seekstart>afile->afilesize) seekstart=afile->afilesize;
  jack_message.command=ASERVER_CMD_FILE_SEEK;
  jack_message.next=NULL;
  jack_message.data=g_strdup_printf("%ld",seekstart);
  jackd->msgq=&jack_message;
  return seekstart;
}

#undef afile

#endif
