// audio.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2009
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "audio.h"
#include "events.h"
#include "effects-weed.h"
#include "support.h"

#include "../libweed/weed.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"


inline void sample_silence_dS (float *dst, unsigned long nsamples) {
  memset(dst,0,nsamples*sizeof(float));
}


void sample_move_d8_d16(short *dst, unsigned char *src,
			unsigned long nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount;
  unsigned char *ptr;
  unsigned char *src_end;

  // take care of (rounding errors ?)
  src_end=src+tbytes-nSrcChannels;

  if(!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  while(nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;
    ccount=0;

    /* loop until all of our destination channels are filled */
    while(nDstCount) {
      nSrcCount--;
      nDstCount--;

      ptr=src+ccount+src_offset_i;
      if (scale<0.f) {
	ptr=ptr>src?ptr:src;
      }
      else {
	ptr=ptr<src_end?ptr:src_end;
      }
      
      *(dst++) = *(ptr)<<8; /* copy the data over */
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if(!nSrcCount && nDstCount) {
	ccount=0;
	nSrcCount = nSrcChannels;
      }
    }
    
    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}



/* convert from any number of source channels to any number of destination channels */
void sample_move_d16_d16(short *dst, short *src,
			 unsigned long nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, gboolean swap_endian, gboolean swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount=0;
  short *ptr;
  short *src_end;

  if(!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  // take care of (rounding errors ?)
  src_end=src+tbytes/sizeof(short)-nSrcChannels;

  while (nsamples--) {

    if ((nSrcCount = nSrcChannels)==(nDstCount = nDstChannels)&&!swap_endian&&!swap_sign) {
      // same number of channels

      ptr=src+src_offset_i;
      if (scale<0.f) {
	ptr=ptr>src?ptr:src;
      }
      else {
	ptr=ptr<src_end?ptr:src_end;
      }

      w_memcpy(dst,ptr,nSrcChannels*sizeof(short));
      dst+=nDstCount;
    } 
    else {
      ccount=0;

      /* loop until all of our destination channels are filled */
      while (nDstCount) {
	nSrcCount--;
	nDstCount--;
	
	ptr=src+ccount+src_offset_i;
	if (scale<0.f) {
	  ptr=ptr>src?ptr:src;
	}
	else {
	  ptr=ptr<src_end?ptr:src_end;
	}

	/* copy the data over */
	if (!swap_endian) *(dst++) = swap_sign?(uint16_t)(*ptr+SAMPLE_MAX_16BITI):*ptr;
	else {
	  *(dst++)=swap_sign?(uint16_t)(((((*ptr)&0x00FF)<<8)+((*ptr)>>8))+SAMPLE_MAX_16BITI):(((*ptr)&0x00FF)<<8)+((*ptr)>>8);
	}

	ccount++;
	
	/* if we ran out of source channels but not destination channels */
	/* then start the src channels back where we were */
	if(!nSrcCount && nDstCount) {
	  ccount=0;
	  nSrcCount = nSrcChannels;
	}
      }
    }
    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}


/* convert from any number of source channels to any number of destination channels - 8 bit output */
void sample_move_d16_d8(uint8_t *dst, short *src,
			 unsigned long nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, gboolean swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount=0;
  short *ptr;
  short *src_end;

  if(!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  // take care of (rounding errors ?)
  src_end=src+tbytes/sizeof(short)-nSrcChannels;

  while (nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;

    ccount=0;
    
    /* loop until all of our destination channels are filled */
    while(nDstCount) {
      nSrcCount--;
      nDstCount--;
      
      ptr=src+ccount+src_offset_i;
      if (scale<0.f) {
	ptr=ptr>src?ptr:src;
      }
      else {
	ptr=ptr<src_end?ptr:src_end;
      }
      
      /* copy the data over */
      *(dst++) = swap_sign?(uint8_t)(((int8_t)(*ptr>>8))+128):(int8_t)(*ptr>>8);
	ccount++;
	
	/* if we ran out of source channels but not destination channels */
	/* then start the src channels back where we were */
	if(!nSrcCount && nDstCount) {
	  ccount=0;
	  nSrcCount = nSrcChannels;
	}
      }

    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}


/* convert from 16 bit to floating point */
/* channels to a buffer that will hold a single channel stream */
/* src_skip is in terms of 16bit samples */
void sample_move_d16_float (float *dst, short *src, unsigned long nsamples, unsigned long src_skip, int is_unsigned, float vol) {
  register float svol;
  float val;

#ifdef ENABLE_OIL
  double x;
  double y=0.f;
#endif

  if (vol==0.) vol=0.0000001f;
  svol=SAMPLE_MAX_16BIT/vol;

#ifdef ENABLE_OIL
  x=1./svol;
#endif

  while (nsamples--) {
    if (!is_unsigned) {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_s16(&val,src,1,&y,&x);
#else
      if ((val = (float)((*src) / svol))>1.0f) val=1.0f;
      else if (val<-1.0f) val=-1.0f;
#endif
    }
    else {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_u16(&val,(unsigned short *)src,1,&y,&x);
#else
      if ((val=((float)((unsigned short)(*src)) / SAMPLE_MAX_16BIT - 1.0f)*vol)>1.0f) val=1.0f;
      else if (val<-1.0f) val=-1.0f;
#endif
    }
    *(dst++)=val;
    src += src_skip;
  }

}



long sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, float scale, int chans, int asamps, int usigned, int swap_endian, float vol) {
  // convert float samples back to int
  long frames_out=0l;
  short val;
  unsigned short valu=0;
  register int i;
  register int offs=0,coffs=0;
  register float coffs_f=0.f;
  short *hbuffs=(short *)holding_buff;
  unsigned short *hbuffu=(unsigned short *)holding_buff;
  unsigned char *hbuffc=(guchar *)holding_buff;

  while ((nsamps-coffs)>0) {
    frames_out++;
    for (i=0;i<chans;i++) {
      val=(short)(*(float_buffer[i]+coffs)*vol*SAMPLE_MAX_16BIT);
      if (usigned) {
	valu=val+SAMPLE_MAX_16BITI;
      }
      if (asamps==16) {
	if (!swap_endian) {
	  if (usigned) *(hbuffu+offs)=(float)valu*vol;
	  else *(hbuffs+offs)=(float)val*vol;
	}
	else {
	  *(hbuffc+offs)=(float)(val&0x00FF)*vol;
	  *(hbuffc+(++offs))=(float)((val&0xFF00)>>8)*vol;
	}
      }
      else {
	if (usigned) *(hbuffc+offs)=(guchar)((float)(valu>>8)*vol);
	else *(hbuffc+offs)=(guchar)((float)val/256.*vol);
      }
      offs++;
    }
    coffs=(gint)(coffs_f+=scale);
  }
  return frames_out;
}




// play from memory buffer

long sample_move_abuf_float (float **obuf, int nchans, int nsamps, int out_arate, float vol) {

  int samples_out=0;

#ifdef ENABLE_JACK

  int samps=0;

  lives_audio_buf_t *abuf;
  int in_arate;
  register size_t offs=0,ioffs,xchan;

  register float src_offset_f=0.f;
  register int src_offset_i=0;

  register int i,j;

  register double scale;

  size_t curval;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->jackd->read_abuf==-1) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return 0;
  }
  abuf=mainw->jackd->abufs[mainw->jackd->read_abuf];
  in_arate=abuf->arate;
  pthread_mutex_unlock(&mainw->abuf_mutex);
  scale=(double)in_arate/(double)out_arate;

  while (nsamps>0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->jackd->read_abuf==-1) {
      pthread_mutex_unlock(&mainw->abuf_mutex);
      return 0;
    }

    ioffs=abuf->start_sample;
    pthread_mutex_unlock(&mainw->abuf_mutex);
    samps=0;

    src_offset_i=0;
    src_offset_f=0.;

    for (i=0;i<nsamps;i++) {
      // process each sample

      if ((curval=ioffs+src_offset_i)>=abuf->samples_filled) {
	// current buffer is full
	break;
      }
      xchan=0;
      for (j=0;j<nchans;j++) {
	pthread_mutex_lock(&mainw->abuf_mutex);
	if (mainw->jackd->read_abuf<0) {
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	  return samples_out;
	}
	if (xchan>=abuf->achans) xchan=0; 
	obuf[j][offs+i]=abuf->data.floatbuf[xchan][curval]*vol;
	pthread_mutex_unlock(&mainw->abuf_mutex);
	xchan++;
      }
      src_offset_i=(int)(src_offset_f+=scale);
      samps++;
      samples_out++;
    }
    
    abuf->start_sample=ioffs+src_offset_i;
    nsamps-=samps;
    offs+=samps;

    if (nsamps>0) {
      // buffer was filled, move on to next buffer

      pthread_mutex_lock(&mainw->abuf_mutex);
      // request main thread to fill another buffer
      mainw->abufs_to_fill++;

      if (mainw->jackd->read_abuf<0) {
	// playback ended while we were processing
	pthread_mutex_unlock(&mainw->abuf_mutex);
	return samples_out;
      }

      mainw->jackd->read_abuf++;

      if (mainw->jackd->read_abuf>=prefs->num_rtaudiobufs) mainw->jackd->read_abuf=0;

      abuf=mainw->jackd->abufs[mainw->jackd->read_abuf];
      
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }

  }
#endif

  return samples_out;
}




long sample_move_abuf_int16 (short *obuf, int nchans, int nsamps, int out_arate) {

  int samples_out=0;

#ifdef HAVE_PULSE_AUDIO

  int samps=0;

  lives_audio_buf_t *abuf;
  int in_arate,nsampsx;
  register size_t offs=0,ioffs,xchan;

  register float src_offset_f=0.f;
  register int src_offset_i=0;

  register int i,j;

  register double scale;
  size_t curval;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->pulsed->read_abuf==-1) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return 0;
  }

  abuf=mainw->pulsed->abufs[mainw->pulsed->read_abuf];
  in_arate=abuf->arate;
  pthread_mutex_unlock(&mainw->abuf_mutex);
  scale=(double)in_arate/(double)out_arate;

  while (nsamps>0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->pulsed->read_abuf==-1) {
      pthread_mutex_unlock(&mainw->abuf_mutex);
      return 0;
    }

    ioffs=abuf->start_sample;
    pthread_mutex_unlock(&mainw->abuf_mutex);
    samps=0;

    src_offset_i=0;
    src_offset_f=0.;
    nsampsx=nsamps*nchans;

    for (i=0;i<nsampsx;i+=nchans) {
      // process each sample

      if ((curval=ioffs+src_offset_i)>=abuf->samples_filled) {
	// current buffer is drained
	break;
      }
      xchan=0;
      curval*=abuf->achans;
      for (j=0;j<nchans;j++) {
	pthread_mutex_lock(&mainw->abuf_mutex);
	if (mainw->pulsed->read_abuf<0) {
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	  return samps;
	}
	if (xchan>=abuf->achans) xchan=0;
	obuf[(offs++)]=abuf->data.int16buf[curval+xchan];
	pthread_mutex_unlock(&mainw->abuf_mutex);
	xchan++;
      }
      src_offset_i=(int)(src_offset_f+=scale);
      samps++;
    }
    
    abuf->start_sample=ioffs+src_offset_i;
    nsamps-=samps;
    samples_out+=samps;

    if (nsamps>0) {
      // buffer was drained, move on to next buffer

      pthread_mutex_lock(&mainw->abuf_mutex);
      // request main thread to fill another buffer
      mainw->abufs_to_fill++;

      if (mainw->pulsed->read_abuf<0) {
	// playback ended while we were processing
	pthread_mutex_unlock(&mainw->abuf_mutex);
	return samples_out;
      }

      mainw->pulsed->read_abuf++;

      if (mainw->pulsed->read_abuf>=prefs->num_rtaudiobufs) mainw->pulsed->read_abuf=0;

      abuf=mainw->pulsed->abufs[mainw->pulsed->read_abuf];
      
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }

  }
