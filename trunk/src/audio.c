// audio.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2014
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#include <weed/weed-effects.h>
#include <weed/weed-palettes.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-palettes.h"
#endif

#include "main.h"
#include "audio.h"
#include "events.h"
#include "callbacks.h"
#include "effects.h"
#include "support.h"

static char *storedfnames[NSTOREDFDS];
static int storedfds[NSTOREDFDS];
static boolean storedfdsset=FALSE;

static void audio_reset_stored_fnames(void) {
  int i;
  for (i=0; i<NSTOREDFDS; i++) {
    storedfnames[i]=NULL;
    storedfds[i]=-1;
  }
  storedfdsset=TRUE;
}



void audio_free_fnames(void) {
  // cleanup stored filehandles after playback/fade/render

  int i;

  if (!storedfdsset) return;

  for (i=0; i<NSTOREDFDS; i++) {
    if (storedfnames!=NULL) {
      lives_free(storedfnames[i]);
      storedfnames[i]=NULL;
      if (storedfds[i]>-1) close(storedfds[i]);
      storedfds[i]=-1;
    }
  }
}


void append_to_audio_bufferf(lives_audio_buf_t *abuf, float *src, uint64_t nsamples, int channum) {
  // append float audio to the audio frame buffer
  size_t nsampsize;

  if (!prefs->push_audio_to_gens) return;

  if (abuf->bufferf==NULL) free_audio_frame_buffer(abuf);

  nsampsize=(abuf->samples_filled+nsamples)*sizeof(float);

  channum++;
  if (channum>abuf->out_achans) {
    register int i;
    abuf->bufferf=(float **)lives_realloc(abuf->bufferf,channum*sizeof(float *));
    for (i=abuf->out_achans; i<channum; i++) {
      abuf->bufferf[i]=NULL;
    }
    abuf->out_achans=channum;
  }
  channum--;
  abuf->bufferf[channum]=(float *)lives_realloc(abuf->bufferf[channum],nsampsize*sizeof(float));
  lives_memcpy(&abuf->bufferf[channum][abuf->samples_filled],src,nsamples*sizeof(float));
}



void append_to_audio_buffer16(lives_audio_buf_t *abuf, void *src, uint64_t nsamples, int channum) {
  // append 16 bit audio to the audio frame buffer
  size_t nsampsize;

  if (!prefs->push_audio_to_gens) return;

  if (abuf->buffer16==NULL) free_audio_frame_buffer(abuf);

  nsampsize=(abuf->samples_filled+nsamples)*2;
  channum++;
  if (abuf->buffer16==NULL||channum>abuf->out_achans) {
    register int i;
    abuf->buffer16=(int16_t **)lives_realloc(abuf->buffer16,channum*sizeof(int16_t *));
    for (i=abuf->out_achans; i<channum; i++) {
      abuf->buffer16[i]=NULL;
    }
    abuf->out_achans=channum;
  }
  channum--;
  abuf->buffer16[channum]=(int16_t *)lives_realloc(abuf->buffer16[channum],nsampsize*2);
  lives_memcpy(&abuf->buffer16[channum][abuf->samples_filled],src,nsamples*2);
#ifdef DEBUG_AFB
  g_print("append16 to afb\n");
#endif
}



void init_audio_frame_buffer(short aplayer) {
  // function should be called when the first video generator with audio input is enabled

  lives_audio_buf_t *abuf;
  abuf=mainw->audio_frame_buffer=(lives_audio_buf_t *)lives_malloc0(sizeof(lives_audio_buf_t));

  abuf->samples_filled=0;
  abuf->swap_endian=FALSE;
  abuf->out_achans=0;

  switch (aplayer) {
#ifdef HAVE_PULSE_AUDIO
  case AUD_PLAYER_PULSE:
    abuf->in_interleaf=TRUE;
    abuf->s16_signed=TRUE;
    abuf->arate=mainw->pulsed->out_arate;
    break;
#endif
#ifdef ENABLE_JACK
  case AUD_PLAYER_JACK:
    abuf->in_interleaf=FALSE;
    abuf->out_interleaf=FALSE;
    abuf->arate=mainw->jackd->sample_out_rate;
    break;
#endif
  default:
    break;
  }
#ifdef DEBUG_AFB
  g_print("init afb\n");
#endif
}



void free_audio_frame_buffer(lives_audio_buf_t *abuf) {
  // function should be called to clear samples
  register int i;
  if (abuf!=NULL) {
    if (abuf->bufferf!=NULL) {
      for (i=0; i<abuf->out_achans; i++) lives_free(abuf->bufferf[i]);
      lives_free(abuf->bufferf);
      abuf->bufferf=NULL;
    }
    if (abuf->buffer32!=NULL) {
      for (i=0; i<abuf->out_achans; i++) lives_free(abuf->buffer32[i]);
      lives_free(abuf->buffer32);
      abuf->buffer32=NULL;
    }
    if (abuf->buffer24!=NULL) {
      for (i=0; i<abuf->out_achans; i++) lives_free(abuf->buffer24[i]);
      lives_free(abuf->buffer24);
      abuf->buffer24=NULL;
    }
    if (abuf->buffer16!=NULL) {
      for (i=0; i<abuf->out_achans; i++) lives_free(abuf->buffer16[i]);
      lives_free(abuf->buffer16);
      abuf->buffer16=NULL;
    }
    if (abuf->buffer8!=NULL) {
      for (i=0; i<abuf->out_achans; i++) lives_free(abuf->buffer8[i]);
      lives_free(abuf->buffer8);
      abuf->buffer8=NULL;
    }

    abuf->samples_filled=0;
    abuf->out_achans=0;
  }
#ifdef DEBUG_AFB
  g_print("clear afb\n");
#endif
}



LIVES_INLINE void sample_silence_dS(float *dst, uint64_t nsamples) {
  // send silence to the jack player
  memset(dst,0,nsamples*sizeof(float));
}