#endif

  return samples_out;
}





/// copy a memory chunk into an audio buffer, applying overall volume control


static size_t chunk_to_float_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int chans=abuf->achans;
  register size_t offs=abuf->samples_filled;
  register int i;

  for (i=0;i<chans;i++) {
    w_memcpy(&(abuf->data.floatbuf[i][offs]),float_buffer[i],nsamps*sizeof(float));
  }
  return (size_t)nsamps;
}


// for pulse audio we use int16, and let it apply the volume

static size_t chunk_to_int16_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int frames_out=0;
  int chans=abuf->achans;
  register size_t offs=abuf->samples_filled*chans;
  register int i;

  while (frames_out<nsamps) {
    for (i=0;i<chans;i++) {
      abuf->data.int16buf[offs+i]=(short)(float_buffer[i][frames_out]*SAMPLE_MAX_16BIT);
    }
    frames_out++;
    offs+=chans;
  }
  return (size_t)frames_out;
}





//#define DEBUG_ARENDER

static void pad_with_silence(int out_fd, off64_t oins_size, long ins_size) {
  // fill to ins_pt with zeros
  guchar *zero_buff;
  size_t sblocksize=SILENCE_BLOCK_SIZE;
  gint sbytes=ins_size-oins_size;
  int i;

#ifdef DEBUG_ARENDER
  g_print("sbytes is %d\n",sbytes);
#endif
  if (sbytes>0) {
    lseek64(out_fd,oins_size,SEEK_SET);
    zero_buff=g_malloc0(SILENCE_BLOCK_SIZE);
    for (i=0;i<sbytes;i+=SILENCE_BLOCK_SIZE) {
      if (sbytes-i<SILENCE_BLOCK_SIZE) sblocksize=sbytes-i;
      dummyvar=write (out_fd,zero_buff,sblocksize);
    }
    g_free(zero_buff);
  }
  else if (sbytes<=0) {
    lseek64(out_fd,oins_size+sbytes,SEEK_SET);
  }
}




long render_audio_segment(gint nfiles, gint *from_files, gint to_file, gdouble *avels, gdouble *fromtime, weed_timecode_t tc_start, weed_timecode_t tc_end, gdouble *chvol, gdouble opvol_start, gdouble opvol_end, lives_audio_buf_t *obuf) {
  // called during multitrack rendering to create the actual audio file
  // (or in-memory buffer for preview playback)

  // also used for fade-in/fade-out in the clip editor (opvol_start, opvol_end)

  // in multitrack, chvol is taken from the audio mixer; opvol is always 1.

  // what we will do here:
  // calculate our target samples (= period * out_rate)

  // calculate how many in_samples for each track (= period * in_rate / ABS (vel) )

  // read in the relevant number of samples for each track and convert to float

  // write this into our float buffers (1 buffer per channel per track)

  // we then send small chunks at a time to our audio volume/pan effect; this is to allow for parameter interpolation

  // the small chunks are processed and mixed, converted from float to int, and then written to the outfile

  // if obuf != NULL we write to float to obuf instead


  // TODO - allow MAX_AUDIO_MEM to be configurable; currently this is fixed at 8 MB
  // 16 or 32 may be a more sensible default for realtime previewing


  // return (audio) frames rendered

  file *outfile=to_file>-1?mainw->files[to_file]:NULL;


  size_t tbytes;
  guchar *in_buff;

  gint out_asamps=to_file>-1?outfile->asampsize/8:0;
  gint out_achans=to_file>-1?outfile->achans:obuf->achans;
  gint out_arate=to_file>-1?outfile->arate:obuf->arate;
  gint out_unsigned=to_file>-1?outfile->signed_endian&AFORM_UNSIGNED:0;
  gint out_bendian=to_file>-1?outfile->signed_endian&AFORM_BIG_ENDIAN:0;

  short *holding_buff;
  float *float_buffer[out_achans*nfiles];

  float *chunk_float_buffer[out_achans*nfiles];

  int c,x;
  register int i,j;
  size_t bytes_read;
  int in_fd[nfiles],out_fd=-1;
  gulong nframes;
  gboolean in_reverse_endian[nfiles],out_reverse_endian=FALSE;
  off64_t seekstart[nfiles];
  gchar *infilename,*outfilename;
  weed_timecode_t tc=tc_start;
  gdouble ins_pt=tc/U_SEC;
  long ins_size=0l,cur_size;
  gdouble time=0.;
  gdouble opvol=opvol_start;
  long frames_out=0;

  int track;

  gint in_asamps[nfiles];
  gint in_achans[nfiles];
  gint in_arate[nfiles];
  gint in_unsigned[nfiles];
  gint in_bendian;

  gboolean is_silent[nfiles];
  gint first_nonsilent=-1;

  long tsamples=((tc_end-tc_start)/U_SEC*out_arate+.5);

  long blocksize,zsamples,xsamples;

  void *finish_buff;

  weed_plant_t *shortcut=NULL;

  size_t max_aud_mem,bytes_to_read,aud_buffer;
  gint max_segments;
  gdouble zavel,zavel_max=0.;

  long tot_frames=0l;

  if (out_achans==0) return 0l;

  if (to_file>-1) {
    // prepare outfile stuff
    outfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,outfile->handle);
#ifdef DEBUG_ARENDER
    g_print("writing to %s\n",outfilename);
#endif
    out_fd=open(outfilename,O_WRONLY|O_CREAT|O_SYNC,S_IRUSR|S_IWUSR);
    g_free(outfilename);
    
    cur_size=get_file_size(out_fd);

    ins_pt*=out_achans*out_arate*out_asamps;
    ins_size=((long)(ins_pt/out_achans/out_asamps+.5))*out_achans*out_asamps;

    if ((!out_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(out_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) out_reverse_endian=TRUE;
    else out_reverse_endian=FALSE;
    
    // fill to ins_pt with zeros
    pad_with_silence(out_fd,cur_size,ins_size);
    sync();


  }
  else {

    if (mainw->event_list!=NULL) cfile->aseek_pos=fromtime[0];

    tc_end-=tc_start;
    tc_start=0;

    if (tsamples>obuf->sample_space-obuf->samples_filled) tsamples=obuf->sample_space-obuf->samples_filled;

  }

#ifdef DEBUG_ARENDER
  g_print("here %d %lld %lld %d\n",nfiles,tc_start,tc_end,to_file);
#endif

  for (track=0;track<nfiles;track++) {
    // prepare infile stuff
    file *infile;

#ifdef DEBUG_ARENDER
    g_print(" track %d %d %.4f %.4f\n",track,from_files[track],fromtime[track],avels[track]);
#endif

    if (avels[track]==0.) {
      is_silent[track]=TRUE;
      continue;
    }

    is_silent[track]=FALSE;
    
    infile=mainw->files[from_files[track]];

    in_asamps[track]=infile->asampsize/8;
    in_achans[track]=infile->achans;
    in_arate[track]=infile->arate;
    in_unsigned[track]=infile->signed_endian&AFORM_UNSIGNED;
    in_bendian=infile->signed_endian&AFORM_BIG_ENDIAN;

    if ((!in_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(in_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) in_reverse_endian[track]=TRUE;
    else in_reverse_endian[track]=FALSE;

    seekstart[track]=(off64_t)(fromtime[track]*in_arate[track])*in_achans[track]*in_asamps[track];
    seekstart[track]=((off64_t)(seekstart[track]/in_achans[track]/(in_asamps[track])))*in_achans[track]*in_asamps[track];
    
    zavel=avels[track]*(gdouble)in_arate[track]/(gdouble)out_arate*in_asamps[track]*in_achans[track]/sizeof(float);
    if (ABS(zavel)>zavel_max) zavel_max=ABS(zavel);

    infilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,infile->handle);
    in_fd[track]=open(infilename,O_RDONLY);
    
    lseek64(in_fd[track],seekstart[track],SEEK_SET);

    g_free(infilename);
  }

  for (track=0;track<nfiles;track++) {
    if (!is_silent[track]) {
      first_nonsilent=track;
      break;
    }
  }

  if (first_nonsilent==-1) {

    // output silence
    if (to_file>-1) {
      long oins_size=ins_size;
      ins_pt=tc_end/U_SEC;
      ins_pt*=out_achans*out_arate*out_asamps;
      ins_size=((long)(ins_pt/out_achans/out_asamps)+.5)*out_achans*out_asamps;
      pad_with_silence(out_fd,oins_size,ins_size);
      sync();
      close (out_fd);
    }
    else {
      for (i=0;i<out_achans;i++) {
	for (j=obuf->samples_filled;j<obuf->samples_filled+tsamples;j++) {
	  if (prefs->audio_player==AUD_PLAYER_JACK) {
	    obuf->data.floatbuf[i][j]=0.;
	  }
	  else {
	    obuf->data.int16buf[j*out_achans+i]=0;
	  }
	}
      }
      obuf->samples_filled+=tsamples;
    }
    return tsamples;
  }

  // we don't want to use more than MAX_AUDIO_MEM bytes
  // (numbers will be much larger than examples given)

  max_aud_mem=MAX_AUDIO_MEM/(1.5+zavel_max); // allow for size of holding_buff and in_buff

  max_aud_mem=(max_aud_mem>>7)<<7; // round to a multiple of 128 

  max_aud_mem=max_aud_mem/out_achans/nfiles; // max mem per channel/track

  bytes_to_read=tsamples*(sizeof(float)); // eg. 120 (20 samples)

  max_segments=(int)((gdouble)bytes_to_read/(gdouble)max_aud_mem+1.); // max segments (rounded up) [e.g ceil(120/45)==3]
  aud_buffer=bytes_to_read/max_segments;  // estimate of buffer size (e.g. 120/3 = 40)

  zsamples=(int)(aud_buffer/sizeof(float)+.5); // ensure whole number of samples (e.g 40 == 10 samples)

  xsamples=zsamples+(tsamples-(max_segments*zsamples)); // e.g 30 - 3 * 10 == 0

  holding_buff=(short *)g_malloc(xsamples*sizeof(short)*out_achans);
  
  for (i=0;i<out_achans*nfiles;i++) {
    float_buffer[i]=g_malloc(xsamples*sizeof(float));
  }

  finish_buff=g_malloc(RENDER_BLOCK_SIZE*out_achans*out_asamps);

  for (i=0;i<nfiles;i++) {
    for (c=0;c<out_achans;c++) {
      chunk_float_buffer[i*out_achans+c]=g_malloc(RENDER_BLOCK_SIZE*sizeof(float));
    }
  }

#ifdef DEBUG_ARENDER
  g_print("  rendering %ld samples\n",tsamples);
#endif

  while (tsamples>0) {
    tsamples-=xsamples;

    for (track=0;track<nfiles;track++) {
      if (is_silent[track]) {
	// zero float_buff
	for(c=0;c<out_achans;c++) {
	  for (x=0;x<xsamples;x++) {
	    float_buffer[track*out_achans+c][x]=0.;
	  }
	}
	continue;
      }

      // calculate tbytes for xsamples

      zavel=avels[track]*(gdouble)in_arate[track]/(gdouble)out_arate;

      tbytes=(gdouble)xsamples*ABS(zavel)*in_asamps[track]*in_achans[track];
      tbytes=((size_t)(tbytes/in_achans[track]/(in_asamps[track])))*in_achans[track]*(in_asamps[track]);

      in_buff=g_malloc(tbytes);

      if (zavel<0.) lseek64(in_fd[track],seekstart[track]-tbytes,SEEK_SET);
    
      bytes_read=read(in_fd[track],in_buff,tbytes);
      //g_print("read %ld bytes\n",bytes_read);

      if (zavel<0.) seekstart[track]-=tbytes;

      if (bytes_read<tbytes) memset(in_buff+bytes_read,0,tbytes-bytes_read);

      nframes=tbytes/(in_asamps[track])/in_achans[track]/ABS(zavel);

      // convert to float
      if (in_asamps[track]==1) {
	sample_move_d8_d16 (holding_buff,(guchar *)in_buff,nframes,tbytes,zavel,out_achans,in_achans[track]);
      }
      else {
	sample_move_d16_d16(holding_buff,(short*)in_buff,nframes,tbytes,zavel,out_achans,in_achans[track],in_reverse_endian[track],FALSE);
      }

      g_free(in_buff);

      for(c=0;c<out_achans;c++) {
	sample_move_d16_float(float_buffer[c+track*out_achans],holding_buff+c,nframes,out_achans,in_unsigned[track],chvol[track]);
      }
    }

    // now we send small chunks at a time to the audio vol/pan effect
    
    blocksize=RENDER_BLOCK_SIZE;

    for (i=0;i<xsamples;i+=RENDER_BLOCK_SIZE) {
      if (i+RENDER_BLOCK_SIZE>xsamples) blocksize=xsamples-i;

      for (track=0;track<nfiles;track++) {
	for (c=0;c<out_achans;c++) {
	  //g_print("xvals %.4f\n",*(float_buffer[track*out_achans+c]+i));
	  w_memcpy(chunk_float_buffer[track*out_achans+c],float_buffer[track*out_achans+c]+i,blocksize*sizeof(float));
	}
      }

      // apply audio filter(s)
      if (mainw->multitrack!=NULL) {
	// we work out the "visibility" of each track at tc
	gdouble *vis=get_track_visibility_at_tc(mainw->multitrack->event_list,nfiles,mainw->multitrack->opts.back_audio_tracks,tc,&shortcut,mainw->multitrack->opts.audio_bleedthru);
	
	// locate the master volume parameter, and multiply all values by vis[track]
	weed_apply_audio_effects(mainw->filter_map,chunk_float_buffer,mainw->multitrack->opts.back_audio_tracks,out_achans,blocksize,out_arate,tc,vis);
	
	g_free(vis);
      }
      
      if (to_file>-1) {
	// convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
	frames_out=sample_move_float_int((void *)finish_buff,chunk_float_buffer,blocksize,1.,out_achans,out_asamps*8,out_unsigned,out_reverse_endian,opvol);
	g_print("size is now %ld\n",get_file_size(out_fd));
	dummyvar=write (out_fd,finish_buff,frames_out*out_asamps*out_achans);
#ifdef DEBUG_ARENDER
	g_print(".");
#endif
	sync();
      }
      else {
	if (prefs->audio_player==AUD_PLAYER_JACK) {
	  frames_out=chunk_to_float_abuf(obuf,chunk_float_buffer,blocksize);
	}
	else {
	  frames_out=chunk_to_int16_abuf(obuf,chunk_float_buffer,blocksize);
	}
	obuf->samples_filled+=frames_out;
      }

      if (mainw->multitrack==NULL) {
	time+=(gdouble)frames_out/(gdouble)out_arate;
	opvol=opvol_start+(opvol_end-opvol_start)*(time/((tc_end-tc_start)/U_SEC));
      }
      else tc+=(gdouble)blocksize/(gdouble)out_arate*U_SEC;
    }
    xsamples=zsamples;

    tot_frames+=frames_out;
  }

  for (i=0;i<out_achans*nfiles;i++) {
    g_free(float_buffer[i]);
  }

  g_free(finish_buff);
  g_free(holding_buff);

  // close files
  for (track=0;track<nfiles;track++) {
    if (!is_silent[track]) {
      close (in_fd[track]);
    }
    for (c=0;c<out_achans;c++) {
      g_free(chunk_float_buffer[track*out_achans+c]);
    }
  }

  if (to_file>-1) {
#ifdef DEBUG_ARENDER
    g_print("fs is %ld\n",get_file_size(out_fd));
#endif
    close (out_fd);
  }

  return tot_frames;
}


inline void aud_fade(gint fileno, gdouble startt, gdouble endt, gdouble startv, gdouble endv) {
  gdouble vel=1.,vol=1.;
  render_audio_segment(1,&fileno,fileno,&vel,&startt,startt*U_SECL,endt*U_SECL,&vol,startv,endv,NULL);
}



#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(gint fileno, gboolean is_window_grab) {
  // open audio file for writing
  file *outfile=mainw->files[fileno];
  gchar *outfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,outfile->handle);
  gint out_bendian=outfile->signed_endian&AFORM_BIG_ENDIAN;

  mainw->aud_rec_fd=open(outfilename,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
  g_free(outfilename);

  mainw->jackd_read=jack_get_driver(0,FALSE);
  mainw->jackd_read->playing_file=fileno;
  mainw->jackd_read->frames_written=0;

  if ((!out_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(out_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->jackd_read->reverse_endian=TRUE;
  else mainw->jackd_read->reverse_endian=FALSE;

  // start jack recording
  jack_open_device_read(mainw->jackd_read);
  jack_read_driver_activate(mainw->jackd_read);

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (is_window_grab) return;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_SOFT;
  // show countdown/stop dialog
  d_print(_("Recording audio..."));
  do_auto_dialog(_("Recording audio"),1);
  jack_rec_audio_end();
}



void jack_rec_audio_end(void) {
  // recording ended

  // stop recording
  jack_close_device(mainw->jackd_read,TRUE);
  mainw->jackd_read=NULL;

  // close file
  close(mainw->aud_rec_fd);
  mainw->cancel_type=CANCEL_KILL;
}

#endif




#ifdef HAVE_PULSE_AUDIO
void pulse_rec_audio_to_clip(gint fileno, gboolean is_window_grab) {
  // open audio file for writing
  file *outfile=mainw->files[fileno];
  gchar *outfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,outfile->handle);
  gint out_bendian=outfile->signed_endian&AFORM_BIG_ENDIAN;

  mainw->aud_rec_fd=open(outfilename,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
  g_free(outfilename);

  mainw->pulsed_read=pulse_get_driver(FALSE);
  mainw->pulsed_read->playing_file=fileno;
  mainw->pulsed_read->frames_written=0;

  if ((!out_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(out_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->pulsed_read->reverse_endian=TRUE;
  else mainw->pulsed_read->reverse_endian=FALSE;

  // start pulse recording
  pulse_driver_activate(mainw->pulsed_read);

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (is_window_grab) return;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_SOFT;
  // show countdown/stop dialog
  d_print(_("Recording audio..."));
  do_auto_dialog(_("Recording audio"),1);
  pulse_rec_audio_end();
}



void pulse_rec_audio_end(void) {
  // recording ended

  // stop recording
  pulse_close_client(mainw->pulsed_read);

  pulse_flush_read_data(mainw->pulsed_read,0,NULL);

  mainw->pulsed_read=NULL;

  // close file
  close(mainw->aud_rec_fd);
  mainw->cancel_type=CANCEL_KILL;
}

#endif




/////////////////////////////////////////////////////////////////

// playback via memory buffers (e.g. in multitrack)




////////////////////////////////////////////////////////////////


static lives_audio_track_state_t *resize_audstate(lives_audio_track_state_t *ostate, int nostate, int nstate) {
  lives_audio_track_state_t *audstate=(lives_audio_track_state_t *)g_malloc(nstate*sizeof(lives_audio_track_state_t));
  int i;

  for (i=0;i<nstate;i++) {
    if (i<nostate) {
      audstate[i].afile=ostate[i].afile;
      audstate[i].seek=ostate[i].seek;
      audstate[i].vel=ostate[i].vel;
    }
    else {
      audstate[i].afile=0;
      audstate[i].seek=audstate[i].vel=0.;
    }
  }

  if (ostate!=NULL) g_free(ostate);

  return audstate;
}
	





static lives_audio_track_state_t *aframe_to_atstate(weed_plant_t *event) {
  int error,atrack;
  int num_aclips=weed_leaf_num_elements(event,"audio_clips");
  int *aclips=weed_get_int_array(event,"audio_clips",&error);
  double *aseeks=weed_get_double_array(event,"audio_seeks",&error);
  int naudstate=0;
  lives_audio_track_state_t *atstate=NULL;
  
  register int i;

  int btoffs=mainw->multitrack!=NULL?mainw->multitrack->opts.back_audio_tracks:1;

  for (i=0;i<num_aclips;i+=2) {
    if (aclips[i+1]>0) { // else ignore
      atrack=aclips[i];
      if (atrack+btoffs>=naudstate-1) {
	atstate=resize_audstate(atstate,naudstate,atrack+btoffs+2);
	naudstate=atrack+btoffs+1;
	atstate[naudstate].afile=-1;
      }
      atstate[atrack+btoffs].afile=aclips[i+1];
      atstate[atrack+btoffs].seek=aseeks[i];
      atstate[atrack+btoffs].vel=aseeks[i+1];
    }
  }

  weed_free(aclips);
  weed_free(aseeks);

  return atstate;
}




lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event, gboolean get_audstate, gboolean exact) {

  // if exact is set, we must rewind back to first active stateful effect, 
  // and play forwards from there (not yet implemented - TODO)

  weed_plant_t *nevent=get_first_event(event_list),*event;
  lives_audio_track_state_t *atstate=NULL,*audstate=NULL;
  weed_plant_t *deinit_event;
  int error,nfiles,nnfiles;
  weed_timecode_t last_tc=0,fill_tc;
  int i;

  // optionally: gets audio state at audio frame prior to st_event, sets atstate[0].tc
  // and initialises audio buffers

  // also gets effects state (initing any effects which should be active)


  fill_tc=get_event_timecode(st_event);

  do {
    event=nevent;
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      mainw->filter_map=event;
    }
    else if (WEED_EVENT_IS_FILTER_INIT(event)) {
      deinit_event=weed_get_voidptr_value(event,"deinit_event",&error);
      if (get_event_timecode(deinit_event)>=fill_tc) {
	// this effect should be activated
	process_events(event,get_event_timecode(event));
      }
    }
    else if (get_audstate&&weed_plant_has_leaf(event,"audio_clips")) {

      atstate=aframe_to_atstate(event);

      if (audstate==NULL) audstate=atstate;
      else {
	// have an existing audio state, update with current
	for (nfiles=0;audstate[nfiles].afile!=-1;nfiles++);

	for (i=0;i<nfiles;i++) {
	  // increase seek values up to current frame
	  audstate[i].seek+=audstate[i].vel*(get_event_timecode(event)-last_tc)/U_SEC;
	}

	for (nnfiles=0;atstate[nnfiles].afile!=-1;nnfiles++);

	if (nnfiles>nfiles) {
	  audstate=resize_audstate(audstate,nfiles,nnfiles+1);
	  audstate[nnfiles].afile=-1;
	}

	for (i=0;i<nnfiles;i++) {
	  if (atstate[i].afile>0) {
	    audstate[i].afile=atstate[i].afile;
	    audstate[i].seek=atstate[i].seek;
	    audstate[i].vel=atstate[i].vel;
	  }
	}                                                             
	g_free(atstate);
      }

      last_tc=get_event_timecode(event);
    }
    nevent=get_next_event(event);
  } while (event!=st_event);


  if (audstate!=NULL) {
    for (nfiles=0;audstate[nfiles].afile!=-1;nfiles++);

    for (i=0;i<nfiles;i++) {
      // increase seek values
      audstate[i].seek+=audstate[i].vel*(fill_tc-last_tc)/U_SEC;
    }

  }


  return audstate;

}



void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, gboolean exact) {
  // fill audio buffer with audio samples, using event_list as a guide
  // if st_event!=NULL, that is our start event, and we will calculate the audio state at that
  // point

  // otherwise, we continue from where we left off the last time


  // all we really do here is set from_files,aseeks and avels arrays and call render_audio_segment

  lives_audio_track_state_t *atstate;
  int nnfiles,i;
  gdouble chvols[65536]; // TODO - use list

  static weed_timecode_t last_tc;
  static weed_timecode_t fill_tc;
  static weed_plant_t *event;
  static int nfiles;

  static int *from_files=NULL;
  static double *aseeks=NULL,*avels=NULL;

  gboolean is_cont=FALSE;

  abuf->samples_filled=0; // write fill level of buffer
  abuf->start_sample=0; // read level

  if (st_event!=NULL) {
    event=st_event;
    last_tc=get_event_timecode(event);

    if (from_files!=NULL) g_free(from_files);
    if (avels!=NULL) g_free(avels);
    if (aseeks!=NULL) g_free(aseeks);

    if (mainw->multitrack!=NULL) nfiles=weed_leaf_num_elements(mainw->multitrack->avol_init_event,"in_tracks");
    else nfiles=1;

    from_files=(int *)g_malloc(nfiles*sizint);
    avels=(double *)g_malloc(nfiles*sizdbl);
    aseeks=(double *)g_malloc(nfiles*sizdbl);

    for (i=0;i<nfiles;i++) {
      from_files[i]=0;
      avels[i]=aseeks[i]=0.;
    }

    atstate=get_audio_and_effects_state_at(event_list,event,TRUE,exact);
    
    if (atstate!=NULL) {
      
      for (nnfiles=0;atstate[nnfiles].afile!=-1;nnfiles++);
      
      for (i=0;i<nnfiles;i++) {
	if (atstate[i].afile>0) {
	  from_files[i]=atstate[i].afile;
	  avels[i]=atstate[i].vel;
	  aseeks[i]=atstate[i].seek;
	}
      }
      
      g_free(atstate);
    }
  }
  else {
    is_cont=TRUE;
  }

  if (mainw->multitrack!=NULL) {
    for (i=0;i<nfiles;i++) {
      if (mainw->multitrack!=NULL&&mainw->multitrack->audio_vols!=NULL) {
	chvols[i]=(gdouble)GPOINTER_TO_INT(g_list_nth_data(mainw->multitrack->audio_vols,i))/1000000.;
      }
    }
  }
  else chvols[0]=1.;

  fill_tc=last_tc+(gdouble)(abuf->sample_space)/(gdouble)abuf->arate*U_SEC;

  // continue until either we have a full buffer, or we reach next audio frame
  while (event!=NULL&&get_event_timecode(event)<=fill_tc) {
    if (!is_cont) event=get_next_frame_event(event);
    if (event!=NULL&&weed_plant_has_leaf(event,"audio_clips")) {
      // got next audio frame
      weed_timecode_t tc=get_event_timecode(event);
      if (tc>=fill_tc) break;

      tc+=(U_SEC/cfile->fps*!is_blank_frame(event,FALSE));

      render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, tc, chvols, 1., 1., abuf);

      for (i=0;i<nfiles;i++) {
	// increase seek values
	aseeks[i]+=avels[i]*(tc-last_tc)/U_SEC;
      }

      last_tc=tc;

      // process audio updates at this frame
      atstate=aframe_to_atstate(event);

      if (atstate!=NULL) {
	for (nnfiles=0;atstate[nnfiles].afile!=-1;nnfiles++);

	for (i=0;i<nnfiles;i++) {
	  if (atstate[i].afile>0) {
	    from_files[i]=atstate[i].afile;
	    avels[i]=atstate[i].vel;
	    aseeks[i]=atstate[i].seek;
	  }
	}
	g_free(atstate);
      }
    }
    is_cont=FALSE;
  }
  
  if (last_tc<fill_tc) {
    render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, fill_tc, chvols, 1., 1., abuf);

    for (i=0;i<nfiles;i++) {
      // increase seek values
      aseeks[i]+=avels[i]*(fill_tc-last_tc)/U_SEC;
    }
  }

  
  mainw->write_abuf++;
  if (mainw->write_abuf>=prefs->num_rtaudiobufs) mainw->write_abuf=0;

  last_tc=fill_tc;

  if (mainw->abufs_to_fill) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    mainw->abufs_to_fill--;
    pthread_mutex_unlock(&mainw->abuf_mutex);
  }

}






void init_jack_audio_buffers (gint achans, gint arate, gboolean exact) {
#ifdef ENABLE_JACK

  int i,chan;

  mainw->jackd->abufs=(lives_audio_buf_t **)g_malloc(prefs->num_rtaudiobufs*sizeof(lives_audio_buf_t *));
  
  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    mainw->jackd->abufs[i]=(lives_audio_buf_t *)g_malloc(sizeof(lives_audio_buf_t));
    
    mainw->jackd->abufs[i]->achans=achans;
    mainw->jackd->abufs[i]->arate=arate;
    mainw->jackd->abufs[i]->sample_space=XSAMPLES;
    mainw->jackd->abufs[i]->data.floatbuf=g_malloc(achans*sizeof(float *));
    for (chan=0;chan<achans;chan++) {
      mainw->jackd->abufs[i]->data.floatbuf[chan]=g_malloc(XSAMPLES*sizeof(float));
    }
  }
#endif
}