void sample_move_d8_d16(short *dst, uint8_t *src,
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  // convert 8 bit audio to 16 bit audio

  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount;
  unsigned char *ptr;
  unsigned char *src_end;

  // take care of rounding errors
  src_end=src+tbytes-nSrcChannels;

  if (!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  while (nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;
    ccount=0;

    /* loop until all of our destination channels are filled */
    while (nDstCount) {
      nSrcCount--;
      nDstCount--;

      ptr=src+ccount+src_offset_i;
      ptr=ptr>src?(ptr<(src_end+ccount)?ptr:(src_end+ccount)):src;

      if (!swap_sign) *(dst++) = *(ptr)<<8;
      else if (swap_sign==SWAP_U_TO_S) *(dst++)=((short)(*(ptr))-128)<<8;
      else *((unsigned short *)(dst++))=((short)(*(ptr))+128)<<8;
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if (!nSrcCount && nDstCount) {
        ccount=0;
        nSrcCount = nSrcChannels;
      }
    }

    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}



/* convert from any number of source channels to any number of destination channels - both interleaved */
void sample_move_d16_d16(int16_t *dst, int16_t *src,
                         uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_endian, int swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount=0;
  int16_t *ptr;
  int16_t *src_end;

  if (!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  // take care of rounding errors
  src_end=src+tbytes/sizeof(short)-nSrcChannels;

  while (nsamples--) {

    if ((nSrcCount = nSrcChannels)==(nDstCount = nDstChannels)&&!swap_endian&&!swap_sign) {
      // same number of channels

      ptr=src+src_offset_i;
      ptr=ptr>src?(ptr<src_end?ptr:src_end):src;

      lives_memcpy(dst,ptr,nSrcChannels*2);
      dst+=nDstCount;
    } else {
      ccount=0;

      /* loop until all of our destination channels are filled */
      while (nDstCount) {
        nSrcCount--;
        nDstCount--;

        ptr=src+ccount+src_offset_i;
        ptr=ptr>src?(ptr<(src_end+ccount)?ptr:(src_end+ccount)):src;

        /* copy the data over */
        if (!swap_endian) {
          if (!swap_sign) *(dst++) = *ptr;
          else if (swap_sign==SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(*ptr+SAMPLE_MAX_16BITI);
          else *(dst++)=*ptr-SAMPLE_MAX_16BITI;
        } else if (swap_endian==SWAP_X_TO_L) {
          if (!swap_sign) *(dst++)=(((*ptr)&0x00FF)<<8)+((*ptr)>>8);
          else if (swap_sign==SWAP_S_TO_U) *((uint16_t *)dst++)=(uint16_t)(((*ptr&0x00FF)<<8)+(*ptr>>8)+SAMPLE_MAX_16BITI);
          else *(dst++)=((*ptr&0x00FF)<<8)+(*ptr>>8)-SAMPLE_MAX_16BITI;
        } else {
          if (!swap_sign) *(dst++)=(((*ptr)&0x00FF)<<8)+((*ptr)>>8);
          else if (swap_sign==SWAP_S_TO_U) *((uint16_t *)dst++)=(uint16_t)(((((uint16_t)(*ptr+SAMPLE_MAX_16BITI))&0x00FF)<<8)+
                (((uint16_t)(*ptr+SAMPLE_MAX_16BITI))>>8));
          else *(dst++)=((((int16_t)(*ptr-SAMPLE_MAX_16BITI))&0x00FF)<<8)+(((int16_t)(*ptr-SAMPLE_MAX_16BITI))>>8);
        }

        ccount++;

        /* if we ran out of source channels but not destination channels */
        /* then start the src channels back where we were */
        if (!nSrcCount && nDstCount) {
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
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount=0;
  short *ptr;
  short *src_end;

  if (!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  src_end=src+tbytes/sizeof(short)-nSrcChannels;

  while (nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;

    ccount=0;

    /* loop until all of our destination channels are filled */
    while (nDstCount) {
      nSrcCount--;
      nDstCount--;

      ptr=src+ccount+src_offset_i;
      ptr=ptr>src?(ptr<(src_end+ccount)?ptr:src_end+ccount):src;

      /* copy the data over */
      if (!swap_sign) *(dst++) = (*ptr>>8);
      else if (swap_sign==SWAP_S_TO_U) *(dst++) = (uint8_t)((int8_t)(*ptr>>8)+128);
      else *((int8_t *)dst++) = (int8_t)((uint8_t)(*ptr>>8)-128);
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if (!nSrcCount && nDstCount) {
        ccount=0;
        nSrcCount = nSrcChannels;
      }
    }

    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}



void sample_move_d16_float(float *dst, short *src, uint64_t nsamples, uint64_t src_skip, int is_unsigned, boolean rev_endian, float vol) {
  // convert 16 bit audio to float audio

  register float svolp,svoln;

#ifdef ENABLE_OIL
  float val=0.;  // set a value to stop valgrind complaining
  double xn,xp,xa;
  double y=0.f;
#else
  register float val;
  register short valss;
#endif

  uint8_t srcx[2];
  short srcxs;
  short *srcp;

  if (vol==0.) vol=0.0000001f;
  svolp=SAMPLE_MAX_16BIT_P/vol;
  svoln=SAMPLE_MAX_16BIT_N/vol;

#ifdef ENABLE_OIL
  xp=1./svolp;
  xn=1./svoln;
  xa=2.*vol/(SAMPLE_MAX_16BIT_P+SAMPLE_MAX_16BIT_N);
#endif

  while (nsamples--) {

    if (rev_endian) {
      memcpy(&srcx,src+1,2);
      srcxs=(srcx[0]<<8) + srcx[1];
      srcp=&srcxs;
    } else srcp=src;

    if (!is_unsigned) {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_s16(&val,srcp,1,&y,val>0?&xp:&xn);
#else
      if ((val = (float)((float)(*srcp) / (*srcp>0?svolp:svoln)))>1.0f) val=1.0f;
      else if (val<-1.0f) val=-1.0f;
#endif
    } else {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_u16(&val,(unsigned short *)srcp,1,&y,&xa);
      val-=vol;
#else
      valss=(unsigned short)*srcp-SAMPLE_MAX_16BITI;
      if ((val = (float)((float)(valss) / (valss>0?svolp:svoln)))>1.0f) val=1.0f;
      else if (val<-1.0f) val=-1.0f;
#endif
    }

    *(dst++)=val;
    src += src_skip;
  }

}



void sample_move_float_float(float *dst, float *src, uint64_t nsamples, float scale, int dst_skip) {
  // copy one channel of float to a buffer, applying the scale (scale 2.0 to double the rate, etc)
  volatile size_t offs=0;
  float offs_f=0.;
  register int i;

  for (i=0; i<nsamples; i++) {
    *dst=src[offs];
    dst+=dst_skip;
    offs=(size_t)(offs_f+=scale);
  }
}



int64_t sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, float scale, int chans, int asamps,
                              int usigned, boolean little_endian, boolean interleaved, float vol) {
  // convert float samples back to int
  // interleaved is for the float buffer; output int is always interleaved
  int64_t frames_out=0l;
  register int i;
  register int offs=0,coffs=0;
  register float coffs_f=0.f;
  short *hbuffs=(short *)holding_buff;
  unsigned short *hbuffu=(unsigned short *)holding_buff;
  unsigned char *hbuffc=(unsigned char *)holding_buff;

  register short val;
  register unsigned short valu=0;
  register float valf;


  while ((nsamps-coffs)>0) {
    frames_out++;
    for (i=0; i<chans; i++) {
      valf=*(float_buffer[i]+(interleaved?(coffs*chans):coffs));
      valf*=vol;
      val=(short)(valf*(valf>0.?SAMPLE_MAX_16BIT_P:SAMPLE_MAX_16BIT_N));

      if (usigned) valu=(val+SAMPLE_MAX_16BITI);

      if (asamps==16) {
        if (!little_endian) {
          if (usigned) *(hbuffu+offs)=valu;
          else *(hbuffs+offs)=val;
        } else {
          if (usigned) {
            *(hbuffc+offs)=valu&0x00FF;
            *(hbuffc+(++offs))=(valu&0xFF00)>>8;
          } else {
            *(hbuffc+offs)=val&0x00FF;
            *(hbuffc+(++offs))=(val&0xFF00)>>8;
          }
        }
      } else {
        *(hbuffc+offs)=(unsigned char)((float)val/256.);
      }
      offs++;
    }
    coffs=(int)(coffs_f+=scale);
  }
  return frames_out;
}




// play from memory buffer

int64_t sample_move_abuf_float(float **obuf, int nchans, int nsamps, int out_arate, float vol) {

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

    for (i=0; i<nsamps; i++) {
      // process each sample

      if ((curval=ioffs+src_offset_i)>=abuf->samples_filled) {
        // current buffer is consumed
        break;
      }
      xchan=0;
      for (j=0; j<nchans; j++) {
        // copy channel by channel (de-interleave)
        pthread_mutex_lock(&mainw->abuf_mutex);
        if (mainw->jackd->read_abuf<0) {
          pthread_mutex_unlock(&mainw->abuf_mutex);
          return samples_out;
        }
        if (xchan>=abuf->out_achans) xchan=0;
        obuf[j][offs+i]=abuf->bufferf[xchan][curval]*vol;
        pthread_mutex_unlock(&mainw->abuf_mutex);
        xchan++;
      }
      // resample on the fly
      src_offset_i=(int)(src_offset_f+=scale);
      samps++;
      samples_out++;
    }

    abuf->start_sample=ioffs+src_offset_i;
    nsamps-=samps;
    offs+=samps;

    if (nsamps>0) {
      // buffer was consumed, move on to next buffer

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




int64_t sample_move_abuf_int16(short *obuf, int nchans, int nsamps, int out_arate) {

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

    for (i=0; i<nsampsx; i+=nchans) {
      // process each sample

      if ((curval=ioffs+src_offset_i)>=abuf->samples_filled) {
        // current buffer is drained
        break;
      }
      xchan=0;
      curval*=abuf->out_achans;
      for (j=0; j<nchans; j++) {
        pthread_mutex_lock(&mainw->abuf_mutex);
        if (mainw->pulsed->read_abuf<0) {
          pthread_mutex_unlock(&mainw->abuf_mutex);
          return samps;
        }
        if (xchan>=abuf->out_achans) xchan=0;
        obuf[(offs++)]=abuf->buffer16[0][curval+xchan];
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





/// copy a memory chunk into an audio buffer


static size_t chunk_to_float_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int chans=abuf->out_achans;
  register size_t offs=abuf->samples_filled;
  register int i;

  for (i=0; i<chans; i++) {
    lives_memcpy(&(abuf->bufferf[i][offs]),float_buffer[i],nsamps*sizeof(float));
  }
  return (size_t)nsamps;
}


boolean float_deinterleave(float *fbuffer, int nsamps, int nchans) {
  // deinterleave a float buffer
  register int i,j;

  float *tmpfbuffer=(float *)lives_try_malloc(nsamps*nchans*sizeof(float));
  if (tmpfbuffer==NULL) return FALSE;

  for (i=0; i<nsamps; i++) {
    for (j=0; j<nchans; j++) {
      tmpfbuffer[nsamps*j+i]=fbuffer[i*nchans+j];
    }
  }
  lives_memcpy(fbuffer,tmpfbuffer,nsamps*nchans*sizeof(float));
  lives_free(tmpfbuffer);
  return TRUE;
}


boolean float_interleave(float *fbuffer, int nsamps, int nchans) {
  // deinterleave a float buffer
  register int i,j;

  float *tmpfbuffer=(float *)lives_try_malloc(nsamps*nchans*sizeof(float));
  if (tmpfbuffer==NULL) return FALSE;

  for (i=0; i<nsamps; i++) {
    for (j=0; j<nchans; j++) {
      tmpfbuffer[i*nchans+j]=fbuffer[j*nsamps+i];
    }
  }
  lives_memcpy(fbuffer,tmpfbuffer,nsamps*nchans*sizeof(float));
  lives_free(tmpfbuffer);
  return TRUE;
}



// for pulse audio we use int16, and let it apply the volume

static size_t chunk_to_int16_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int frames_out=0;
  int chans=abuf->out_achans;
  register size_t offs=abuf->samples_filled*chans;
  register int i;
  register float valf;

  while (frames_out<nsamps) {
    for (i=0; i<chans; i++) {
      valf=float_buffer[i][frames_out];
      abuf->buffer16[0][offs+i]=(short)(valf*(valf>0.?SAMPLE_MAX_16BIT_P:SAMPLE_MAX_16BIT_N));
    }
    frames_out++;
    offs+=chans;
  }
  return (size_t)frames_out;
}





//#define DEBUG_ARENDER

static boolean pad_with_silence(int out_fd, off64_t oins_size, int64_t ins_size, int asamps, int aunsigned, boolean big_endian) {
  // fill to ins_pt with zeros (or 0x80.. for unsigned)
  uint8_t *zero_buff;
  size_t sblocksize=SILENCE_BLOCK_SIZE;
  int sbytes=ins_size-oins_size;
  register int i;

  boolean retval=TRUE;

#ifdef DEBUG_ARENDER
  g_print("sbytes is %d\n",sbytes);
#endif
  if (sbytes>0) {
    lseek64(out_fd,oins_size,SEEK_SET);
    if (!aunsigned) zero_buff=(uint8_t *)lives_malloc0(SILENCE_BLOCK_SIZE);
    else {
      zero_buff=(uint8_t *)lives_malloc(SILENCE_BLOCK_SIZE);
      if (asamps==1) memset(zero_buff,0x80,SILENCE_BLOCK_SIZE);
      else {
        for (i=0; i<SILENCE_BLOCK_SIZE; i+=2) {
          if (big_endian) {
            memset(zero_buff+i,0x80,1);
            memset(zero_buff+i+1,0x00,1);
          } else {
            memset(zero_buff+i,0x00,1);
            memset(zero_buff+i+1,0x80,1);
          }
        }
      }
    }

    for (i=0; i<sbytes; i+=SILENCE_BLOCK_SIZE) {
      if (sbytes-i<SILENCE_BLOCK_SIZE) sblocksize=sbytes-i;
      mainw->write_failed=FALSE;
      lives_write(out_fd,zero_buff,sblocksize,TRUE);
      if (mainw->write_failed) retval=FALSE;
    }
    lives_free(zero_buff);
  } else if (sbytes<=0) {
    lseek64(out_fd,oins_size+sbytes,SEEK_SET);
  }
  return retval;
}




static void audio_process_events_to(weed_timecode_t tc) {
  // process events from mainw->audio_event up to tc
  // at playback start we should have set mainw->audio_event and mainw->afilter_map

  int hint,error;

  weed_plant_t *event=mainw->audio_event;

  weed_timecode_t ctc;


  while (event!=NULL && get_event_timecode(event)<=tc) {
    hint=get_event_hint(event);
    ctc=get_event_timecode(event);

    switch (hint) {

    case WEED_EVENT_HINT_FILTER_INIT: {
      weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(event,"deinit_event",&error);
      if (get_event_timecode(deinit_event)<tc) break;
      process_events(event,TRUE,ctc);
    }
    break;
    case WEED_EVENT_HINT_FILTER_DEINIT: {
      weed_plant_t *init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
      if (weed_plant_has_leaf(init_event,"host_tag")) {
        process_events(event,TRUE,ctc);
      }
    }
    break;
    case WEED_EVENT_HINT_FILTER_MAP:
#ifdef DEBUG_EVENTS
      g_print("got new audio effect map\n");
#endif
      mainw->afilter_map=event;
      break;
    default:
      break;
    }
    event=get_next_event(event);
  }

  mainw->audio_event=event;

}



int64_t render_audio_segment(int nfiles, int *from_files, int to_file, double *avels, double *fromtime,
                             weed_timecode_t tc_start, weed_timecode_t tc_end, double *chvol, double opvol_start,
                             double opvol_end, lives_audio_buf_t *obuf) {
  // called during multitrack rendering to create the actual audio file
  // (or in-memory buffer for preview playback in multitrack)

  // also used for fade-in/fade-out in the clip editor (opvol_start, opvol_end)

  // in multitrack, chvol is taken from the audio mixer; opvol is always 1.

  // what we will do here:
  // calculate our target samples (= period * out_rate)

  // calculate how many in_samples for each track (= period * in_rate / ABS (vel) )

  // read in the relevant number of samples for each track and convert to float

  // write this into our float buffers (1 buffer per channel per track)

  // we then send small chunks at a time to any audio effects; this is to allow for parameter interpolation

  // the small chunks are processed and mixed, converted from float to int, and then written to the outfile

  // if obuf != NULL we write to obuf instead


  // TODO - allow MAX_AUDIO_MEM to be configurable; currently this is fixed at 8 MB
  // 16 or 32 may be a more sensible default for realtime previewing


  // return (audio) frames rendered

  lives_clip_t *outfile=to_file>-1?mainw->files[to_file]:NULL;


  size_t tbytes;
  uint8_t *in_buff;

  int out_asamps=to_file>-1?outfile->asampsize/8:0;
  int out_achans=to_file>-1?outfile->achans:obuf->out_achans;
  int out_arate=to_file>-1?outfile->arate:obuf->arate;
  int out_unsigned=to_file>-1?outfile->signed_endian&AFORM_UNSIGNED:0;
  int out_bendian=to_file>-1?outfile->signed_endian&AFORM_BIG_ENDIAN:0;

  short *holding_buff;
  float *float_buffer[out_achans*nfiles];

  float *chunk_float_buffer[out_achans*nfiles];

  int c,x;
  register int i,j;
  ssize_t bytes_read;

  int in_fd[nfiles],out_fd=-1;

  uint64_t nframes;

  boolean in_reverse_endian[nfiles],out_reverse_endian=FALSE;

  off64_t seekstart[nfiles];
  char *infilename,*outfilename;

  weed_timecode_t tc=tc_start;

  double ins_pt=tc/U_SEC;
  double time=0.;
  double opvol=opvol_start;
  double *vis=NULL;

  int64_t frames_out=0;
  int64_t ins_size=0l,cur_size;

  int track;

  int in_asamps[nfiles];
  int in_achans[nfiles];
  int in_arate[nfiles];
  int in_unsigned[nfiles];
  int in_bendian;

  boolean is_silent[nfiles];
  int first_nonsilent=-1;

  int64_t tsamples=((tc_end-tc_start)/U_SEC*out_arate+.5);

  int64_t blocksize,zsamples,xsamples;

  void *finish_buff;

  weed_plant_t *shortcut=NULL;

  size_t max_aud_mem,bytes_to_read,aud_buffer;
  int max_segments;
  double zavel,zavel_max=0.;

  int64_t tot_frames=0l;

  int render_block_size=RENDER_BLOCK_SIZE;

  if (out_achans*nfiles*tsamples==0) return 0l;

  if (!storedfdsset) audio_reset_stored_fnames();

  if (mainw->event_list==NULL||(mainw->multitrack==NULL&&nfiles==1&&from_files[0]==mainw->ascrap_file)) render_block_size*=100;

  if (to_file>-1) {
    // prepare outfile stuff
    outfilename=lives_build_filename(prefs->tmpdir,outfile->handle,"audio",NULL);
#ifdef DEBUG_ARENDER
    g_print("writing to %s\n",outfilename);
#endif
    out_fd=open(outfilename,O_WRONLY|O_CREAT|O_SYNC,S_IRUSR|S_IWUSR);
    lives_free(outfilename);

    if (out_fd<0) {
      if (mainw->write_failed_file!=NULL) lives_free(mainw->write_failed_file);
      mainw->write_failed_file=lives_strdup(outfilename);
      mainw->write_failed=TRUE;
      return 0l;
    }

#ifdef IS_MINGW
    setmode(out_fd, O_BINARY);
#endif

    cur_size=get_file_size(out_fd);

    ins_pt*=out_achans*out_arate*out_asamps;
    ins_size=((int64_t)(ins_pt/out_achans/out_asamps+.5))*out_achans*out_asamps;

    if ((!out_bendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||
        (out_bendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN)))
      out_reverse_endian=TRUE;
    else out_reverse_endian=FALSE;

    // fill to ins_pt with zeros
    pad_with_silence(out_fd,cur_size,ins_size,out_asamps,out_unsigned,out_bendian);

  } else {

    if (mainw->event_list!=NULL) cfile->aseek_pos=fromtime[0];

    tc_end-=tc_start;
    tc_start=0;

    if (tsamples>obuf->samp_space-obuf->samples_filled) tsamples=obuf->samp_space-obuf->samples_filled;

  }

#ifdef DEBUG_ARENDER
  g_print("here %d %lld %lld %d\n",nfiles,tc_start,tc_end,to_file);
#endif

  for (track=0; track<nfiles; track++) {
    // prepare infile stuff
    lives_clip_t *infile;

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

    if (LIVES_UNLIKELY(in_achans[track]==0)) is_silent[track]=TRUE;
    else {
      if ((!in_bendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||
          (in_bendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN)))
        in_reverse_endian[track]=TRUE;
      else in_reverse_endian[track]=FALSE;

      seekstart[track]=(off64_t)(fromtime[track]*in_arate[track])*in_achans[track]*in_asamps[track];
      seekstart[track]=((off64_t)(seekstart[track]/in_achans[track]/(in_asamps[track])))*in_achans[track]*in_asamps[track];

      zavel=avels[track]*(double)in_arate[track]/(double)out_arate*in_asamps[track]*in_achans[track]/sizeof(float);
      if (ABS(zavel)>zavel_max) zavel_max=ABS(zavel);

      infilename=lives_build_filename(prefs->tmpdir,infile->handle,"audio",NULL);


      // try to speed up access by keeping some files open
      if (track<NSTOREDFDS&&storedfnames[track]!=NULL&&!strcmp(infilename,storedfnames[track])) {
        in_fd[track]=storedfds[track];
      } else {
        if (track<NSTOREDFDS&&storedfds[track]>-1) close(storedfds[track]);
        in_fd[track]=open(infilename,O_RDONLY);
        if (in_fd[track]<0) {
          if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
          mainw->read_failed_file=lives_strdup(infilename);
          mainw->read_failed=TRUE;
        }

#ifdef IS_MINGW
        setmode(in_fd[track], O_BINARY);
#endif
        if (track<NSTOREDFDS) {
          storedfds[track]=in_fd[track];
          storedfnames[track]=lives_strdup(infilename);
        }
      }

      if (in_fd[track]>-1) lseek64(in_fd[track],seekstart[track],SEEK_SET);

      lives_free(infilename);
    }
  }

  for (track=0; track<nfiles; track++) {
    if (!is_silent[track]) {
      first_nonsilent=track;
      break;
    }
  }

  if (first_nonsilent==-1) {
    // all in tracks are empty
    // output silence
    if (to_file>-1) {
      int64_t oins_size=ins_size;
      ins_pt=tc_end/U_SEC;
      ins_pt*=out_achans*out_arate*out_asamps;
      ins_size=((int64_t)(ins_pt/out_achans/out_asamps)+.5)*out_achans*out_asamps;
      pad_with_silence(out_fd,oins_size,ins_size,out_asamps,out_unsigned,out_bendian);
      //sync();
      close(out_fd);
    } else {
      for (i=0; i<out_achans; i++) {
        for (j=obuf->samples_filled; j<obuf->samples_filled+tsamples; j++) {
          if (prefs->audio_player==AUD_PLAYER_JACK) {
            obuf->bufferf[i][j]=0.;
          } else {
            if (!out_unsigned) obuf->buffer16[0][j*out_achans+i]=0;
            else {
              if (out_bendian) obuf->buffer16[0][j*out_achans+i]=0x8000;
              else obuf->buffer16[0][j*out_achans+i]=0x80;
            }
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

  // we use float here because our audio effects use float
  // tsamples is total samples (30 in this example)
  bytes_to_read=tsamples*(sizeof(float)); // eg. 120 (30 samples)

  // how many segments do we need to read all bytes ?
  max_segments=(int)((double)bytes_to_read/(double)max_aud_mem+1.); // max segments (rounded up) [e.g ceil(120/45)==3]

  // then, how many bytes per segment
  aud_buffer=bytes_to_read/max_segments;  // estimate of buffer size (e.g. 120/3 = 40)

  zsamples=(int)(aud_buffer/sizeof(float)+.5); // ensure whole number of samples (e.g 40 == 10 samples), round up

  xsamples=zsamples+(tsamples-(max_segments*zsamples)); // e.g 10 + 30 - 3 * 10 == 10

  holding_buff=(short *)lives_malloc(xsamples*sizeof(short)*out_achans);

  for (i=0; i<out_achans*nfiles; i++) {
    float_buffer[i]=(float *)lives_malloc(xsamples*sizeof(float));
  }

  finish_buff=lives_malloc(tsamples*out_achans*out_asamps);

#ifdef DEBUG_ARENDER
  g_print("  rendering %ld samples\n",tsamples);
#endif

  while (tsamples>0) {
    tsamples-=xsamples;

    for (track=0; track<nfiles; track++) {
      if (is_silent[track]) {
        // zero float_buff
        for (c=0; c<out_achans; c++) {
          for (x=0; x<xsamples; x++) {
            float_buffer[track*out_achans+c][x]=0.;
          }
        }
        continue;
      }

      // calculate tbytes for xsamples

      zavel=avels[track]*(double)in_arate[track]/(double)out_arate;

      tbytes=(int)((double)xsamples*ABS(zavel)+((double)fastrand()/(double)LIVES_MAXUINT32))*
             in_asamps[track]*in_achans[track];

      in_buff=(uint8_t *)lives_malloc(tbytes);

      if (zavel<0. && in_fd[track]>-1) lseek64(in_fd[track],seekstart[track]-tbytes,SEEK_SET);

      bytes_read=0;
      mainw->read_failed=FALSE;
      if (in_fd[track]>-1) bytes_read=lives_read(in_fd[track],in_buff,tbytes,TRUE);

      if (bytes_read<0) bytes_read=0;

      if (from_files[track]==mainw->ascrap_file) {
        // be forgiving with the ascrap file
        if (mainw->read_failed) mainw->read_failed=FALSE;
      }

      if (zavel<0.) seekstart[track]-=bytes_read;

      if (bytes_read<tbytes&&bytes_read>=0) {
        if (zavel>0) memset(in_buff+bytes_read,0,tbytes-bytes_read);
        else {
          memmove(in_buff+tbytes-bytes_read,in_buff,bytes_read);
          memset(in_buff,0,tbytes-bytes_read);
        }
      }

      nframes=(tbytes/(in_asamps[track])/in_achans[track]/ABS(zavel)+.001);

      // convert to float
      if (in_asamps[track]==1) {
        sample_move_d8_d16(holding_buff,(uint8_t *)in_buff,nframes,tbytes,zavel,out_achans,in_achans[track],0);
      } else {
        sample_move_d16_d16(holding_buff,(short *)in_buff,nframes,tbytes,zavel,out_achans,
                            in_achans[track],in_reverse_endian[track]?SWAP_X_TO_L:0,0);
      }

      lives_free(in_buff);

      for (c=0; c<out_achans; c++) {
        sample_move_d16_float(float_buffer[c+track*out_achans],holding_buff+c,nframes,
                              out_achans,in_unsigned[track],FALSE,chvol[track]);
      }
    }

    // now we send small chunks at a time to the audio vol/pan effect
    shortcut=NULL;
    blocksize=render_block_size;

    for (i=0; i<xsamples; i+=render_block_size) {
      if (i+render_block_size>xsamples) blocksize=xsamples-i;

      for (track=0; track<nfiles; track++) {
        for (c=0; c<out_achans; c++) {
          //g_print("xvals %.4f\n",*(float_buffer[track*out_achans+c]+i));
          chunk_float_buffer[track*out_achans+c]=float_buffer[track*out_achans+c]+i;
        }
      }

      if (mainw->event_list!=NULL&&mainw->filter_map!=NULL&&!(mainw->multitrack==NULL&&from_files[0]==mainw->ascrap_file)) {
        // we need to apply all audio effects with output here.
        // even in clipedit mode (for preview/rendering with an event list)
        // also, we will need to keep updating the mainw->filter_map from mainw->event_list, as filters may switched on and off during the block

        int nbtracks=0;

        // apply audio filter(s)
        if (mainw->multitrack!=NULL) {
          // we work out the "visibility" of each track at tc

          vis=get_track_visibility_at_tc(mainw->multitrack->event_list,nfiles,
                                         mainw->multitrack->opts.back_audio_tracks,tc,&shortcut,
                                         mainw->multitrack->opts.audio_bleedthru);

          // first track is ascrap_file - flag that no effects should be applied to it, except for the audio mixer
          if (mainw->ascrap_file>-1&&from_files[0]==mainw->ascrap_file) vis[0]=-vis[0];

          nbtracks=mainw->multitrack->opts.back_audio_tracks;
        }

        // process events up to current tc:
        // filter inits and deinits, and filter maps

        audio_process_events_to(tc);

        // locate the master volume parameter, and multiply all values by vis[track]
        weed_apply_audio_effects(mainw->afilter_map,chunk_float_buffer,nbtracks,
                                 out_achans,blocksize,out_arate,tc,vis);

        if (vis!=NULL) lives_free(vis);

      }

      if (mainw->multitrack==NULL&&opvol_end!=opvol_start) {
        time+=(double)frames_out/(double)out_arate;
        opvol=opvol_start+(opvol_end-opvol_start)*(time/(double)((tc_end-tc_start)/U_SEC));
      }

      if (to_file>-1&&mainw->multitrack==NULL&&opvol_start!=opvol_end) {
        // output to file
        // convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
        frames_out=sample_move_float_int((void *)finish_buff,chunk_float_buffer,blocksize,1.,out_achans,
                                         out_asamps*8,out_unsigned,out_reverse_endian,FALSE,opvol);
        lives_write(out_fd,finish_buff,frames_out*out_asamps*out_achans,TRUE);
        tot_frames+=frames_out;

#ifdef DEBUG_ARENDER
        g_print(".");
#endif
      }
      tc+=(double)blocksize/(double)out_arate*U_SEC;
    }

    if (to_file>-1) {
      // output to file
      // convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
      frames_out=sample_move_float_int((void *)finish_buff,float_buffer,xsamples,1.,out_achans,
                                       out_asamps*8,out_unsigned,out_reverse_endian,FALSE,opvol);
      lives_write(out_fd,finish_buff,frames_out*out_asamps*out_achans,TRUE);
#ifdef DEBUG_ARENDER
      g_print(".");
#endif
      tot_frames+=frames_out;
    } else {
      // output to memory buffer
      if (prefs->audio_player==AUD_PLAYER_JACK) {
        frames_out=chunk_to_float_abuf(obuf,float_buffer,xsamples);
      } else {
        frames_out=chunk_to_int16_abuf(obuf,float_buffer,xsamples);
      }
      obuf->samples_filled+=frames_out;
      tot_frames+=frames_out;
    }

    xsamples=zsamples;
  }



  if (xsamples>0) {
    for (i=0; i<out_achans*nfiles; i++) {
      if (float_buffer[i]!=NULL) lives_free(float_buffer[i]);
    }
  }

  if (finish_buff!=NULL) lives_free(finish_buff);
  if (holding_buff!=NULL) lives_free(holding_buff);

  // close files
  for (track=0; track<nfiles; track++) {
    if (!is_silent[track]) {
      if (track>=NSTOREDFDS && in_fd[track]>-1) close(in_fd[track]);
    }
  }

  if (to_file>-1) {
#ifdef DEBUG_ARENDER
    g_print("fs is %ld\n",get_file_size(out_fd));
#endif
    close(out_fd);
  }

  return tot_frames;
}


LIVES_INLINE void aud_fade(int fileno, double startt, double endt, double startv, double endv) {
  double vel=1.,vol=1.;

  mainw->read_failed=mainw->write_failed=FALSE;
  render_audio_segment(1,&fileno,fileno,&vel,&startt,startt*U_SECL,endt*U_SECL,&vol,startv,endv,NULL);

  if (mainw->write_failed) {
    char *outfilename=lives_build_filename(prefs->tmpdir,mainw->files[fileno]->handle,"audio",NULL);
    do_write_failed_error_s(outfilename,NULL);
  }

  if (mainw->read_failed) {
    char *infilename=lives_build_filename(prefs->tmpdir,mainw->files[fileno]->handle,"audio",NULL);
    do_read_failed_error_s(infilename,NULL);
  }

}



#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(int fileno, int old_file, lives_rec_audio_type_t rec_type) {
  // open audio file for writing
  lives_clip_t *outfile;

  boolean jackd_read_started=(mainw->jackd_read!=NULL);

  int retval;

  if (fileno==-1) {
    // respond to external audio, but do not record it (yet)
    mainw->jackd_read=jack_get_driver(0,FALSE);
    mainw->jackd_read->playing_file=fileno;
    mainw->jackd_read->frames_written=0;

    mainw->jackd_read->reverse_endian=FALSE;

    // start jack "recording"
    jack_open_device_read(mainw->jackd_read);
    jack_read_driver_activate(mainw->jackd_read,FALSE);

    return;
  }

  outfile=mainw->files[fileno];

  if (mainw->aud_rec_fd==-1) {
    char *outfilename=lives_build_filename(prefs->tmpdir,outfile->handle,"audio",NULL);
    do {
      retval=0;
      mainw->aud_rec_fd=open(outfilename,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
      if (mainw->aud_rec_fd<0) {
        retval=do_write_failed_error_s_with_retry(outfilename,lives_strerror(errno),NULL);
        if (retval==LIVES_RESPONSE_CANCEL) {
          lives_free(outfilename);
          return;
        }
      }
    } while (retval==LIVES_RESPONSE_RETRY);
    lives_free(outfilename);
  }

  if (rec_type==RECA_GENERATED) {
    mainw->jackd->playing_file=fileno;
  } else {
    if (!jackd_read_started) mainw->jackd_read=jack_get_driver(0,FALSE);
    mainw->jackd_read->playing_file=fileno;
    mainw->jackd_read->frames_written=0;
  }


#ifdef IS_MINGW
  setmode(mainw->aud_rec_fd, O_BINARY);
#endif

  if (rec_type==RECA_EXTERNAL||rec_type==RECA_GENERATED) {
    int asigned;
    int aendian;
    uint64_t fsize=get_file_size(mainw->aud_rec_fd);

    if (rec_type==RECA_EXTERNAL) {
      mainw->jackd_read->reverse_endian=FALSE;

      // start jack recording

      if (!jackd_read_started) {
        jack_open_device_read(mainw->jackd_read);
        jack_read_driver_activate(mainw->jackd_read,FALSE);
      }

      outfile->arate=outfile->arps=mainw->jackd_read->sample_in_rate;
      outfile->achans=mainw->jackd_read->num_input_channels;

      outfile->asampsize=16;
      outfile->signed_endian=get_signed_endian(TRUE,TRUE);

      asigned=!(outfile->signed_endian&AFORM_UNSIGNED);
      aendian=!(outfile->signed_endian&AFORM_BIG_ENDIAN);

      mainw->jackd_read->frames_written=fsize/(outfile->achans*(outfile->asampsize>>3));
    } else {
      mainw->jackd->reverse_endian=FALSE;
      outfile->arate=outfile->arps=mainw->jackd->sample_out_rate;
      outfile->achans=mainw->jackd->num_output_channels;

      outfile->asampsize=16;
      outfile->signed_endian=get_signed_endian(TRUE,TRUE);

      asigned=!(outfile->signed_endian&AFORM_UNSIGNED);
      aendian=!(outfile->signed_endian&AFORM_BIG_ENDIAN);
    }

    save_clip_value(fileno,CLIP_DETAILS_ACHANS,&outfile->achans);
    save_clip_value(fileno,CLIP_DETAILS_ARATE,&outfile->arps);
    save_clip_value(fileno,CLIP_DETAILS_PB_ARATE,&outfile->arate);
    save_clip_value(fileno,CLIP_DETAILS_ASAMPS,&outfile->asampsize);
    save_clip_value(fileno,CLIP_DETAILS_AENDIAN,&aendian);
    save_clip_value(fileno,CLIP_DETAILS_ASIGNED,&asigned);

  } else {

    int out_bendian=outfile->signed_endian&AFORM_BIG_ENDIAN;

    if ((!out_bendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||
        (out_bendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN)))
      mainw->jackd_read->reverse_endian=TRUE;
    else mainw->jackd_read->reverse_endian=FALSE;

    // start jack recording
    jack_open_device_read(mainw->jackd_read);
    jack_read_driver_activate(mainw->jackd_read,TRUE);
  }

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type==RECA_WINDOW_GRAB||rec_type==RECA_EXTERNAL||rec_type==RECA_GENERATED) return;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_SOFT;
  // show countdown/stop dialog
  mainw->suppress_dprint=FALSE;
  d_print(_("Recording audio..."));
  mainw->suppress_dprint=TRUE;
  if (rec_type==RECA_NEW_CLIP) {
    do_auto_dialog(_("Recording audio"),1);
  } else {
    int current_file=mainw->current_file;
    mainw->current_file=old_file;
    on_playsel_activate(NULL,NULL);
    mainw->current_file=current_file;
  }
  jack_rec_audio_end(!(prefs->perm_audio_reader&&prefs->audio_src==AUDIO_SRC_EXT),TRUE);
}



void jack_rec_audio_end(boolean close_device, boolean close_fd) {
  // recording ended

  if (close_device) {
    // stop recording
    if (mainw->jackd_read!=NULL) jack_close_device(mainw->jackd_read);
    mainw->jackd_read=NULL;
  } else mainw->jackd_read->in_use=FALSE;


  if (close_fd&&mainw->aud_rec_fd!=-1) {
    // close file
    close(mainw->aud_rec_fd);
    mainw->aud_rec_fd=-1;
    mainw->cancel_type=CANCEL_KILL;
  }
}
#endif




#ifdef HAVE_PULSE_AUDIO
void pulse_rec_audio_to_clip(int fileno, int old_file, lives_rec_audio_type_t rec_type) {
  // open audio file for writing
  lives_clip_t *outfile;
  int retval;

  if (fileno==-1) {
    mainw->pulsed_read=pulse_get_driver(FALSE);
    mainw->pulsed_read->playing_file=-1;
    mainw->pulsed_read->frames_written=0;

    mainw->pulsed_read->reverse_endian=FALSE;
    mainw->aud_rec_fd=-1;

    pulse_driver_activate(mainw->pulsed_read);

    return;
  }


  outfile=mainw->files[fileno];

  if (mainw->aud_rec_fd==-1) {
    char *outfilename=lives_build_filename(prefs->tmpdir,outfile->handle,"audio",NULL);
    do {
      retval=0;
      mainw->aud_rec_fd=open(outfilename,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
      if (mainw->aud_rec_fd<0) {
        retval=do_write_failed_error_s_with_retry(outfilename,lives_strerror(errno),NULL);
        if (retval==LIVES_RESPONSE_CANCEL) {
          lives_free(outfilename);
          return;
        }
      }
    } while (retval==LIVES_RESPONSE_RETRY);
    lives_free(outfilename);
  }

  if (rec_type==RECA_GENERATED) {
    mainw->pulsed->playing_file=fileno;
  } else {
    mainw->pulsed_read=pulse_get_driver(FALSE);
    mainw->pulsed_read->playing_file=fileno;
    mainw->pulsed_read->frames_written=0;
  }

#ifdef IS_MINGW
  setmode(mainw->aud_rec_fd, O_BINARY);
#endif

  if (rec_type==RECA_EXTERNAL||rec_type==RECA_GENERATED) {
    int asigned;
    int aendian;
    uint64_t fsize=get_file_size(mainw->aud_rec_fd);

    if (rec_type==RECA_EXTERNAL) {
      mainw->pulsed_read->reverse_endian=FALSE;

      pulse_driver_activate(mainw->pulsed_read);

      outfile->arate=outfile->arps=mainw->pulsed_read->out_arate;
      outfile->achans=mainw->pulsed_read->out_achans;
      outfile->asampsize=mainw->pulsed_read->out_asamps;
      outfile->signed_endian=get_signed_endian(mainw->pulsed_read->out_signed!=AFORM_UNSIGNED,
                             mainw->pulsed_read->out_endian!=AFORM_BIG_ENDIAN);

      asigned=!(outfile->signed_endian&AFORM_UNSIGNED);
      aendian=!(outfile->signed_endian&AFORM_BIG_ENDIAN);

      mainw->pulsed_read->frames_written=fsize/(outfile->achans*(outfile->asampsize>>3));
    } else {
      mainw->pulsed->reverse_endian=FALSE;
      outfile->arate=outfile->arps=mainw->pulsed->out_arate;
      outfile->achans=mainw->pulsed->out_achans;
      outfile->asampsize=mainw->pulsed->out_asamps;
      outfile->signed_endian=get_signed_endian(mainw->pulsed->out_signed!=AFORM_UNSIGNED,
                             mainw->pulsed->out_endian!=AFORM_BIG_ENDIAN);

      asigned=!(outfile->signed_endian&AFORM_UNSIGNED);
      aendian=!(outfile->signed_endian&AFORM_BIG_ENDIAN);
    }

    save_clip_value(fileno,CLIP_DETAILS_ACHANS,&outfile->achans);
    save_clip_value(fileno,CLIP_DETAILS_ARATE,&outfile->arps);
    save_clip_value(fileno,CLIP_DETAILS_PB_ARATE,&outfile->arate);
    save_clip_value(fileno,CLIP_DETAILS_ASAMPS,&outfile->asampsize);
    save_clip_value(fileno,CLIP_DETAILS_AENDIAN,&aendian);
    save_clip_value(fileno,CLIP_DETAILS_ASIGNED,&asigned);

  } else {
    int out_bendian=outfile->signed_endian&AFORM_BIG_ENDIAN;

    if ((!out_bendian&&(capable->byte_order==LIVES_BIG_ENDIAN))||
        (out_bendian&&(capable->byte_order==LIVES_LITTLE_ENDIAN)))
      mainw->pulsed_read->reverse_endian=TRUE;
    else mainw->pulsed_read->reverse_endian=FALSE;

    // start pulse recording
    pulse_driver_activate(mainw->pulsed_read);
  }

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type==RECA_WINDOW_GRAB||rec_type==RECA_EXTERNAL||rec_type==RECA_GENERATED) return;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_SOFT;
  // show countdown/stop dialog
  mainw->suppress_dprint=FALSE;
  d_print(_("Recording audio..."));
  mainw->suppress_dprint=TRUE;
  if (rec_type==RECA_NEW_CLIP) do_auto_dialog(_("Recording audio"),1);
  else {
    int current_file=mainw->current_file;
    mainw->current_file=old_file;
    on_playsel_activate(NULL,NULL);
    mainw->current_file=current_file;
  }
  pulse_rec_audio_end(!(prefs->perm_audio_reader&&prefs->audio_src==AUDIO_SRC_EXT),TRUE);
}



void pulse_rec_audio_end(boolean close_device, boolean close_fd) {
  // recording ended

  // stop recording

  if (mainw->pulsed_read!=NULL) {
    pa_threaded_mainloop_lock(mainw->pulsed_read->mloop);

    if (mainw->pulsed_read->playing_file>-1)
      pulse_flush_read_data(mainw->pulsed_read,mainw->pulsed_read->playing_file,0,mainw->pulsed_read->reverse_endian,NULL);

    if (close_device) pulse_close_client(mainw->pulsed_read);

    pa_threaded_mainloop_unlock(mainw->pulsed_read->mloop);

    if (close_device) mainw->pulsed_read=NULL;
    else {
      mainw->pulsed_read->in_use=FALSE;
      mainw->pulsed_read->playing_file=-1;
    }
  }

  if (mainw->aud_rec_fd!=-1&&close_fd) {
    // close file
    close(mainw->aud_rec_fd);
    mainw->aud_rec_fd=-1;
    mainw->cancel_type=CANCEL_KILL;
  }
}

#endif




/////////////////////////////////////////////////////////////////

// playback via memory buffers (e.g. in multitrack)




////////////////////////////////////////////////////////////////


static lives_audio_track_state_t *resize_audstate(lives_audio_track_state_t *ostate, int nostate, int nstate) {
  // increase the element size of the audstate array (ostate)
  // from nostate elements to nstate elements

  lives_audio_track_state_t *audstate=(lives_audio_track_state_t *)lives_malloc(nstate*sizeof(lives_audio_track_state_t));
  int i;

  for (i=0; i<nstate; i++) {
    if (i<nostate) {
      audstate[i].afile=ostate[i].afile;
      audstate[i].seek=ostate[i].seek;
      audstate[i].vel=ostate[i].vel;
    } else {
      audstate[i].afile=0;
      audstate[i].seek=audstate[i].vel=0.;
    }
  }

  if (ostate!=NULL) lives_free(ostate);

  return audstate;
}






static lives_audio_track_state_t *aframe_to_atstate(weed_plant_t *event) {
  // parse an audio frame, and set the track file, seek and velocity values


  int error,atrack;
  int num_aclips=weed_leaf_num_elements(event,"audio_clips");
  int *aclips=weed_get_int_array(event,"audio_clips",&error);
  double *aseeks=weed_get_double_array(event,"audio_seeks",&error);
  int naudstate=0;
  lives_audio_track_state_t *atstate=NULL;

  register int i;

  int btoffs=mainw->multitrack!=NULL?mainw->multitrack->opts.back_audio_tracks:1;


  for (i=0; i<num_aclips; i+=2) {
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

  lives_free(aclips);
  lives_free(aseeks);

  return atstate;
}




lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event,
    boolean get_audstate, boolean exact) {

  // if exact is set, we must rewind back to first active stateful effect,
  // and play forwards from there (not yet implemented - TODO)

  weed_plant_t *nevent=get_first_event(event_list),*event;
  lives_audio_track_state_t *atstate=NULL,*audstate=NULL;
  weed_plant_t *deinit_event;
  int error,nfiles,nnfiles;
  weed_timecode_t last_tc=0,fill_tc;
  int i;

  // gets effects state (initing any effects which should be active)

  // optionally: gets audio state at audio frame prior to st_event, sets atstate[0].tc
  // and initialises audio buffers

  mainw->filter_map=NULL;
  mainw->afilter_map=NULL;

  mainw->audio_event=st_event;

  fill_tc=get_event_timecode(st_event);

  do {
    event=nevent;
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      mainw->afilter_map=mainw->filter_map=event;
    } else if (WEED_EVENT_IS_FILTER_INIT(event)) {
      deinit_event=(weed_plant_t *)weed_get_voidptr_value(event,"deinit_event",&error);
      if (get_event_timecode(deinit_event)>=fill_tc) {
        // this effect should be activated
        process_events(event,FALSE,get_event_timecode(event));
        process_events(event,TRUE,get_event_timecode(event));
      }
    } else if (get_audstate&&WEED_EVENT_IS_AUDIO_FRAME(event)) {

      atstate=aframe_to_atstate(event);

      if (audstate==NULL) audstate=atstate;
      else {
        // have an existing audio state, update with current
        for (nfiles=0; audstate[nfiles].afile!=-1; nfiles++);

        for (i=0; i<nfiles; i++) {
          // increase seek values up to current frame
          audstate[i].seek+=audstate[i].vel*(get_event_timecode(event)-last_tc)/U_SEC;
        }

        for (nnfiles=0; atstate[nnfiles].afile!=-1; nnfiles++);

        if (nnfiles>nfiles) {
          audstate=resize_audstate(audstate,nfiles,nnfiles+1);
          audstate[nnfiles].afile=-1;
        }

        for (i=0; i<nnfiles; i++) {
          if (atstate[i].afile>0) {
            audstate[i].afile=atstate[i].afile;
            audstate[i].seek=atstate[i].seek;
            audstate[i].vel=atstate[i].vel;
          }
        }
        lives_free(atstate);
      }

      last_tc=get_event_timecode(event);
    }
    nevent=get_next_event(event);
  } while (event!=st_event);

  if (audstate!=NULL) {
    for (nfiles=0; audstate[nfiles].afile!=-1; nfiles++);

    for (i=0; i<nfiles; i++) {
      // increase seek values
      audstate[i].seek+=audstate[i].vel*(fill_tc-last_tc)/U_SEC;
    }

  }


  return audstate;

}



void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, boolean exact) {
  // fill audio buffer with audio samples, using event_list as a guide
  // if st_event!=NULL, that is our start event, and we will calculate the audio state at that
  // point

  // otherwise, we continue from where we left off the last time


  // all we really do here is set from_files,aseeks and avels arrays and call render_audio_segment

  lives_audio_track_state_t *atstate=NULL;
  int nnfiles,i;
  double chvols[MAX_AUDIO_TRACKS]; // TODO - use list

  static weed_timecode_t last_tc;
  static weed_timecode_t fill_tc;
  static weed_plant_t *event;
  static int nfiles;

  static int *from_files=NULL;
  static double *aseeks=NULL,*avels=NULL;

  boolean is_cont=FALSE;
  if (abuf==NULL) return;

  abuf->samples_filled=0; // write fill level of buffer
  abuf->start_sample=0; // read level

  if (st_event!=NULL) {
    // this is only called for the first buffered read
    event=st_event;
    last_tc=get_event_timecode(event);

    if (from_files!=NULL) lives_free(from_files);
    if (avels!=NULL) lives_free(avels);
    if (aseeks!=NULL) lives_free(aseeks);

    if (mainw->multitrack!=NULL) nfiles=weed_leaf_num_elements(mainw->multitrack->avol_init_event,"in_tracks");

    else nfiles=1;

    from_files=(int *)lives_malloc(nfiles*sizint);
    avels=(double *)lives_malloc(nfiles*sizdbl);
    aseeks=(double *)lives_malloc(nfiles*sizdbl);

    for (i=0; i<nfiles; i++) {
      from_files[i]=0;
      avels[i]=aseeks[i]=0.;
    }

    // TODO - actually what we should do here is get the audio state for
    // the *last* frame in the buffer and then adjust the seeks back to the
    // beginning of the buffer, in case an audio track starts during the
    // buffering period. The current way is fine for a preview, but when we
    // implement rendering of *partial* event lists we will need to do this

    // a negative seek value would mean that we need to pad silence at the
    // start of the track buffer


    if (event!=get_first_event(event_list))
      atstate=get_audio_and_effects_state_at(event_list,event,TRUE,exact);

    // process audio updates at this frame
    else atstate=aframe_to_atstate(event);

    mainw->audio_event=event;

    if (atstate!=NULL) {

      for (nnfiles=0; atstate[nnfiles].afile!=-1; nnfiles++);

      for (i=0; i<nnfiles; i++) {
        if (atstate[i].afile>0) {
          from_files[i]=atstate[i].afile;
          avels[i]=atstate[i].vel;
          aseeks[i]=atstate[i].seek;
        }
      }

      lives_free(atstate);
    }
  } else {
    is_cont=TRUE;
  }

  if (mainw->multitrack!=NULL) {
    // get channel volumes from the mixer
    for (i=0; i<nfiles; i++) {
      if (mainw->multitrack!=NULL&&mainw->multitrack->audio_vols!=NULL) {
        chvols[i]=(double)LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->multitrack->audio_vols,i))/1000000.;
      }
    }
  } else chvols[0]=1.;

  fill_tc=last_tc+(double)(abuf->samp_space)/(double)abuf->arate*U_SEC;

  // continue until either we have a full buffer, or we reach next audio frame
  while (event!=NULL&&get_event_timecode(event)<=fill_tc) {
    if (!is_cont) event=get_next_frame_event(event);
    if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
      // got next audio frame
      weed_timecode_t tc=get_event_timecode(event);
      if (tc>=fill_tc) break;

      tc+=(U_SEC/cfile->fps*!is_blank_frame(event,FALSE));

      mainw->read_failed=FALSE;
      if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
      mainw->read_failed_file=NULL;

      render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, tc, chvols, 1., 1., abuf);

      if (mainw->read_failed) {
        do_read_failed_error_s(mainw->read_failed_file,NULL);
      }

      for (i=0; i<nfiles; i++) {
        // increase seek values
        aseeks[i]+=avels[i]*(tc-last_tc)/U_SEC;
      }

      last_tc=tc;

      // process audio updates at this frame
      atstate=aframe_to_atstate(event);

      if (atstate!=NULL) {
        for (nnfiles=0; atstate[nnfiles].afile!=-1; nnfiles++);

        for (i=0; i<nnfiles; i++) {
          if (atstate[i].afile>0) {
            from_files[i]=atstate[i].afile;
            avels[i]=atstate[i].vel;
            aseeks[i]=atstate[i].seek;
          }
        }
        lives_free(atstate);
      }
    }
    is_cont=FALSE;
  }

  if (last_tc<fill_tc) {
    // flush the rest of the audio

    mainw->read_failed=FALSE;
    if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
    mainw->read_failed_file=NULL;

    render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, fill_tc, chvols, 1., 1., abuf);
    for (i=0; i<nfiles; i++) {
      // increase seek values
      aseeks[i]+=avels[i]*(fill_tc-last_tc)/U_SEC;
    }
  }

  if (mainw->read_failed) {
    do_read_failed_error_s(mainw->read_failed_file,NULL);
  }

  mainw->write_abuf++;
  if (mainw->write_abuf>=prefs->num_rtaudiobufs) mainw->write_abuf=0;

  last_tc=fill_tc;

  if (mainw->abufs_to_fill>0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    mainw->abufs_to_fill--;
    pthread_mutex_unlock(&mainw->abuf_mutex);
  }

}




void init_jack_audio_buffers(int achans, int arate, boolean exact) {
#ifdef ENABLE_JACK

  int i,chan;

  mainw->jackd->abufs=(lives_audio_buf_t **)lives_malloc(prefs->num_rtaudiobufs*sizeof(lives_audio_buf_t *));

  for (i=0; i<prefs->num_rtaudiobufs; i++) {
    mainw->jackd->abufs[i]=(lives_audio_buf_t *)lives_malloc(sizeof(lives_audio_buf_t));

    mainw->jackd->abufs[i]->out_achans=achans;
    mainw->jackd->abufs[i]->arate=arate;
    mainw->jackd->abufs[i]->samp_space=XSAMPLES/prefs->num_rtaudiobufs;
    mainw->jackd->abufs[i]->bufferf=(float **)lives_malloc(achans*sizeof(float *));
    for (chan=0; chan<achans; chan++) {
      mainw->jackd->abufs[i]->bufferf[chan]=(float *)lives_malloc(XSAMPLES/prefs->num_rtaudiobufs*sizeof(float));
    }
  }
#endif
}


void init_pulse_audio_buffers(int achans, int arate, boolean exact) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  mainw->pulsed->abufs=(lives_audio_buf_t **)lives_malloc(prefs->num_rtaudiobufs*sizeof(lives_audio_buf_t *));

  for (i=0; i<prefs->num_rtaudiobufs; i++) {
    mainw->pulsed->abufs[i]=(lives_audio_buf_t *)lives_malloc(sizeof(lives_audio_buf_t));

    mainw->pulsed->abufs[i]->out_achans=achans;
    mainw->pulsed->abufs[i]->arate=arate;
    mainw->pulsed->abufs[i]->samp_space=XSAMPLES/prefs->num_rtaudiobufs;  // samp_space here is in stereo samples
    mainw->pulsed->abufs[i]->buffer16=(short **)lives_malloc(sizeof(short *));
    mainw->pulsed->abufs[i]->buffer16[0]=(short *)lives_malloc(XSAMPLES/prefs->num_rtaudiobufs*achans*sizeof(short));
  }
#endif
}



void free_jack_audio_buffers(void) {
#ifdef ENABLE_JACK

  int i,chan;

  if (mainw->jackd==NULL) return;

  if (mainw->jackd->abufs==NULL) return;

  for (i=0; i<prefs->num_rtaudiobufs; i++) {
    if (mainw->jackd->abufs[i]!=NULL) {
      for (chan=0; chan<mainw->jackd->abufs[i]->out_achans; chan++) {
        lives_free(mainw->jackd->abufs[i]->bufferf[chan]);
      }
      lives_free(mainw->jackd->abufs[i]->bufferf);
      lives_free(mainw->jackd->abufs[i]);
    }
  }
  lives_free(mainw->jackd->abufs);
#endif
}


void free_pulse_audio_buffers(void) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  if (mainw->pulsed==NULL) return;

  if (mainw->pulsed->abufs==NULL) return;

  for (i=0; i<prefs->num_rtaudiobufs; i++) {
    if (mainw->pulsed->abufs[i]!=NULL) {
      lives_free(mainw->pulsed->abufs[i]->buffer16[0]);
      lives_free(mainw->pulsed->abufs[i]->buffer16);
      lives_free(mainw->pulsed->abufs[i]);
    }
  }
  lives_free(mainw->pulsed->abufs);
#endif
}




boolean resync_audio(int frameno) {
  // if we are using a realtime audio player, resync to frameno
  // and return TRUE

  // otherwise return FALSE


  // this is called for example when the play position jumps, either due
  // to external transport changes, (jack transport, osc retrigger or goto)
  // or if we are looping a video selection

  // this is only active if "audio follows video rate/fps changes" is set

  if (!(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) return FALSE;

  // if recording external audio, we are intrinsically in sync
  if (mainw->record&&prefs->audio_src==AUDIO_SRC_EXT) return TRUE;


#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
    if (!mainw->is_rendering) {

      if (mainw->jackd->playing_file!=-1&&!jack_audio_seek_frame(mainw->jackd,frameno)) {
        if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd,frameno);
      }

      if (mainw->agen_key==0&&!mainw->agen_needs_reinit&&!has_audio_filters(AF_TYPE_NONA)) {
        mainw->rec_aclip=mainw->current_file;
        mainw->rec_avel=cfile->pb_fps/cfile->fps;
        mainw->rec_aseek=(double)mainw->jackd->seek_pos/(double)(cfile->arate*cfile->achans*cfile->asampsize/8);
      }
    }

    return TRUE;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
    if (!mainw->is_rendering) {
      if (mainw->pulsed->playing_file!=-1&&!pulse_audio_seek_frame(mainw->pulsed,frameno)) {
        if (pulse_try_reconnect()) pulse_audio_seek_frame(mainw->pulsed,frameno);
      }
      if (mainw->agen_key==0&&!mainw->agen_needs_reinit&&!has_audio_filters(AF_TYPE_NONA)) {
        mainw->rec_aclip=mainw->current_file;
        mainw->rec_avel=cfile->pb_fps/cfile->fps;
        mainw->rec_aseek=(double)mainw->pulsed->seek_pos/(double)(cfile->arate*cfile->achans*cfile->asampsize/8);
      }
    }
    return TRUE;
  }
#endif
  return FALSE;

}




//////////////////////////////////////////////////////////////////////////
static lives_audio_buf_t *cache_buffer=NULL;
static pthread_t athread;


static void *cache_my_audio(void *arg) {
  lives_audio_buf_t *cbuffer = (lives_audio_buf_t *)arg;
  char *filename;
  register int i;

  cbuffer->is_ready=TRUE;

  while (!cbuffer->die) {

    // wait for request from client
    while (cbuffer->is_ready&&!cbuffer->die) {
      sched_yield();
      lives_usleep(prefs->sleep_time);
    }

    if (cbuffer->die) {
      if (cbuffer->_fd!=-1) close(cbuffer->_fd);
      return cbuffer;
    }

    // read from file and process data
    //lives_printerr("got buffer request !\n");

    if (cbuffer->operation!=LIVES_READ_OPERATION) {
      cbuffer->is_ready=TRUE;
      continue;
    }

    cbuffer->eof=FALSE;

    // TODO - if out_asamps changed, we need to free all buffers and set _cachans==0

    if (cbuffer->out_asamps!=cbuffer->_casamps) {
      if (cbuffer->bufferf!=NULL) {
        // free float channels
        for (i=0; i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i++) {
          lives_free(cbuffer->bufferf[i]);
        }
        lives_free(cbuffer->bufferf);
        cbuffer->bufferf=NULL;
      }

      if (cbuffer->buffer16!=NULL) {
        // free 16bit channels
        for (i=0; i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i++) {
          lives_free(cbuffer->buffer16[i]);
        }
        lives_free(cbuffer->buffer16);
        cbuffer->buffer16=NULL;
      }

      cbuffer->_cachans=0;
      cbuffer->_cout_interleaf=FALSE;
      cbuffer->_csamp_space=0;
    }


    // do we need to allocate output buffers ?
    switch (cbuffer->out_asamps) {
    case 8:
    case 24:
    case 32:
      // not yet implemented
      break;
    case 16:
      // we need 16 bit buffer(s) only
      if (cbuffer->bufferf!=NULL) {
        // free float channels
        for (i=0; i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i++) {
          lives_free(cbuffer->bufferf[i]);
        }
        lives_free(cbuffer->bufferf);
        cbuffer->bufferf=NULL;
      }

      if ((cbuffer->out_interleaf?1:cbuffer->out_achans) != (cbuffer->_cout_interleaf?1:cbuffer->_cachans)
          || (cbuffer->samp_space/(cbuffer->out_interleaf?1:cbuffer->out_achans) !=
              (cbuffer->_csamp_space/(cbuffer->_cout_interleaf?1:cbuffer->_cachans)))) {
        // channels or samp_space changed

        if ((cbuffer->out_interleaf?1:cbuffer->out_achans) > (cbuffer->_cout_interleaf?1:cbuffer->_cachans)) {
          // ouput channels increased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(short *));
          for (i=(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i<(cbuffer->out_interleaf?1:cbuffer->out_achans); i++) {
            cbuffer->buffer16[i]=NULL;
          }
        }

        for (i=0; i<(cbuffer->out_interleaf?1:cbuffer->out_achans); i++) {
          // realloc existing channels and add new ones
          cbuffer->buffer16[i]=(short *)lives_realloc(cbuffer->buffer16[i], cbuffer->samp_space*sizeof(short)*
                               (cbuffer->out_interleaf?cbuffer->out_achans:1));
        }

        // free any excess channels

        for (i=(cbuffer->out_interleaf?1:cbuffer->out_achans); i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i++) {
          lives_free(cbuffer->buffer16[i]);
        }

        if ((cbuffer->out_interleaf?1:cbuffer->out_achans) < (cbuffer->_cout_interleaf?1:cbuffer->_cachans)) {
          // output channels decreased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(short *));
        }
      }

      break;
    case -32:
      // we need 16 bit buffer(s) and float buffer(s)

      // 16 bit buffers follow in out_achans but in_interleaf...

      if ((cbuffer->in_interleaf?1:cbuffer->out_achans) != (cbuffer->_cin_interleaf?1:cbuffer->_cachans)
          || (cbuffer->samp_space/(cbuffer->in_interleaf?1:cbuffer->out_achans) !=
              (cbuffer->_csamp_space/(cbuffer->_cin_interleaf?1:cbuffer->_cachans)))) {
        // channels or samp_space changed

        if ((cbuffer->in_interleaf?1:cbuffer->out_achans) > (cbuffer->_cin_interleaf?1:cbuffer->_cachans)) {
          // ouput channels increased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16,(cbuffer->in_interleaf?1:cbuffer->out_achans)*sizeof(short *));
          for (i=(cbuffer->_cin_interleaf?1:cbuffer->_cachans); i<(cbuffer->in_interleaf?1:cbuffer->out_achans); i++) {
            cbuffer->buffer16[i]=NULL;
          }
        }

        for (i=0; i<(cbuffer->in_interleaf?1:cbuffer->out_achans); i++) {
          // realloc existing channels and add new ones
          cbuffer->buffer16[i]=(short *)lives_realloc(cbuffer->buffer16[i], cbuffer->samp_space*sizeof(short)*
                               (cbuffer->in_interleaf?cbuffer->out_achans:1));
        }

        // free any excess channels

        for (i=(cbuffer->in_interleaf?1:cbuffer->out_achans); i<(cbuffer->_cin_interleaf?1:cbuffer->_cachans); i++) {
          lives_free(cbuffer->buffer16[i]);
        }


        if ((cbuffer->in_interleaf?1:cbuffer->out_achans)<(cbuffer->_cin_interleaf?1:cbuffer->_cachans)) {
          // output channels decreased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16,(cbuffer->in_interleaf?1:cbuffer->out_achans)*sizeof(short *));
        }
      }



      if ((cbuffer->out_interleaf?1:cbuffer->out_achans) != (cbuffer->_cout_interleaf?1:cbuffer->_cachans)
          || (cbuffer->samp_space/(cbuffer->out_interleaf?1:cbuffer->out_achans) !=
              (cbuffer->_csamp_space/(cbuffer->_cout_interleaf?1:cbuffer->_cachans)))) {
        // channels or samp_space changed

        if ((cbuffer->out_interleaf?1:cbuffer->out_achans) > (cbuffer->_cout_interleaf?1:cbuffer->_cachans)) {
          // ouput channels increased
          cbuffer->bufferf = (float **)
                             lives_realloc(cbuffer->bufferf,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(float *));
          for (i=(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i<(cbuffer->out_interleaf?1:cbuffer->out_achans); i++) {
            cbuffer->bufferf[i]=NULL;
          }
        }

        for (i=0; i<(cbuffer->out_interleaf?1:cbuffer->out_achans); i++) {
          // realloc existing channels and add new ones
          cbuffer->bufferf[i]=(float *)lives_realloc(cbuffer->bufferf[i], cbuffer->samp_space*sizeof(float)*
                              (cbuffer->out_interleaf?cbuffer->out_achans:1));
        }

        // free any excess channels

        for (i=(cbuffer->out_interleaf?1:cbuffer->out_achans); i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans); i++) {
          lives_free(cbuffer->bufferf[i]);
        }


        if ((cbuffer->out_interleaf?1:cbuffer->out_achans) > (cbuffer->_cout_interleaf?1:cbuffer->_cachans)) {
          // output channels decreased
          cbuffer->bufferf = (float **)
                             lives_realloc(cbuffer->bufferf,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(float *));
        }
      }

      break;
    default:
      break;
    }

    // update _cinterleaf, etc.
    cbuffer->_cin_interleaf=cbuffer->in_interleaf;
    cbuffer->_cout_interleaf=cbuffer->out_interleaf;
    cbuffer->_csamp_space=cbuffer->samp_space;
    cbuffer->_cachans=cbuffer->out_achans;
    cbuffer->_casamps=cbuffer->out_asamps;

    // open new file if necessary

    if (cbuffer->fileno!=cbuffer->_cfileno) {
      lives_clip_t *afile=mainw->files[cbuffer->fileno];

      if (cbuffer->_fd!=-1) close(cbuffer->_fd);

      if (afile->opening)
        filename=lives_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,mainw->files[cbuffer->fileno]->handle);
      else filename=lives_strdup_printf("%s/%s/audio",prefs->tmpdir,mainw->files[cbuffer->fileno]->handle);

      cbuffer->_fd=open(filename,O_RDONLY);
      if (cbuffer->_fd==-1) {
        lives_printerr("audio cache thread: error opening %s\n",filename);
        cbuffer->in_achans=0;
        cbuffer->fileno=-1;  ///< let client handle this
        cbuffer->is_ready=TRUE;
        continue;
      }

#ifdef IS_MINGW
      setmode(cbuffer->_fd, O_BINARY);
#endif

      lives_free(filename);
    }

    if (cbuffer->fileno!=cbuffer->_cfileno||cbuffer->seek!=cbuffer->_cseek) {
      lseek64(cbuffer->_fd, cbuffer->seek, SEEK_SET);
    }

    cbuffer->_cfileno=cbuffer->fileno;

    // prepare file read buffer

    if (cbuffer->bytesize!=cbuffer->_cbytesize) {
      cbuffer->_filebuffer=(uint8_t *)lives_realloc(cbuffer->_filebuffer,cbuffer->bytesize);

      if (cbuffer->_filebuffer==NULL) {
        cbuffer->_cbytesize=cbuffer->bytesize=0;
        cbuffer->in_achans=0;
        cbuffer->is_ready=TRUE;
        continue;
      }

    }

    // read from file
    cbuffer->_cbytesize=read(cbuffer->_fd, cbuffer->_filebuffer, cbuffer->bytesize);

    if (cbuffer->_cbytesize<0) {
      // there is not much we can do if we get a read error, since we are running in a realtime thread here
      // just mark it as 0 channels, 0 bytes
      cbuffer->bytesize=cbuffer->_cbytesize=0;
      cbuffer->in_achans=0;
      cbuffer->is_ready=TRUE;
      continue;
    }

    if (cbuffer->_cbytesize<cbuffer->bytesize) {
      cbuffer->eof=TRUE;
      cbuffer->_csamp_space=(int64_t)((double)cbuffer->samp_space/(double)cbuffer->bytesize*(double)cbuffer->_cbytesize);
      cbuffer->samp_space=cbuffer->_csamp_space;
    }

    cbuffer->bytesize=cbuffer->_cbytesize;
    cbuffer->_cseek=(cbuffer->seek+=cbuffer->bytesize);

    // do conversion


    // convert from 8 bit to 16 bit and mono to stereo if necessary
    // resample as we go
    if (cbuffer->in_asamps==8) {

      // TODO - error on non-interleaved
      sample_move_d8_d16(cbuffer->buffer16[0],(uint8_t *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize,
                         cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans, 0);
    }
    // 16 bit input samples
    // resample as we go
    else {
      sample_move_d16_d16(cbuffer->buffer16[0], (short *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize,
                          cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans,
                          cbuffer->swap_endian?SWAP_X_TO_L:0, 0);
    }


    // if our out_asamps is 16, we are done


    cbuffer->is_ready=TRUE;
  }
  return cbuffer;
}



lives_audio_buf_t *audio_cache_init(void) {

  cache_buffer=(lives_audio_buf_t *)lives_malloc0(sizeof(lives_audio_buf_t));
  cache_buffer->is_ready=FALSE;
  cache_buffer->in_achans=0;

  // NULL all pointers of cache_buffer

  cache_buffer->buffer8=NULL;
  cache_buffer->buffer16=NULL;
  cache_buffer->buffer24=NULL;
  cache_buffer->buffer32=NULL;
  cache_buffer->bufferf=NULL;
  cache_buffer->_filebuffer=NULL;
  cache_buffer->_cbytesize=0;
  cache_buffer->_csamp_space=0;
  cache_buffer->_cachans=0;
  cache_buffer->_casamps=0;
  cache_buffer->_cout_interleaf=FALSE;
  cache_buffer->_cin_interleaf=FALSE;
  cache_buffer->eof=FALSE;
  cache_buffer->die=FALSE;

  cache_buffer->_cfileno=-1;
  cache_buffer->_cseek=-1;
  cache_buffer->_fd=-1;


  // init the audio caching thread for rt playback
  pthread_create(&athread,NULL,cache_my_audio,cache_buffer);

  return cache_buffer;
}



void audio_cache_end(void) {

  int i;
  lives_audio_buf_t *xcache_buffer;

  cache_buffer->die=TRUE;  ///< tell cache thread to exit when possible
  pthread_join(athread,NULL);

  // free all buffers

  for (i=0; i<(cache_buffer->_cin_interleaf?1:cache_buffer->_cachans); i++) {
    if (cache_buffer->buffer8!=NULL&&cache_buffer->buffer8[i]!=NULL) lives_free(cache_buffer->buffer8[i]);
    if (cache_buffer->buffer16!=NULL&&cache_buffer->buffer16[i]!=NULL) lives_free(cache_buffer->buffer16[i]);
    if (cache_buffer->buffer24!=NULL&&cache_buffer->buffer24[i]!=NULL) lives_free(cache_buffer->buffer24[i]);
    if (cache_buffer->buffer32!=NULL&&cache_buffer->buffer32[i]!=NULL) lives_free(cache_buffer->buffer32[i]);
    if (cache_buffer->bufferf!=NULL&&cache_buffer->bufferf[i]!=NULL) lives_free(cache_buffer->bufferf[i]);
  }

  if (cache_buffer->buffer8!=NULL) lives_free(cache_buffer->buffer8);
  if (cache_buffer->buffer16!=NULL) lives_free(cache_buffer->buffer16);
  if (cache_buffer->buffer24!=NULL) lives_free(cache_buffer->buffer24);
  if (cache_buffer->buffer32!=NULL) lives_free(cache_buffer->buffer32);
  if (cache_buffer->bufferf!=NULL) lives_free(cache_buffer->bufferf);

  if (cache_buffer->_filebuffer!=NULL) lives_free(cache_buffer->_filebuffer);

  // make this threadsafe
  xcache_buffer=cache_buffer;
  cache_buffer=NULL;
  lives_free(xcache_buffer);
}


lives_audio_buf_t *audio_cache_get_buffer(void) {
  return cache_buffer;
}


///////////////////////////////////////

// plugin handling

boolean get_audio_from_plugin(float *fbuffer, int nchans, int arate, int nsamps) {
  // get audio from an audio generator; fbuffer is filled with non-interleaved float

  weed_timecode_t tc;

  int error;

  int xnchans;
  int aint;

  weed_plant_t *inst=rte_keymode_get_instance(mainw->agen_key,rte_key_getmode(mainw->agen_key));
  weed_plant_t *filter;
  weed_plant_t *channel;
  weed_plant_t *ctmpl;

  weed_process_f *process_func_ptr_ptr;
  weed_process_f process_func;

  if (mainw->agen_needs_reinit) return FALSE; // wait for other thread to reinit us
  tc=(double)mainw->agen_samps_count/(double)arate*U_SEC; // we take our timing from the number of samples read

getaud1:

  aint=WEED_FALSE;
  xnchans=nchans;
  filter=weed_instance_get_filter(inst,FALSE);
  channel=get_enabled_channel(inst,0,FALSE);

  if (channel!=NULL) {
    ctmpl=weed_get_plantptr_value(channel,"template",&error);

    if (weed_plant_has_leaf(ctmpl,"audio_rate")&&weed_get_int_value(ctmpl,"audio_rate",&error)!=arate) {
      // TODO - resample if audio rate is wrong
      return FALSE;
    }

    if (weed_plant_has_leaf(ctmpl,"audio_interleaf")) aint=weed_get_boolean_value(ctmpl,"audio_interleaf",&error);
    if (weed_plant_has_leaf(ctmpl,"audio_channels")) xnchans=weed_get_int_value(ctmpl,"audio_channels",&error);

    // stop video thread from possibly interpolating/deiniting
    if (pthread_mutex_trylock(&mainw->interp_mutex)) return FALSE;


    // make sure values match, else we need to reinit the plugin
    if (xnchans!=weed_get_int_value(channel,"audio_channels",&error)||
        arate!=weed_get_int_value(channel,"audio_rate",&error)||
        weed_get_boolean_value(channel,"audio_interleaf",&error)!=aint) {
      // reinit plugin
      mainw->agen_needs_reinit=TRUE;
    }

    weed_set_int_value(channel,"audio_channels",xnchans);
    weed_set_int_value(channel,"audio_rate",arate);
    weed_set_boolean_value(channel,"audio_interleaf",aint);
    weed_set_int_value(channel,"audio_data_length",nsamps);
    weed_set_voidptr_value(channel,"audio_data",fbuffer);

    weed_set_double_value(inst,"fps",cfile->pb_fps);

    if (mainw->agen_needs_reinit) {
      // allow main thread to complete the reinit so we do not delay; just return silence
      memset(fbuffer,0,nsamps*nchans*sizeof(float));
      pthread_mutex_unlock(&mainw->interp_mutex);
      return FALSE;
    }

    weed_set_int64_value(channel,"timecode",tc);
  }


  if (mainw->pconx!=NULL&&!(mainw->preview||mainw->is_rendering)) {
    // chain any data pipelines
    if (!pthread_mutex_trylock(&mainw->data_mutex[mainw->agen_key-1])) {
      mainw->agen_needs_reinit=pconx_chain_data(mainw->agen_key-1,rte_key_getmode(mainw->agen_key));
      filter_mutex_unlock(mainw->agen_key-1);

      if (mainw->agen_needs_reinit) {
        // allow main thread to complete the reinit so we do not delay; just return silence
        memset(fbuffer,0,nsamps*nchans*sizeof(float));
        pthread_mutex_unlock(&mainw->interp_mutex);
        return FALSE;
      }
    }
  }



  weed_leaf_get(filter,"process_func",0,(void *)&process_func_ptr_ptr);
  process_func=process_func_ptr_ptr[0];

  if ((*process_func)(inst,tc)==WEED_ERROR_PLUGIN_INVALID) {
    pthread_mutex_unlock(&mainw->interp_mutex);
    return FALSE;
  }
  pthread_mutex_unlock(&mainw->interp_mutex);

  if (channel!=NULL&&aint==WEED_TRUE) {
    if (!float_deinterleave(fbuffer,nsamps,nchans)) return FALSE;
  }

  if (xnchans==1&&nchans==2) {
    // if we got mono but we wanted stereo, copy to right channel
    lives_memcpy(&fbuffer[nsamps],fbuffer,nsamps*sizeof(float));
  }

  if (weed_plant_has_leaf(inst,"host_next_instance")) {
    // handle compound fx
    inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
    goto getaud1;
  }

  mainw->agen_samps_count+=nsamps;

  return TRUE;
}


void reinit_audio_gen(void) {
  int agen_key=mainw->agen_key;
  int ret;

  weed_plant_t *inst=rte_keymode_get_instance(agen_key,rte_key_getmode(mainw->agen_key));

  ret=weed_reinit_effect(inst,TRUE);
  if (ret==FILTER_NO_ERROR||ret==FILTER_INFO_REINITED) {
    mainw->agen_needs_reinit=FALSE;
    mainw->agen_key=agen_key;
  }
}


////////////////////////////////////////
// apply audio as "rendered effect"

static int audio_fd;
static char *audio_file;

off64_t audio_pos;

weed_timecode_t aud_tc;

boolean apply_rte_audio_init(void) {
  char *com;

  if (!prefs->conserve_space) {
    mainw->error=FALSE;
    com=lives_strdup_printf("%s backup_audio %s",prefs->backend_sync,cfile->handle);
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->error) {
      d_print_failed();
      return FALSE;
    }
  }

  audio_pos=(double)((cfile->start-1)*cfile->arate*cfile->achans*cfile->asampsize/8)/cfile->fps;
  audio_file=lives_build_filename(prefs->tmpdir,cfile->handle,"audio",NULL);

  audio_fd=open(audio_file,O_RDWR|O_CREAT,DEF_FILE_PERMS);

  if (audio_fd==-1) return FALSE;

#ifdef IS_MINGW
  setmode(audio_fd,O_BINARY);
#endif

  if (audio_pos>cfile->afilesize) {
    off64_t audio_end_pos=(double)((cfile->start-1)*cfile->arate*cfile->achans*cfile->asampsize/8)/cfile->fps;
    pad_with_silence(audio_fd, audio_pos, audio_end_pos, cfile->asampsize, cfile->signed_endian&AFORM_UNSIGNED,
                     cfile->signed_endian&AFORM_BIG_ENDIAN);
  } else lseek64(audio_fd,audio_pos,SEEK_SET);

  aud_tc=0;

  return TRUE;
}


void apply_rte_audio_end(boolean del) {
  close(audio_fd);
  if (del) lives_rm(audio_file);
  lives_free(audio_file);
}


boolean apply_rte_audio(int nframes) {
  size_t tbytes;
  uint8_t *in_buff;
  float **fltbuf,*fltbufni=NULL;
  short *shortbuf=NULL;

  boolean rev_endian=FALSE;

  register int i;

  int abigendian=cfile->signed_endian&AFORM_BIG_ENDIAN;
  int onframes;

  // read nframes of audio from clip or generator

  if ((abigendian&&capable->byte_order==LIVES_LITTLE_ENDIAN)||(!abigendian&&capable->byte_order==LIVES_BIG_ENDIAN)) rev_endian=TRUE;

  tbytes=nframes*cfile->achans*cfile->asampsize/8;

  if (mainw->agen_key==0) {
    if (tbytes+audio_pos>cfile->afilesize) tbytes=cfile->afilesize-audio_pos;
    if (tbytes<=0) return TRUE;
    nframes=tbytes/cfile->achans/(cfile->asampsize/8);
  }

  onframes=nframes;

  in_buff=(uint8_t *)lives_try_malloc(tbytes);
  if (in_buff==NULL) return FALSE;

  if (cfile->asampsize==8) {
    shortbuf=(short *)lives_try_malloc(tbytes);
    if (shortbuf==NULL) {
      lives_free(in_buff);
      return FALSE;
    }
  }

  fltbuf=(float **)lives_malloc(cfile->achans*sizeof(float *));


  if (mainw->agen_key==0) {
    mainw->read_failed=FALSE;

    tbytes=lives_read(audio_fd,in_buff,tbytes,FALSE);

    if (mainw->read_failed) {
      do_read_failed_error_s(audio_file,NULL);
      if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
      mainw->read_failed_file=NULL;
      lives_free(fltbuf);
      lives_free(in_buff);
      if (shortbuf!=NULL) lives_free(shortbuf);
      return FALSE;
    }


    if (cfile->asampsize==8) {
      sample_move_d8_d16(shortbuf, in_buff, nframes, tbytes,
                         1.0, cfile->achans, cfile->achans, 0);
    } else shortbuf=(short *)in_buff;

    nframes=tbytes/cfile->achans/(cfile->asampsize/8);

    // convert to float

    for (i=0; i<cfile->achans; i++) {
      // convert s16 to non-interleaved float
      fltbuf[i]=(float *)lives_try_malloc(nframes*sizeof(float));
      if (fltbuf[i]==NULL) {
        for (--i; i>=0; i--) {
          lives_free(fltbuf[i]);
        }
        lives_free(fltbuf);
        if (shortbuf!=(short *)in_buff) lives_free(shortbuf);
        lives_free(in_buff);
        return FALSE;
      }
      memset(fltbuf[i],0,nframes*sizeof(float));
      if (nframes>0) sample_move_d16_float(fltbuf[i],shortbuf+i,nframes,cfile->achans,(cfile->signed_endian&AFORM_UNSIGNED),rev_endian,1.0);
    }

  } else {
    fltbufni=(float *)lives_try_malloc(nframes*cfile->achans*sizeof(float));
    if (fltbufni==NULL) {
      lives_free(fltbuf);
      if (shortbuf!=(short *)in_buff) lives_free(shortbuf);
      lives_free(in_buff);
      return FALSE;
    }
    get_audio_from_plugin(fltbufni, cfile->achans, cfile->arate, nframes);
    for (i=0; i<cfile->achans; i++) {
      fltbuf[i]=fltbufni+i*nframes;
    }
  }


  // apply any audio effects

  aud_tc+=(double)onframes/(double)cfile->arate*U_SEC;
  // apply any audio effects with in_channels

  weed_apply_audio_effects_rt(fltbuf,cfile->achans,onframes,cfile->arate,aud_tc,FALSE);

  if (!(has_audio_filters(AF_TYPE_NONA)||mainw->agen_key!=0)) {
    // analysers only - no need to save
    audio_pos+=tbytes;

    if (fltbufni==NULL) {
      for (i=0; i<cfile->achans; i++) {
        lives_free(fltbuf[i]);
      }
    } else lives_free(fltbufni);

    lives_free(fltbuf);

    if (shortbuf!=(short *)in_buff) lives_free(shortbuf);
    lives_free(in_buff);

    return TRUE;
  }

  // convert float audio back to int
  sample_move_float_int(in_buff,fltbuf,onframes,1.0,cfile->achans,cfile->asampsize,(cfile->signed_endian&AFORM_UNSIGNED),
                        !(cfile->signed_endian&AFORM_BIG_ENDIAN),FALSE,1.0);

  if (fltbufni==NULL) {
    for (i=0; i<cfile->achans; i++) {
      lives_free(fltbuf[i]);
    }
  } else lives_free(fltbufni);

  lives_free(fltbuf);


  // save to file
  mainw->write_failed=FALSE;
  lseek64(audio_fd,audio_pos,SEEK_SET);
  tbytes=onframes*cfile->achans*cfile->asampsize/8;
  lives_write(audio_fd,in_buff,tbytes,FALSE);
  audio_pos+=tbytes;

  if (shortbuf!=(short *)in_buff) lives_free(shortbuf);
  lives_free(in_buff);

  if (mainw->write_failed) {
    do_write_failed_error_s(audio_file,NULL);
    return FALSE;
  }

  return TRUE;
}



boolean push_audio_to_channel(weed_plant_t *achan, lives_audio_buf_t *abuf) {
  // push audio from abuf into an audio channel
  // audio will be formatted to the channel requested format

  // NB: if player is jack, we will have non-interleaved float
  // if player is pulse, we will have interleaved S16
  void *dst,*src;

  weed_plant_t *ctmpl;

  float scale;

  size_t samps,offs;

  boolean tinter;
  int trate,tlen,tchans;
  int alen;
  int error;

  register int i;

  if (abuf->samples_filled==0) {
    weed_set_int_value(achan,"audio_data_length",0);
    weed_set_voidptr_value(achan,"audio_data",NULL);
    return FALSE;
  }

  ctmpl=weed_get_plantptr_value(achan,"template",&error);

  if (weed_plant_has_leaf(achan,"audio_rate")) trate=weed_get_int_value(achan,"audio_rate",&error);
  else if (weed_plant_has_leaf(ctmpl,"audio_rate")) trate=weed_get_int_value(ctmpl,"audio_rate",&error);
  else trate=DEFAULT_AUDIO_RATE;

  if (weed_plant_has_leaf(achan,"audio_channels")) tchans=weed_get_int_value(achan,"audio_channels",&error);
  else if (weed_plant_has_leaf(ctmpl,"audio_channels")) tchans=weed_get_int_value(ctmpl,"audio_channels",&error);
  else tchans=DEFAULT_AUDIO_CHANS;

  if (weed_plant_has_leaf(achan,"audio_interleaf")) tinter=weed_get_boolean_value(achan,"audio_interleaf",&error);
  else if (weed_plant_has_leaf(ctmpl,"audio_interleaf")) tinter=weed_get_boolean_value(ctmpl,"audio_interleaf",&error);
  else tinter=FALSE;

  if (weed_plant_has_leaf(ctmpl,"audio_data_length")) tlen=weed_get_int_value(ctmpl,"audio_data_length",&error);
  else tlen=0;

#ifdef DEBUG_AFB
  g_print("push from afb %d\n",abuf->samples_filled);
#endif

  // plugin will get float, so we first convert to that
  if (abuf->bufferf==NULL) {
    // try 8 bit -> 16 -> float
    if (abuf->buffer8!=NULL&&abuf->buffer16==NULL) {
      int swap=0;
      if (!abuf->s8_signed) swap=SWAP_U_TO_S;
      abuf->s16_signed=TRUE;
      abuf->buffer16=(int16_t **)lives_malloc(abuf->out_achans*sizeof(int16_t *));
      for (i=0; i<abuf->out_achans; i++) {
        abuf->bufferf[i]=(float *)lives_malloc(abuf->samples_filled*2);
        sample_move_d8_d16(abuf->buffer16[i],abuf->buffer8[i], abuf->samples_filled, abuf->samples_filled*2,
                           1.0, abuf->out_achans, abuf->out_achans, swap);

      }
    }


    // try convert S16 -> float
    if (abuf->buffer16!=NULL) {
      abuf->bufferf=(float **)lives_malloc(abuf->out_achans*sizeof(float *));
      for (i=0; i<abuf->out_achans; i++) {
        abuf->bufferf[i]=(float *)lives_malloc(abuf->samples_filled*sizeof(float));
        if (!abuf->in_interleaf) {
          sample_move_d16_float(abuf->bufferf[i],abuf->buffer16[i],abuf->samples_filled,1,
                                (abuf->s16_signed?AFORM_SIGNED:AFORM_UNSIGNED),abuf->swap_endian,1.0);
        } else {
          sample_move_d16_float(abuf->bufferf[i],&abuf->buffer16[0][i],abuf->samples_filled,abuf->out_achans,
                                (abuf->s16_signed?AFORM_SIGNED:AFORM_UNSIGNED),abuf->swap_endian,1.0);
        }
      }
      abuf->out_interleaf=FALSE;
    }
  }

  if (abuf->bufferf==NULL) return FALSE;
  // now we should have float

  samps=abuf->samples_filled;

  // push to achan "audio_data", taking into account "audio_data_length", "audio_interleaf", "audio_channels"

  alen=samps;
  if (alen>tlen&&tlen>0) alen=tlen;

  offs=samps-alen;

  scale=(float)trate/(float)abuf->arate;

  // malloc audio_data
  dst=lives_malloc(alen*tchans*sizeof(float));

  // set channel values
  weed_set_voidptr_value(achan,"audio_data",dst);
  weed_set_boolean_value(achan,"audio_interleaf",tinter);
  weed_set_int_value(achan,"audio_data_length",alen);
  weed_set_int_value(achan,"audio_channels",tchans);
  weed_set_int_value(achan,"audio_rate",trate);

  // copy data from abuf->bufferf[] to "audio_data"
  for (i=0; i<tchans; i++) {
    src=abuf->bufferf[i%abuf->out_achans]+offs;
    if (!tinter) {
      if ((int)abuf->arate==trate) {
        lives_memcpy(dst,src,alen*sizeof(float));
      } else {
        // needs resample
        sample_move_float_float((float *)dst,(float *)src,alen,scale,1);
      }
      dst+=alen*sizeof(float);
    } else {
      sample_move_float_float((float *)dst,(float *)src,alen,scale,tchans);
      dst+=sizeof(float);
    }
  }
  return TRUE;
}




////////////////////////////////////////
// audio streaming

lives_pgid_t astream_pgid=0;

boolean start_audio_stream(void) {
  const char *playername="audiostreamer.pl";
  char *astream_name=NULL;
  char *astream_name_out=NULL;

  // playback plugin wants an audio stream - so fork and run the stream
  // player
  char *astname=lives_strdup_printf("livesaudio-%d.pcm",capable->mainpid);
  char *astname_out=lives_strdup_printf("livesaudio-%d.stream",capable->mainpid);
  char *astreamer,*com;

  int arate=0;
  int afd;
  int alarm_handle;

  boolean timeout=FALSE;

  astream_name=lives_build_filename(prefs->tmpdir,astname,NULL);

  mkfifo(astream_name,S_IRUSR|S_IWUSR);

  astream_name_out=lives_build_filename(prefs->tmpdir,astname_out,NULL);

  lives_free(astname);
  lives_free(astname_out);

  if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    arate=(int)mainw->pulsed->out_arate;
    // TODO - chans, samps, signed, endian
#endif
  }

  if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    arate=(int)mainw->jackd->sample_out_rate;
    // TODO - chans, samps, signed, endian

#endif
  }

  astreamer=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_AUDIO_STREAM,playername,NULL);
  com=lives_strdup_printf("%s play %d \"%s\" \"%s\" %d",astreamer,mainw->vpp->audio_codec,astream_name,astream_name_out,arate);
  lives_free(astreamer);

  astream_pgid=lives_fork(com);

  alarm_handle=lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

  do {
    // wait for other thread to create stream (or timeout)
    afd=open(astream_name,O_WRONLY|O_SYNC);
    if (afd!=-1) break;
    lives_usleep(prefs->sleep_time);
  } while (!(timeout=lives_alarm_get(alarm_handle)));

  lives_alarm_clear(alarm_handle);

#ifdef IS_MINGW
  setmode(afd, O_BINARY);
#endif

  if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    mainw->pulsed->astream_fd=afd;
#endif
  }

  if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    mainw->jackd->astream_fd=afd;
#endif
  }

#ifdef IS_MINGW
  setmode(afd, O_BINARY);
#endif

  lives_free(astream_name);
  lives_free(astream_name_out);

  return TRUE;
}



void stop_audio_stream(void) {
  if (astream_pgid>0) {
    // if we were streaming audio, kill it
    const char *playername="audiostreamer.pl";
    char *astname=lives_strdup_printf("livesaudio-%d.pcm",capable->mainpid);
    char *astname_out=lives_strdup_printf("livesaudio-%d.stream",capable->mainpid);
    char *astreamer=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_AUDIO_STREAM,playername,NULL);

    char *astream_name=lives_build_filename(prefs->tmpdir,astname,NULL);
    char *astream_name_out=lives_build_filename(prefs->tmpdir,astname_out,NULL);

    char *com;

    lives_free(astname);
    lives_free(astname_out);

    if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed->astream_fd>-1) close(mainw->pulsed->astream_fd);
      mainw->pulsed->astream_fd=-1;
#endif
    }
    if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (mainw->jackd->astream_fd>-1) close(mainw->jackd->astream_fd);
      mainw->jackd->astream_fd=-1;
#endif
    }

    lives_killpg(astream_pgid,LIVES_SIGKILL);
    lives_rm(astream_name);
    lives_free(astream_name);

    // astreamer should remove cooked stream
    com=lives_strdup_printf("\"%s\" cleanup %d \"%s\"",astreamer,mainw->vpp->audio_codec,astream_name_out);
    lives_system(com,FALSE);
    lives_free(astreamer);
    lives_free(com);
    lives_free(astream_name_out);

  }

}


void clear_audio_stream(void) {
  // remove raw and cooked streams
  char *astname=lives_strdup_printf("livesaudio-%d.pcm",capable->mainpid);
  char *astream_name=lives_build_filename(prefs->tmpdir,astname,NULL);
  char *astname_out=lives_strdup_printf("livesaudio-%d.stream",capable->mainpid);
  char *astream_name_out=lives_build_filename(prefs->tmpdir,astname_out,NULL);
  lives_rm(astream_name);
  lives_rm(astream_name_out);
  lives_free(astname);
  lives_free(astream_name);
  lives_free(astname_out);
  lives_free(astream_name_out);
}



LIVES_INLINE void audio_stream(void *buff, size_t nbytes, int fd) {
  if (fd!=-1) {
    lives_write(fd,buff,nbytes,TRUE);
  }
}