void init_pulse_audio_buffers (gint achans, gint arate, gboolean exact) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  mainw->pulsed->abufs=(lives_audio_buf_t **)g_malloc(prefs->num_rtaudiobufs*sizeof(lives_audio_buf_t *));
  
  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    mainw->pulsed->abufs[i]=(lives_audio_buf_t *)g_malloc(sizeof(lives_audio_buf_t));
    
    mainw->pulsed->abufs[i]->achans=achans;
    mainw->pulsed->abufs[i]->arate=arate;
    mainw->pulsed->abufs[i]->sample_space=XSAMPLES;  // sample_space here is in stereo samples
    mainw->pulsed->abufs[i]->data.int16buf=g_malloc(XSAMPLES*achans*sizeof(short));
  }
#endif
}



void free_jack_audio_buffers(void) {
#ifdef ENABLE_JACK

  int i,chan;

  if (mainw->jackd==NULL) return;

  if (mainw->jackd->abufs==NULL) return;

  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    if (mainw->jackd->abufs[i]!=NULL) {
      for (chan=0;chan<mainw->jackd->abufs[i]->achans;chan++) {
	g_free(mainw->jackd->abufs[i]->data.floatbuf[chan]);
      }
      g_free(mainw->jackd->abufs[i]->data.floatbuf);
      g_free(mainw->jackd->abufs[i]);
   }
  }
#endif
}


void free_pulse_audio_buffers(void) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  if (mainw->pulsed==NULL) return;

  if (mainw->pulsed->abufs==NULL) return;

  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    if (mainw->pulsed->abufs[i]!=NULL) {
      g_free(mainw->pulsed->abufs[i]->data.int16buf);
      g_free(mainw->pulsed->abufs[i]);
    }
  }
#endif
}




//////////////////////////////////////////////////////////////////////////





