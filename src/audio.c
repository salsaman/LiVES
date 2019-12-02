// audio.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2019
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "audio.h"
#include "events.h"
#include "callbacks.h"
#include "effects.h"
#include "support.h"

static char *storedfnames[NSTOREDFDS];
static int storedfds[NSTOREDFDS];
static boolean storedfdsset = FALSE;

LIVES_GLOBAL_INLINE boolean is_realtime_aplayer(int ptype) {
  return (ptype == AUD_PLAYER_JACK || ptype == AUD_PLAYER_PULSE || ptype == AUD_PLAYER_NONE);
}


static void audio_reset_stored_fnames(void) {
  int i;
  for (i = 0; i < NSTOREDFDS; i++) {
    storedfnames[i] = NULL;
    storedfds[i] = -1;
  }
  storedfdsset = TRUE;
}


LIVES_GLOBAL_INLINE char *get_achannel_name(int totchans, int idx) {
  if (totchans == 1) return lives_strdup(_("Mono"));
  if (totchans == 2) {
    if (idx == 0) return lives_strdup(_("Left channel"));
    if (idx == 1) return lives_strdup(_("Right channel"));
  }
  return lives_strdup_printf(_("Audio channel %d"), idx);
}


LIVES_GLOBAL_INLINE char *lives_get_audio_file_name(int fnum) {
  char *fname = lives_build_filename(prefs->workdir, mainw->files[fnum]->handle, CLIP_AUDIO_FILENAME, NULL);
  if (mainw->files[fnum]->opening && !lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
    char *tmp = lives_strdup_printf("%s.%s", fname, LIVES_FILE_EXT_PCM);
    lives_free(fname);
    return tmp;
  }
  return fname;
}


LIVES_GLOBAL_INLINE const char *audio_player_get_display_name(const char *aplayer) {
  if (!strcmp(aplayer, AUDIO_PLAYER_PULSE)) return AUDIO_PLAYER_PULSE_AUDIO;
  return aplayer;
}


void audio_free_fnames(void) {
  // cleanup stored filehandles after playback/fade/render

  int i;

  if (!storedfdsset) return;

  for (i = 0; i < NSTOREDFDS; i++) {
    lives_freep((void **)&storedfnames[i]);
    if (storedfds[i] > -1) close(storedfds[i]);
    storedfds[i] = -1;
  }
}


void append_to_audio_bufferf(float *src, uint64_t nsamples, int channum) {
  // append float audio to the audio frame buffer
  size_t nsampsize;
  lives_audio_buf_t *abuf;

  if (!prefs->push_audio_to_gens) return;

  pthread_mutex_lock(&mainw->abuf_mutex);

  abuf = (lives_audio_buf_t *)mainw->audio_frame_buffer; // this is a pointer to either mainw->afb[0] or mainw->afb[1]

  if (abuf == NULL) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return;
  }

  if (abuf->bufferf == NULL) free_audio_frame_buffer(abuf);

  nsampsize = (abuf->samples_filled + nsamples) * sizeof(float);

  channum++;
  if (channum > abuf->out_achans) {
    register int i;
    abuf->bufferf = (float **)lives_realloc(abuf->bufferf, channum * sizeof(float *));
    for (i = abuf->out_achans; i < channum; i++) {
      abuf->bufferf[i] = NULL;
    }
    abuf->out_achans = channum;
  }
  channum--;
  abuf->bufferf[channum] = (float *)lives_realloc(abuf->bufferf[channum], nsampsize * sizeof(float) + EXTRA_BYTES);
  lives_memcpy(&abuf->bufferf[channum][abuf->samples_filled], src, nsamples * sizeof(float));
  pthread_mutex_unlock(&mainw->abuf_mutex);
}


void append_to_audio_buffer16(void *src, uint64_t nsamples, int channum) {
  // append 16 bit audio to the audio frame buffer
  size_t nsampsize;
  lives_audio_buf_t *abuf;

  if (!prefs->push_audio_to_gens) return;

  pthread_mutex_lock(&mainw->abuf_mutex);

  abuf = (lives_audio_buf_t *)mainw->audio_frame_buffer; // this is a pointer to either mainw->afb[0] or mainw->afb[1]

  if (abuf == NULL) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return;
  }
  if (abuf->buffer16 == NULL) free_audio_frame_buffer(abuf);

  nsampsize = (abuf->samples_filled + nsamples);
  channum++;
  if (abuf->buffer16 == NULL || channum > abuf->out_achans) {
    register int i;
    abuf->buffer16 = (int16_t **)lives_calloc(channum, sizeof(short *));
    for (i = abuf->out_achans; i < channum; i++) {
      abuf->buffer16[i] = NULL;
    }
    abuf->out_achans = channum;
  }
  channum--;
  abuf->buffer16[channum] = (short *)lives_realloc(abuf->buffer16[channum], nsampsize * sizeof(short) + EXTRA_BYTES);
  lives_memcpy(&abuf->buffer16[channum][abuf->samples_filled], src, nsamples * 2);
#ifdef DEBUG_AFB
  g_print("append16 to afb\n");
#endif
  pthread_mutex_unlock(&mainw->abuf_mutex);
}


void init_audio_frame_buffers(short aplayer) {
  // function should be called when the first video generator with audio input is enabled
  register int i;
  pthread_mutex_lock(&mainw->abuf_mutex);

  for (i = 0; i < 2; i++) {
    lives_audio_buf_t *abuf;
    mainw->afb[i] = abuf = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));

    abuf->samples_filled = 0;
    abuf->swap_endian = FALSE;
    abuf->out_achans = 0;
    abuf->start_sample = 0;

    switch (aplayer) {
#ifdef HAVE_PULSE_AUDIO
    case AUD_PLAYER_PULSE:
      abuf->in_interleaf = abuf->out_interleaf = TRUE;
      abuf->s16_signed = TRUE;
      if (mainw->pulsed_read != NULL) {
        abuf->in_achans = abuf->out_achans = mainw->pulsed_read->in_achans;
        abuf->arate = mainw->pulsed_read->in_arate;
      } else if (mainw->pulsed != NULL) {
        abuf->in_achans = abuf->out_achans = mainw->pulsed->out_achans;
        abuf->arate = mainw->pulsed->out_arate;
      }
      break;
#endif
#ifdef ENABLE_JACK
    case AUD_PLAYER_JACK:
      abuf->in_interleaf = abuf->out_interleaf = FALSE;
      if (mainw->jackd_read != NULL && mainw->jackd_read->in_use) {
        abuf->in_achans = abuf->out_achans = mainw->jackd_read->num_input_channels;
        abuf->arate = mainw->jackd_read->sample_in_rate;
        abuf->in_achans = abuf->out_achans = mainw->jackd_read->num_input_channels;
      } else if (mainw->jackd != NULL) {
        abuf->in_achans = abuf->out_achans = mainw->jackd->num_output_channels;
        abuf->arate = mainw->jackd->sample_out_rate;
        abuf->in_achans = abuf->out_achans = mainw->jackd->num_output_channels;
      }
      break;
#endif
    default:
      break;
    }
  }

  mainw->audio_frame_buffer = mainw->afb[0];
  pthread_mutex_unlock(&mainw->abuf_mutex);

#ifdef DEBUG_AFB
  g_print("init afb\n");
#endif
}


void free_audio_frame_buffer(lives_audio_buf_t *abuf) {
  // function should be called to clear samples
  // cannot use lives_freep
  register int i;
  if (abuf != NULL) {
    if (abuf->bufferf != NULL) {
      for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->bufferf[i]);
      lives_free(abuf->bufferf);
      abuf->bufferf = NULL;
    }
    if (abuf->buffer32 != NULL) {
      for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer32[i]);
      lives_free(abuf->buffer32);
      abuf->buffer32 = NULL;
    }
    if (abuf->buffer24 != NULL) {
      for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer24[i]);
      lives_free(abuf->buffer24);
      abuf->buffer24 = NULL;
    }
    if (abuf->buffer16 != NULL) {
      for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer16[i]);
      lives_free(abuf->buffer16);
      abuf->buffer16 = NULL;
    }
    if (abuf->buffer8 != NULL) {
      for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer8[i]);
      lives_free(abuf->buffer8);
      abuf->buffer8 = NULL;
    }

    abuf->samples_filled = 0;
    abuf->out_achans = 0;
    abuf->start_sample = 0;
  }
#ifdef DEBUG_AFB
  g_print("clear afb %p\n", abuf);
#endif
}


float get_float_audio_val_at_time(int fnum, int afd, double secs, int chnum, int chans) {
  // return audio level between -1.0 and +1.0

  // afd must be opened with lives_open_buffered_rdonly()

  lives_clip_t *afile = mainw->files[fnum];
  int64_t bytes;
  off_t apos;

  uint8_t val8;
  uint8_t val8b;

  uint16_t val16;

  float val;

  bytes = secs * afile->arate * afile->achans * afile->asampsize / 8;
  if (bytes == 0) return 0.;

  apos = ((int64_t)(bytes / afile->achans / (afile->asampsize / 8))) * afile->achans * (afile->asampsize / 8); // quantise

  if (afd == -1) {
    // deal with read errors after drawing a whole block
    mainw->read_failed = TRUE;
    return 0.;
  }

  apos += afile->asampsize / 8 * chnum;

  lives_lseek_buffered_rdonly_absolute(afd, apos);

  if (afile->asampsize == 8) {
    // 8 bit sample size
    lives_read_buffered(afd, &val8, 1, FALSE);
    if (!(afile->signed_endian & AFORM_UNSIGNED)) val = val8 >= 128 ? val8 - 256 : val8;
    else val = val8 - 127;
    val /= 127.;
  } else {
    // 16 bit sample size
    lives_read_buffered(afd, &val8, 1, TRUE);
    lives_read_buffered(afd, &val8b, 1, TRUE);
    if (afile->signed_endian & AFORM_BIG_ENDIAN) val16 = (uint16_t)(val8 << 8) + val8b;
    else val16 = (uint16_t)(val8b << 8) + val8;
    if (!(afile->signed_endian & AFORM_UNSIGNED)) val = val16 >= 32768 ? val16 - 65536 : val16;
    else val = val16 - 32767;
    val /= 32767.;
  }

  //printf("val is %f\n",val);
  return val;
}


LIVES_GLOBAL_INLINE void sample_silence_dS(float *dst, uint64_t nsamples) {
  // send silence to the jack player
  lives_memset(dst, 0, nsamples * sizeof(float));
}


void sample_silence_stream(int nchans, int nframes) {
  float **fbuff = (float **)lives_calloc(nchans, sizeof(float *));
  boolean memok = TRUE;
  int i;

  for (i = 0; i < nchans; i++) {
    fbuff[i] = (float *)lives_calloc(nframes, sizeof(float));
    if (!fbuff[i]) memok = FALSE;
  }
  if (memok) {
    pthread_mutex_lock(&mainw->vpp_stream_mutex);
    if (mainw->ext_audio && mainw->vpp != NULL && mainw->vpp->render_audio_frame_float != NULL) {
      (*mainw->vpp->render_audio_frame_float)(fbuff, nframes);
    }
    pthread_mutex_unlock(&mainw->vpp_stream_mutex);
  }
  for (i = 0; i < nchans; i++) {
    lives_freep((void **)&fbuff[i]);
  }
  free(fbuff);
}


// TODO: going from >1 channels to 1, we should average
void sample_move_d8_d16(short *dst, uint8_t *src,
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  // convert 8 bit audio to 16 bit audio

  // endianess will be machine endian

  register int nSrcCount, nDstCount;
  register float src_offset_f = 0.f;
  register int src_offset_i = 0;
  register int ccount;
  unsigned char *ptr;
  unsigned char *src_end;

  // take care of rounding errors
  src_end = src + tbytes - nSrcChannels;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_f = ((float)(nsamples) * (-scale) - 1.f);
    src_offset_i = (int)src_offset_f * nSrcChannels;
  }

  while (nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;
    ccount = 0;

    /* loop until all of our destination channels are filled */
    while (nDstCount) {
      nSrcCount--;
      nDstCount--;

      ptr = src + ccount + src_offset_i;
      ptr = ptr > src
            ? (ptr < (src_end + ccount) ? ptr : (src_end + ccount)) : src;

      if (!swap_sign) *(dst++) = *(ptr) << 8;
      else if (swap_sign == SWAP_U_TO_S) *(dst++) = ((short)(*(ptr)) - 128) << 8;
      else *((unsigned short *)(dst++)) = ((short)(*(ptr)) + 128) << 8;
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if (!nSrcCount && nDstCount) {
        ccount = 0;
        nSrcCount = nSrcChannels;
      }
    }

    /* advance the the position */
    src_offset_i = (int)(src_offset_f += scale) * nSrcChannels;
  }
}


/* convert from any number of source channels to any number of destination channels - both interleaved */
// TODO: going from >1 channels to 1, we should average
void sample_move_d16_d16(int16_t *dst, int16_t *src,
                         uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_endian, int swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f = 0.f;
  register int src_offset_i = 0;
  register int ccount = 0;
  int16_t *ptr;
  int16_t *src_end;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_f = ((float)(nsamples) * (-scale) - 1.f);
    src_offset_i = (int)src_offset_f * nSrcChannels;
  }

  // take care of rounding errors
  src_end = src + tbytes / sizeof(short) - nSrcChannels;

  while (nsamples--) {  // vagrind
    if ((nSrcCount = nSrcChannels) == (nDstCount = nDstChannels) && !swap_endian && !swap_sign) {
      // same number of channels

      if (scale == 1.f) {
        lives_memcpy((void *)dst, (void *)src, tbytes);
        return;
      }

      ptr = src + src_offset_i;
      ptr = ptr > src ? (ptr < src_end ? ptr : src_end) : src;

      lives_memcpy(dst, ptr, nSrcChannels * 2);
      dst += nDstCount;
    } else {
      ccount = 0;

      /* loop until all of our destination channels are filled */
      while (nDstCount) {
        nSrcCount--;
        nDstCount--;

        ptr = src + ccount + src_offset_i;
        ptr = ptr > src ? (ptr < (src_end + ccount) ? ptr : (src_end + ccount)) : src;

        /* copy the data over */
        if (!swap_endian) {
          if (!swap_sign) *(dst++) = *ptr;
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(*ptr + SAMPLE_MAX_16BITI);
          else *(dst++) = *ptr - SAMPLE_MAX_16BITI;
        } else if (swap_endian == SWAP_X_TO_L) {
          if (!swap_sign) *(dst++) = (((*ptr) & 0x00FF) << 8) + ((*ptr) >> 8);
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(((*ptr & 0x00FF) << 8) + (*ptr >> 8) + SAMPLE_MAX_16BITI);
          else *(dst++) = ((*ptr & 0x00FF) << 8) + (*ptr >> 8) - SAMPLE_MAX_16BITI;
        } else {
          if (!swap_sign) *(dst++) = (((*ptr) & 0x00FF) << 8) + ((*ptr) >> 8);
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(((((uint16_t)(*ptr + SAMPLE_MAX_16BITI)) & 0x00FF) << 8) +
                (((uint16_t)(*ptr + SAMPLE_MAX_16BITI)) >> 8));
          else *(dst++) = ((((int16_t)(*ptr - SAMPLE_MAX_16BITI)) & 0x00FF) << 8) + (((int16_t)(*ptr - SAMPLE_MAX_16BITI)) >> 8);
        }

        ccount++;

        /* if we ran out of source channels but not destination channels */
        /* then start the src channels back where we were */
        if (!nSrcCount && nDstCount) {
          ccount = 0;
          nSrcCount = nSrcChannels;
        }
      }
    }
    /* advance the the position */
    src_offset_i = (int)(src_offset_f += scale) * nSrcChannels;
  }
}


/* convert from any number of source channels to any number of destination channels - 8 bit output */
// TODO: going from >1 channels to 1, we should average
void sample_move_d16_d8(uint8_t *dst, short *src,
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f = 0.f;
  register int src_offset_i = 0;
  register int ccount = 0;
  short *ptr;
  short *src_end;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_f = ((float)(nsamples) * (-scale) - 1.f);
    src_offset_i = (int)src_offset_f * nSrcChannels;
  }

  src_end = src + tbytes / sizeof(short) - nSrcChannels;

  while (nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;

    ccount = 0;

    /* loop until all of our destination channels are filled */
    while (nDstCount) {
      nSrcCount--;
      nDstCount--;

      ptr = src + ccount + src_offset_i;
      ptr = ptr > src ? (ptr < (src_end + ccount) ? ptr : src_end + ccount) : src;

      /* copy the data over */
      if (!swap_sign) *(dst++) = (*ptr >> 8);
      else if (swap_sign == SWAP_S_TO_U) *(dst++) = (uint8_t)((int8_t)(*ptr >> 8) + 128);
      else *((int8_t *)dst++) = (int8_t)((uint8_t)(*ptr >> 8) - 128);
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if (!nSrcCount && nDstCount) {
        ccount = 0;
        nSrcCount = nSrcChannels;
      }
    }

    /* advance the the position */
    src_offset_i = (int)(src_offset_f += scale) * nSrcChannels;
  }
}


float sample_move_d16_float(float *dst, short *src, uint64_t nsamples, uint64_t src_skip, int is_unsigned, boolean rev_endian, float vol) {
  // convert 16 bit audio to float audio

  // returns abs(maxvol heard)

  register float svolp, svoln;

#ifdef ENABLE_OIL
  float val = 0.; // set a value to stop valgrind complaining
  float maxval = 0.;
  double xn, xp, xa;
  double y = 0.f;
#else
  register float val;
  register float maxval = 0.;
  register short valss;
#endif

  uint8_t srcx[2];
  short srcxs;
  short *srcp;

  if (vol == 0.) vol = 0.0000001f;
  svolp = SAMPLE_MAX_16BIT_P / vol;
  svoln = SAMPLE_MAX_16BIT_N / vol;

#ifdef ENABLE_OIL
  xp = 1. / svolp;
  xn = 1. / svoln;
  xa = 2.*vol / (SAMPLE_MAX_16BIT_P + SAMPLE_MAX_16BIT_N);
#endif

  while (nsamples--) { // valgrind
    if (rev_endian) {
      lives_memcpy(&srcx, src, 2);
      srcxs = ((srcx[1] & 0xFF)  << 8) + (srcx[0] & 0xFF);
      srcp = &srcxs;
    } else srcp = src;

    if (!is_unsigned) {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_s16(&val, srcp, 1, &y, val > 0 ? &xp : &xn);
#else
      if ((val = (float)((float)(*srcp) / (*srcp > 0 ? svolp : svoln))) > 1.0f) val = 1.0f;
      else if (val < -1.0f) val = -1.0f;
#endif
    } else {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_u16(&val, (unsigned short *)srcp, 1, &y, &xa);
      val -= vol;
#else
      valss = (unsigned short) * srcp - SAMPLE_MAX_16BITI;
      if ((val = (float)((float)(valss) / (valss > 0 ? svolp : svoln))) > 1.0f) val = 1.0f;
      else if (val < -1.0f) val = -1.0f;
#endif
    }

    if (val > maxval) maxval = val;
    else if (-val > maxval) maxval = -val;

    *(dst++) = val;
    src += src_skip;
  }
  return maxval;
}


void sample_move_float_float(float *dst, float *src, uint64_t nsamples, float scale, int dst_skip) {
  // copy one channel of float to a buffer, applying the scale (scale 2.0 to double the rate, etc)
  size_t offs = 0;
  float offs_f = 0.;
  register int i;

  if (scale == 1.f && dst_skip == 1) {
    lives_memcpy((void *)dst, (void *)src, nsamples * sizeof(float));
    return;
  }

  for (i = 0; i < nsamples; i++) {
    *dst = src[offs];
    dst += dst_skip;
    offs = (size_t)(offs_f += scale);
  }
}


int64_t sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, float scale, int chans, int asamps,
                              int usigned, boolean rev_endian, boolean interleaved, float vol) {
  // convert float samples back to int
  // interleaved is for the float buffer; output int is always interleaved

  // scale is out_sample_rate / in_sample_rate (so 2.0 would play twice as fast, etc.)

  // nsamps is number of samples, asamps is sample bit size (8 or 16)

  // output is in holding_buff which can be cast to uint8_t * or uint16_t *

  // returns number of frames out

  int64_t frames_out = 0l;
  register int i;
  register int offs = 0, coffs = 0;
  register float coffs_f = 0.f;
  short *hbuffs = (short *)holding_buff;
  unsigned short *hbuffu = (unsigned short *)holding_buff;
  unsigned char *hbuffc = (unsigned char *)holding_buff;

  register short val;
  register unsigned short valu = 0;
  register float valf;

  while ((nsamps - coffs) > 0) {
    frames_out++;
    for (i = 0; i < chans; i++) {
      valf = *(float_buffer[i] + (interleaved ? (coffs * chans) : coffs));
      valf *= vol;
      val = (short)(valf * (valf > 0. ? SAMPLE_MAX_16BIT_P : SAMPLE_MAX_16BIT_N));

      if (usigned) valu = (val + SAMPLE_MAX_16BITI);

      if (asamps == 16) {
        if (!rev_endian) {
          if (usigned) *(hbuffu + offs) = valu;
          else *(hbuffs + offs) = val;
        } else {
          if (usigned) {
            *(hbuffc + offs) = valu & 0x00FF;
            *(hbuffc + (++offs)) = (valu & 0xFF00) >> 8;
          } else {
            *(hbuffc + offs) = val & 0x00FF;
            *(hbuffc + (++offs)) = (val & 0xFF00) >> 8;
          }
        }
      } else {
        *(hbuffc + offs) = (unsigned char)((float)val / 256.);
      }
      offs++;
    }
    coffs = (int)(coffs_f += scale);
  }
  return frames_out;
}


// play from memory buffer

int64_t sample_move_abuf_float(float **obuf, int nchans, int nsamps, int out_arate, float vol) {
  int samples_out = 0;

#ifdef ENABLE_JACK

  int samps = 0;

  lives_audio_buf_t *abuf;
  int in_arate;
  register size_t offs = 0, ioffs, xchan;

  register float src_offset_f = 0.f;
  register int src_offset_i = 0;

  register int i, j;

  register double scale;

  size_t curval;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->jackd->read_abuf == -1) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return 0;
  }
  abuf = mainw->jackd->abufs[mainw->jackd->read_abuf];
  in_arate = abuf->arate;
  pthread_mutex_unlock(&mainw->abuf_mutex);
  scale = (double)in_arate / (double)out_arate;

  while (nsamps > 0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->jackd->read_abuf == -1) {
      pthread_mutex_unlock(&mainw->abuf_mutex);
      return 0;
    }

    ioffs = abuf->start_sample;
    pthread_mutex_unlock(&mainw->abuf_mutex);
    samps = 0;

    src_offset_i = 0;
    src_offset_f = 0.;

    for (i = 0; i < nsamps; i++) {
      // process each sample

      if ((curval = ioffs + src_offset_i) >= abuf->samples_filled) {
        // current buffer is consumed
        break;
      }
      xchan = 0;
      for (j = 0; j < nchans; j++) {
        // copy channel by channel (de-interleave)
        pthread_mutex_lock(&mainw->abuf_mutex);
        if (mainw->jackd->read_abuf < 0) {
          pthread_mutex_unlock(&mainw->abuf_mutex);
          return samples_out;
        }
        if (xchan >= abuf->out_achans) xchan = 0;
        obuf[j][offs + i] = abuf->bufferf[xchan][curval] * vol;
        pthread_mutex_unlock(&mainw->abuf_mutex);
        xchan++;
      }
      // resample on the fly
      src_offset_i = (int)(src_offset_f += scale);
      samps++;
      samples_out++;
    }

    abuf->start_sample = ioffs + src_offset_i;
    nsamps -= samps;
    offs += samps;

    if (nsamps > 0) {
      // buffer was consumed, move on to next buffer

      pthread_mutex_lock(&mainw->abuf_mutex);
      // request main thread to fill another buffer
      mainw->abufs_to_fill++;
      if (mainw->jackd->read_abuf < 0) {
        // playback ended while we were processing
        pthread_mutex_unlock(&mainw->abuf_mutex);
        return samples_out;
      }

      mainw->jackd->read_abuf++;

      if (mainw->jackd->read_abuf >= prefs->num_rtaudiobufs) mainw->jackd->read_abuf = 0;

      abuf = mainw->jackd->abufs[mainw->jackd->read_abuf];

      pthread_mutex_unlock(&mainw->abuf_mutex);
    }
  }
#endif

  return samples_out;
}


int64_t sample_move_abuf_int16(short *obuf, int nchans, int nsamps, int out_arate) {
  int samples_out = 0;

#ifdef HAVE_PULSE_AUDIO

  int samps = 0;

  lives_audio_buf_t *abuf;
  int in_arate, nsampsx;
  register size_t offs = 0, ioffs, xchan;

  register float src_offset_f = 0.f;
  register int src_offset_i = 0;

  register int i, j;

  register double scale;
  size_t curval;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->pulsed->read_abuf == -1) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return 0;
  }

  abuf = mainw->pulsed->abufs[mainw->pulsed->read_abuf];
  in_arate = abuf->arate;
  pthread_mutex_unlock(&mainw->abuf_mutex);
  scale = (double)in_arate / (double)out_arate;

  while (nsamps > 0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->pulsed->read_abuf == -1) {
      pthread_mutex_unlock(&mainw->abuf_mutex);
      return 0;
    }

    ioffs = abuf->start_sample;
    pthread_mutex_unlock(&mainw->abuf_mutex);
    samps = 0;

    src_offset_i = 0;
    src_offset_f = 0.;
    nsampsx = nsamps * nchans;

    for (i = 0; i < nsampsx; i += nchans) {
      // process each sample

      if ((curval = ioffs + src_offset_i) >= abuf->samples_filled) {
        // current buffer is drained
        break;
      }
      xchan = 0;
      curval *= abuf->out_achans;
      for (j = 0; j < nchans; j++) {
        pthread_mutex_lock(&mainw->abuf_mutex);
        if (mainw->pulsed->read_abuf < 0) {
          pthread_mutex_unlock(&mainw->abuf_mutex);
          return samps;
        }
        if (xchan >= abuf->out_achans) xchan = 0;
        obuf[(offs++)] = abuf->buffer16[0][curval + xchan];
        pthread_mutex_unlock(&mainw->abuf_mutex);
        xchan++;
      }
      src_offset_i = (int)(src_offset_f += scale);
      samps++;
    }

    abuf->start_sample = ioffs + src_offset_i;
    nsamps -= samps;
    samples_out += samps;

    if (nsamps > 0) {
      // buffer was drained, move on to next buffer
      pthread_mutex_lock(&mainw->abuf_mutex);
      // request main thread to fill another buffer
      mainw->abufs_to_fill++;

      if (mainw->pulsed->read_abuf < 0) {
        // playback ended while we were processing
        pthread_mutex_unlock(&mainw->abuf_mutex);
        return samples_out;
      }

      mainw->pulsed->read_abuf++;
      if (mainw->pulsed->read_abuf >= prefs->num_rtaudiobufs) mainw->pulsed->read_abuf = 0;
      abuf = mainw->pulsed->abufs[mainw->pulsed->read_abuf];
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }
  }

#endif

  return samples_out;
}


/// copy a memory chunk into an audio buffer

static size_t chunk_to_float_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int chans = abuf->out_achans;
  register size_t offs = abuf->samples_filled;
  register int i;

  for (i = 0; i < chans; i++) {
    lives_memcpy(&(abuf->bufferf[i][offs]), float_buffer[i], nsamps * sizeof(float));
  }
  return (size_t)nsamps;
}


boolean float_deinterleave(float *fbuffer, int nsamps, int nchans) {
  // deinterleave a float buffer
  register int i, j;

  float *tmpfbuffer = (float *)lives_calloc_safety(nsamps * nchans, sizeof(float));
  if (tmpfbuffer == NULL) return FALSE;

  for (i = 0; i < nsamps; i++) {
    for (j = 0; j < nchans; j++) {
      tmpfbuffer[nsamps * j + i] = fbuffer[i * nchans + j];
    }
  }
  lives_memcpy(fbuffer, tmpfbuffer, nsamps * nchans * sizeof(float));
  lives_free(tmpfbuffer);
  return TRUE;
}


boolean float_interleave(float *fbuffer, int nsamps, int nchans) {
  // deinterleave a float buffer
  register int i, j;

  float *tmpfbuffer = (float *)lives_calloc_safety(nsamps * nchans, sizeof(float));
  if (tmpfbuffer == NULL) return FALSE;

  for (i = 0; i < nsamps; i++) {
    for (j = 0; j < nchans; j++) {
      tmpfbuffer[i * nchans + j] = fbuffer[j * nsamps + i];
    }
  }
  lives_memcpy(fbuffer, tmpfbuffer, nsamps * nchans * sizeof(float));
  lives_free(tmpfbuffer);
  return TRUE;
}


// for pulse audio we use int16, and let it apply the volume

static size_t chunk_to_int16_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int frames_out = 0;
  int chans = abuf->out_achans;
  register size_t offs = abuf->samples_filled * chans;
  register int i;
  register float valf;

  while (frames_out < nsamps) {
    for (i = 0; i < chans; i++) {
      valf = float_buffer[i][frames_out];
      abuf->buffer16[0][offs + i] = (short)(valf * (valf > 0. ? SAMPLE_MAX_16BIT_P : SAMPLE_MAX_16BIT_N));
    }
    frames_out++;
    offs += chans;
  }
  return (size_t)frames_out;
}


//#define DEBUG_ARENDER

static boolean pad_with_silence(int out_fd, off64_t oins_size, int64_t ins_size, int asamps, int aunsigned, boolean big_endian) {
  // fill to ins_pt with zeros (or 0x80.. for unsigned)
  // oins_size is the current file length, ins_size is the point where we want append to (both in bytes)
  // if ins size < oins_size we just seek to ins_size
  // otherwise we pad from oins_size to ins_size

  uint8_t *zero_buff;
  size_t sblocksize = SILENCE_BLOCK_SIZE;
  int sbytes = ins_size - oins_size;
  register int i;

  boolean retval = TRUE;

#ifdef DEBUG_ARENDER
  g_print("sbytes is %d\n", sbytes);
#endif
  if (sbytes > 0) {
    lseek64(out_fd, oins_size, SEEK_SET);
    if (!aunsigned) zero_buff = (uint8_t *)lives_calloc_safety(SILENCE_BLOCK_SIZE >> 3, 8);
    else {
      zero_buff = (uint8_t *)lives_calloc_safety(SILENCE_BLOCK_SIZE >> 3, 8);
      if (asamps > 1) {
        for (i = 0; i < SILENCE_BLOCK_SIZE; i += 2) {
          if (big_endian) {
            lives_memset(zero_buff + i, 0x80, 1);
            lives_memset(zero_buff + i + 1, 0x00, 1);
          } else {
            lives_memset(zero_buff + i, 0x00, 1);
            lives_memset(zero_buff + i + 1, 0x80, 1);
          }
        }
      }
    }

    for (i = 0; i < sbytes; i += SILENCE_BLOCK_SIZE) {
      if (sbytes - i < SILENCE_BLOCK_SIZE) sblocksize = sbytes - i;
      mainw->write_failed = FALSE;
      lives_write(out_fd, zero_buff, sblocksize, TRUE);
      if (mainw->write_failed) retval = FALSE;
    }
    lives_free(zero_buff);
  } else if (sbytes <= 0) {
    lseek64(out_fd, ins_size, SEEK_SET);
  }
  return retval;
}


static void audio_process_events_to(weed_timecode_t tc) {
  // process events from mainw->audio_event up to tc
  // at playback start we should have set mainw->audio_event and mainw->afilter_map

  int hint, error;

  weed_plant_t *event = mainw->audio_event;

  weed_timecode_t ctc;

  while (event != NULL && get_event_timecode(event) <= tc) {
    hint = get_event_hint(event);
    ctc = get_event_timecode(event);

    switch (hint) {
    case WEED_EVENT_HINT_FILTER_INIT: {
      weed_plant_t *deinit_event;
      if (!weed_plant_has_leaf(event, WEED_LEAF_DEINIT_EVENT)) {
        LIVES_ERROR("Init event with no deinit !\n");
      } else {
        deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, &error);
        if (deinit_event == NULL) {
          LIVES_ERROR("NULL deinit !\n");
        } else {
          if (get_event_timecode(deinit_event) < tc) break;
        }
      }
      process_events(event, TRUE, ctc);
    }
    break;
    case WEED_EVENT_HINT_FILTER_DEINIT: {
      weed_plant_t *init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error);
      if (weed_plant_has_leaf(init_event, WEED_LEAF_HOST_TAG)) {
        process_events(event, TRUE, ctc);
      }
    }
    break;
    case WEED_EVENT_HINT_FILTER_MAP:
#ifdef DEBUG_EVENTS
      g_print("got new audio effect map\n");
#endif
      mainw->afilter_map = event;
      break;
    default:
      break;
    }
    event = get_next_event(event);
  }

  mainw->audio_event = event;
}


/**
   @brief render a chunk of audio, apply effects and mixing it

    called during multitrack rendering to create the actual audio file
    (or in-memory buffer for preview playback in multitrack)

    also used for fade-in/fade-out in the clip editor (opvol_start, opvol_end)

    in multitrack, chvol is taken from the audio mixer; opvol is always 1.

    what we will do here:
    calculate our target samples (= period * out_rate)

    calculate how many in_samples for each track (= period * in_rate / ABS (vel) )

    read in the relevant number of samples for each track and convert to float

    write this into our float buffers (1 buffer per channel per track)

    we then send small chunks at a time to any audio effects; this is to allow for parameter interpolation

    the small chunks are processed and mixed, converted from float back to int, and then written to the outfile

    if obuf != NULL we write to obuf instead */
//#define DEBUG_ARENDER
int64_t render_audio_segment(int nfiles, int *from_files, int to_file, double *avels, double *fromtime,
                             weed_timecode_t tc_start, weed_timecode_t tc_end, double *chvol, double opvol_start,
                             double opvol_end, lives_audio_buf_t *obuf) {

  // TODO - allow MAX_AUDIO_MEM to be configurable; currently this is fixed at 8 MB
  // 16 or 32 may be a more sensible default for realtime previewing
  // return (audio) frames rendered

  weed_plant_t *shortcut = NULL;
  lives_clip_t *outfile = to_file > -1 ? mainw->files[to_file] : NULL;
  uint8_t *in_buff;
  void *finish_buff = NULL;  ///< only used if we are writing output to a file
  double *vis = NULL;
  short *holding_buff;
  weed_layer_t **layers = NULL;
  char *infilename, *outfilename;

  off64_t seekstart[nfiles];

  int in_fd[nfiles];
  int in_asamps[nfiles];
  int in_achans[nfiles];
  int in_arate[nfiles];
  int in_unsigned[nfiles];

  boolean in_reverse_endian[nfiles];
  boolean is_silent[nfiles];

  size_t max_aud_mem, bytes_to_read, aud_buffer;
  size_t tbytes;

  ssize_t bytes_read;

  uint64_t nframes;

  weed_timecode_t tc = tc_start;

  double ins_pt = tc / TICKS_PER_SECOND_DBL;
  double time = 0.;
  double opvol = opvol_start;
  double zavel, zavel_max = 0.;

  boolean out_reverse_endian = FALSE;
  boolean is_fade = FALSE;

  int out_asamps = to_file > -1 ? outfile->asampsize / 8 : 0;
  int out_achans = to_file > -1 ? outfile->achans : obuf->out_achans;
  int out_arate = to_file > -1 ? outfile->arate : obuf->arate;
  int out_unsigned = to_file > -1 ? outfile->signed_endian & AFORM_UNSIGNED : 0;
  int out_bendian = to_file > -1 ? outfile->signed_endian & AFORM_BIG_ENDIAN : 0;

  int track;
  int in_bendian;
  int first_nonsilent = -1;
  int max_segments;
  int render_block_size = RENDER_BLOCK_SIZE;
  int c, x, y;
  int out_fd = -1;

  register int i, j;

  int64_t frames_out = 0;
  int64_t ins_size = 0l, cur_size;
  int64_t tsamples = ((double)(tc_end - tc_start) / TICKS_PER_SECOND_DBL * out_arate + .5);
  int64_t blocksize, zsamples, xsamples;
  int64_t tot_frames = 0l;

  float *float_buffer[out_achans * nfiles];
  float *chunk_float_buffer[out_achans * nfiles];

  if (out_achans * nfiles * tsamples == 0) return 0l;

  if (to_file > -1 && mainw->multitrack == NULL && opvol_start != opvol_end) is_fade = TRUE;

  if (!storedfdsset) audio_reset_stored_fnames();

  if (!(is_fade) && (mainw->event_list == NULL || (mainw->multitrack == NULL && nfiles == 1 && from_files != NULL &&
                     from_files != NULL && from_files[0] == mainw->ascrap_file))) render_block_size *= 100;

  if (to_file > -1) {
    // prepare outfile stuff
    outfilename = lives_get_audio_file_name(to_file);
#ifdef DEBUG_ARENDER
    g_print("writing to %s\n", outfilename);
#endif
    out_fd = lives_open3(outfilename, O_WRONLY | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR);
    lives_free(outfilename);

    if (out_fd < 0) {
      lives_freep((void **)&mainw->write_failed_file);
      mainw->write_failed_file = lives_strdup(outfilename);
      mainw->write_failed = TRUE;
      return 0l;
    }

    cur_size = get_file_size(out_fd);

    if (opvol_start == opvol_end && opvol_start == 0.) ins_pt = tc_end / TICKS_PER_SECOND_DBL;
    ins_pt *= out_achans * out_arate * out_asamps;
    ins_size = ((int64_t)(ins_pt / out_achans / out_asamps + .5)) * out_achans * out_asamps;

    if ((!out_bendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
      out_reverse_endian = TRUE;
    else out_reverse_endian = FALSE;

    // fill to ins_pt with zeros
    pad_with_silence(out_fd, cur_size, ins_size, out_asamps, out_unsigned, out_bendian);

    if (opvol_start == opvol_end && opvol_start == 0.) {
      close(out_fd);
      return tsamples;
    }
  } else {
    if (mainw->event_list != NULL) cfile->aseek_pos = fromtime[0];

    tc_end -= tc_start;
    tc_start = 0;

    if (tsamples > obuf->samp_space - obuf->samples_filled) tsamples = obuf->samp_space - obuf->samples_filled;
  }

#ifdef DEBUG_ARENDER
  g_print("here %d %ld %ld %d\n", nfiles, tc_start, tc_end, to_file);
#endif

  for (track = 0; track < nfiles; track++) {
    // prepare infile stuff
    lives_clip_t *infile;

#ifdef DEBUG_ARENDER
    g_print(" track %d %d %.4f %.4f\n", track, from_files[track], fromtime[track], avels[track]);
#endif

    if (avels[track] == 0.) {
      is_silent[track] = TRUE;
      continue;
    }

    is_silent[track] = FALSE;

    infile = mainw->files[from_files[track]];

    in_asamps[track] = infile->asampsize / 8;
    in_achans[track] = infile->achans;
    in_arate[track] = infile->arate;
    in_unsigned[track] = infile->signed_endian & AFORM_UNSIGNED;
    in_bendian = infile->signed_endian & AFORM_BIG_ENDIAN;

    if (LIVES_UNLIKELY(in_achans[track] == 0)) is_silent[track] = TRUE;
    else {
      if ((!in_bendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
          (in_bendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
        in_reverse_endian[track] = TRUE;
      else in_reverse_endian[track] = FALSE;

      seekstart[track] = (off64_t)(fromtime[track] * in_arate[track]) * in_achans[track] * in_asamps[track];
      seekstart[track] = ((off64_t)(seekstart[track] / in_achans[track] / (in_asamps[track]))) * in_achans[track] * in_asamps[track];

      zavel = avels[track] * (double)in_arate[track] / (double)out_arate * in_asamps[track] * in_achans[track] / sizeof(float);
      if (ABS(zavel) > zavel_max) zavel_max = ABS(zavel);

      infilename = lives_get_audio_file_name(from_files[track]);

      // try to speed up access by keeping some files open
      if (track < NSTOREDFDS && storedfnames[track] != NULL && !strcmp(infilename, storedfnames[track])) {
        in_fd[track] = storedfds[track];
      } else {
        if (track < NSTOREDFDS && storedfds[track] > -1) close(storedfds[track]);
        in_fd[track] = lives_open2(infilename, O_RDONLY);
        if (in_fd[track] < 0) {
          lives_freep((void **)&mainw->read_failed_file);
          mainw->read_failed_file = lives_strdup(infilename);
          mainw->read_failed = TRUE;
        }

        if (track < NSTOREDFDS) {
          storedfds[track] = in_fd[track];
          storedfnames[track] = lives_strdup(infilename);
        }
      }

      if (in_fd[track] > -1) lseek64(in_fd[track], seekstart[track], SEEK_SET);

      lives_free(infilename);
    }
  }

  for (track = 0; track < nfiles; track++) {
    if (!is_silent[track]) {
      first_nonsilent = track;
      break;
    }
  }

  if (first_nonsilent == -1) {
    // all in tracks are empty
    // output silence
    if (to_file > -1) {
      int64_t oins_size = ins_size;
      ins_pt = tc_end / TICKS_PER_SECOND_DBL;
      ins_pt *= out_achans * out_arate * out_asamps;
      ins_size = ((int64_t)(ins_pt / out_achans / out_asamps) + .5) * out_achans * out_asamps;
      pad_with_silence(out_fd, oins_size, ins_size, out_asamps, out_unsigned, out_bendian);
      //sync();
      close(out_fd);
    } else {
      for (i = 0; i < out_achans; i++) {
        for (j = obuf->samples_filled; j < obuf->samples_filled + tsamples; j++) {
          if (prefs->audio_player == AUD_PLAYER_JACK) {
            obuf->bufferf[i][j] = 0.;
          } else {
            if (!out_unsigned) obuf->buffer16[0][j * out_achans + i] = 0x00;
            else {
              if (out_bendian) {
                lives_memset(&obuf->buffer16_8[0][(j * out_achans + i) * 2], 0x80, 1);
                lives_memset(&obuf->buffer16_8[0][(j * out_achans + i) * 2 + 1], 0x00, 1);
              } else obuf->buffer16[0][j * out_achans + i] = 0x0080;
            }
          }
        }
      }

      obuf->samples_filled += tsamples;
    }

    return tsamples;
  }

  /// we don't want to use more than MAX_AUDIO_MEM bytes
  /// (numbers will be much larger than examples given)
  max_aud_mem = MAX_AUDIO_MEM / (1.5 + zavel_max); // allow for size of holding_buff and in_buff
  max_aud_mem = (max_aud_mem >> 7) << 7; // round to a multiple of 128
  max_aud_mem = max_aud_mem / out_achans / nfiles; // max mem per channel/track

  // we use float here because our audio effects use float
  // tsamples is total samples (30 in this example)
  bytes_to_read = tsamples * (sizeof(float)); // eg. 120 (30 samples)

  // how many segments do we need to read all bytes ?
  max_segments = (int)((double)bytes_to_read / (double)max_aud_mem + 1.); // max segments (rounded up) [e.g ceil(120/45)==3]

  // then, how many bytes per segment
  aud_buffer = bytes_to_read / max_segments; // estimate of buffer size (e.g. 120/3 = 40)

  zsamples = (int)(aud_buffer / sizeof(float) + .5); // ensure whole number of samples (e.g 40 == 10 samples), round up

  xsamples = zsamples + (tsamples - (max_segments * zsamples)); // e.g 10 + 30 - 3 * 10 == 10

  holding_buff = (short *)lives_calloc_safety(xsamples * out_achans,  sizeof(short));

  for (i = 0; i < out_achans * nfiles; i++) {
    float_buffer[i] = (float *)lives_calloc_safety(xsamples, sizeof(float));
  }

  if (to_file > -1)
    finish_buff = lives_calloc_safety(tsamples, out_achans * out_asamps);

#ifdef DEBUG_ARENDER
  g_print("  rendering %ld samples %f\n", tsamples, opvol);
#endif

  while (tsamples > 0) {
    tsamples -= xsamples;

    for (track = 0; track < nfiles; track++) {
      if (is_silent[track]) {
        // zero float_buff
        for (c = 0; c < out_achans; c++) {
          for (x = 0; x < xsamples; x++) {
            float_buffer[track * out_achans + c][x] = 0.;
          }
        }
        continue;
      }

      /// calculate tbytes for xsamples
      /// TODO: when previewing / rendering in the clip editor, we should interpolate the velocities between frames
      zavel = avels[track] * (double)in_arate[track] / (double)out_arate;

      /// tbytes: how many bytes we want ot read in. This is xsamples * the track velocity.
      /// we add a small random factor here, so half the time we round up, half the time we round down
      /// otherwise we would be gradually losing or gaining samples
      tbytes = (int)((double)xsamples * ABS(zavel) + ((double)fastrand() / (double)LIVES_MAXUINT64)) *
               in_asamps[track] * in_achans[track];

      in_buff = (uint8_t *)lives_calloc_safety(tbytes, 1);

      if (zavel < 0. && in_fd[track] > -1) lseek64(in_fd[track], seekstart[track] - tbytes, SEEK_SET);

      bytes_read = 0;
      mainw->read_failed = FALSE;

      if (in_fd[track] > -1) bytes_read = lives_read(in_fd[track], in_buff, tbytes, TRUE); // ## valgrind

      if (bytes_read < 0) bytes_read = 0;

      if (from_files[track] == mainw->ascrap_file) {
        // be forgiving with the ascrap file
        if (mainw->read_failed) mainw->read_failed = FALSE;
      }

      if (zavel < 0.) seekstart[track] -= bytes_read;

      if (bytes_read < tbytes && bytes_read >= 0) {
        if (zavel > 0) lives_memset(in_buff + bytes_read, 0, tbytes - bytes_read);
        else {
          lives_memmove(in_buff + tbytes - bytes_read, in_buff, bytes_read);
          lives_memset(in_buff, 0, tbytes - bytes_read);
        }
      }

      nframes = (tbytes / (in_asamps[track]) / in_achans[track] / ABS(zavel) + .001);

      /// convert to float
      /// - first we convert to 16 bit stereo (if it was 8 bit and / or mono) and we resample
      /// input is tbytes bytes at rate * velocity, and we should get out nframes audio frames at out_arate. out_achans
      /// result is in holding_buff
      if (in_asamps[track] == 1) {
        sample_move_d8_d16(holding_buff, (uint8_t *)in_buff, nframes, tbytes, zavel, out_achans, in_achans[track], 0);
      } else {
        sample_move_d16_d16(holding_buff, (short *)in_buff, nframes, tbytes, zavel, out_achans,
                            in_achans[track], in_reverse_endian[track] ? SWAP_X_TO_L : 0, 0);
      }

      lives_free(in_buff);

      for (c = 0; c < out_achans; c++) {
        /// now we convert to holding_buff to float in float_buffer and adjust the track volume
        sample_move_d16_float(float_buffer[c + track * out_achans], holding_buff + c, nframes,
                              out_achans, in_unsigned[track], FALSE, chvol[track]);
      }
    }

    // next we send small chunks at a time to the audio vol/pan effect + any other audio effects
    shortcut = NULL;
    blocksize = render_block_size; ///< this is our chunk size

    for (i = 0; i < xsamples; i += render_block_size) {
      if (i + render_block_size > xsamples) blocksize = xsamples - i;

      for (track = 0; track < nfiles; track++) {
        for (c = 0; c < out_achans; c++) {
          //g_print("xvals %.4f\n",*(float_buffer[track*out_achans+c]+i));
          chunk_float_buffer[track * out_achans + c] = float_buffer[track * out_achans + c] + i;
        }
      }

      if (mainw->event_list != NULL && mainw->filter_map != NULL
          && !(mainw->multitrack == NULL && from_files[0] == mainw->ascrap_file)) {
        // we need to apply all audio effects with output here.
        // even in clipedit mode (for preview/rendering with an event list)
        // also, we will need to keep updating mainw->filter_map from mainw->event_list,
        // as filters may switched on and off during the block

        int nbtracks = 0;

        // process events up to current tc:
        // filter inits and deinits, and filter maps will update the current fx state
        audio_process_events_to(tc);

        // apply audio filter(s)
        if (mainw->multitrack != NULL) {
          /// here we work out the "visibility" of each track at tc (i.e we only get audio from the front track + backing audio)
          /// any transitions will combine audio from 2 layers (if the pref is set)
          /// backing audio tracks are always full visible
          /// the array is used to set the values of the "is_volume_master" parameter of the effect (see the Weed Audio spec.)
          vis = get_track_visibility_at_tc(mainw->multitrack->event_list, nfiles,
                                           mainw->multitrack->opts.back_audio_tracks, tc, &shortcut,
                                           mainw->multitrack->opts.audio_bleedthru);

          /// first track is ascrap_file - flag that no effects should be applied to it, except for the audio mixer
          /// since effects were already applied to the saved audio
          if (mainw->ascrap_file > -1 && from_files[0] == mainw->ascrap_file) vis[0] = -vis[0];

          nbtracks = mainw->multitrack->opts.back_audio_tracks;
        }

        /// the audio is now packaged into audio layers, one for each track (file). This makes it easier to remap
        /// the audio tracks from effect to effect, as layers are interchangeable with filter channels
        layers = (weed_layer_t **)lives_calloc(nfiles, sizeof(weed_layer_t *));
        for (x = 0; x < nfiles; x++) {
          float **adata = (float **)lives_calloc(out_achans, sizeof(float *));
          layers[x] = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
          for (y = 0; y < out_achans; y++) {
            adata[y] = chunk_float_buffer[x * out_achans + y];
          }

          weed_layer_set_audio_data(layers[x], adata, out_arate, out_achans, blocksize);
          lives_free(adata);
        }

        /// apply the audo effects
        weed_apply_audio_effects(mainw->afilter_map, layers, nbtracks, out_achans, blocksize, out_arate, tc, vis);
        lives_freep((void **)&vis);

        if (layers != NULL) {
          /// after processing we get the audio data back from the layers
          for (x = 0; x < nfiles; x++) {
            float **adata = (weed_layer_get_audio_data(layers[x], NULL));
            for (y = 0; y < out_achans; y++) {
              chunk_float_buffer[x * out_achans + y] = adata[y];
            }
            lives_free(adata);
            weed_layer_set_audio_data(layers[x], NULL, 0, 0, 0);
            weed_layer_free(layers[x]);
          }
          lives_freep((void **)&layers);
        }
      }

      if (mainw->multitrack == NULL && opvol_end != opvol_start) {
        time += (double)frames_out / (double)out_arate;
        opvol = opvol_start + (opvol_end - opvol_start) * (time / (double)((tc_end - tc_start) / TICKS_PER_SECOND_DBL));
      }

      if (is_fade) {
        // output to file
        // convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
        frames_out = sample_move_float_int((void *)finish_buff, chunk_float_buffer, blocksize, 1., out_achans,
                                           out_asamps * 8, out_unsigned, out_reverse_endian, FALSE, opvol);
        lives_write(out_fd, finish_buff, frames_out * out_asamps * out_achans, TRUE);
        threaded_dialog_spin(0.);
        tot_frames += frames_out;
#ifdef DEBUG_ARENDER
        g_print(".");
#endif
      }
      tc += (double)blocksize / (double)out_arate * TICKS_PER_SECOND_DBL;
    }

    if (!is_fade) {
      if (to_file > -1) {
        /// output to file:
        /// convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
        frames_out = sample_move_float_int((void *)finish_buff, float_buffer, xsamples, 1., out_achans,
                                           out_asamps * 8, out_unsigned, out_reverse_endian, FALSE, opvol);

        lives_write(out_fd, finish_buff, frames_out * out_asamps * out_achans, TRUE);
#ifdef DEBUG_ARENDER
        g_print(".");
#endif
        tot_frames += frames_out;
      } else {
        /// output to memory buffer; for jack we retain the float audio, for pulse we use int16_t
        if (prefs->audio_player == AUD_PLAYER_JACK) {
          frames_out = chunk_to_float_abuf(obuf, float_buffer, xsamples);
        } else {
          frames_out = chunk_to_int16_abuf(obuf, float_buffer, xsamples);
        }
        obuf->samples_filled += frames_out;
        tot_frames += frames_out;
      }
    }
    xsamples = zsamples;
  }

  if (xsamples > 0) {
    for (i = 0; i < out_achans * nfiles; i++) {
      if (float_buffer[i] != NULL) lives_free(float_buffer[i]);
    }
  }

  if (finish_buff != NULL) lives_free(finish_buff);
  if (holding_buff != NULL) lives_free(holding_buff);

  // close files
  for (track = 0; track < nfiles; track++) {
    if (!is_silent[track]) {
      if (track >= NSTOREDFDS && in_fd[track] > -1) close(in_fd[track]);
    }
  }

  if (to_file > -1) {
#ifdef DEBUG_ARENDER
    g_print("fs is %ld\n", get_file_size(out_fd));
#endif
    close(out_fd);
  }

  return tot_frames;
}


void aud_fade(int fileno, double startt, double endt, double startv, double endv) {
  double vel = 1., vol = 1.;

  mainw->read_failed = mainw->write_failed = FALSE;
  render_audio_segment(1, &fileno, fileno, &vel, &startt, (weed_timecode_t)(startt * TICKS_PER_SECOND_DBL),
                       (weed_timecode_t)(endt * TICKS_PER_SECOND_DBL), &vol, startv, endv, NULL);

  if (mainw->write_failed) {
    char *outfilename = lives_get_audio_file_name(fileno);
    do_write_failed_error_s(outfilename, NULL);
    lives_free(outfilename);
  }

  if (mainw->read_failed) {
    char *infilename = lives_get_audio_file_name(fileno);
    do_read_failed_error_s(infilename, NULL);
    lives_free(infilename);
  }
}


#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(int fileno, int old_file, lives_rec_audio_type_t rec_type) {
  // open audio file for writing
  lives_clip_t *outfile;

  boolean jackd_read_started = (mainw->jackd_read != NULL);

  int retval;

  // should we set is_paused ? (yes)
  // should we reset time (no)

  if (fileno == -1) {
    // respond to external audio, but do not record it (yet)
    if (mainw->jackd_read == NULL) {
      mainw->jackd_read = jack_get_driver(0, FALSE);
      mainw->jackd_read->playing_file = fileno;
      mainw->jackd_read->reverse_endian = FALSE;

      // start jack "recording"
      jack_create_client_reader(mainw->jackd_read);
      jack_read_driver_activate(mainw->jackd_read, FALSE);
    }
    return;
  }

  outfile = mainw->files[fileno];

  if (mainw->aud_rec_fd == -1) {
    char *outfilename = lives_get_audio_file_name(fileno);
    do {
      retval = 0;
      mainw->aud_rec_fd = lives_open3(outfilename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
      if (mainw->aud_rec_fd < 0) {
        retval = do_write_failed_error_s_with_retry(outfilename, lives_strerror(errno), NULL);
        if (retval == LIVES_RESPONSE_CANCEL) {
          lives_free(outfilename);
          return;
        }
      }
    } while (retval == LIVES_RESPONSE_RETRY);
    lives_free(outfilename);
    if (fileno == mainw->ascrap_file) mainw->files[mainw->ascrap_file]->cb_src = mainw->aud_rec_fd;
  }

  if (rec_type == RECA_GENERATED) {
    mainw->jackd->playing_file = fileno;
  } else {
    if (!jackd_read_started) {
      mainw->jackd_read = jack_get_driver(0, FALSE);
      jack_create_client_reader(mainw->jackd_read);
      jack_read_driver_activate(mainw->jackd_read, FALSE);
      mainw->jackd_read->is_paused = TRUE;
      jack_time_reset(mainw->jackd_read, 0);
    }
    mainw->jackd_read->playing_file = fileno;
  }

  if (rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED) {
    int asigned;
    int aendian;
    uint64_t fsize = get_file_size(mainw->aud_rec_fd);

    if (rec_type == RECA_EXTERNAL) {
      mainw->jackd_read->reverse_endian = FALSE;

      outfile->arate = outfile->arps = mainw->jackd_read->sample_in_rate;
      outfile->achans = mainw->jackd_read->num_input_channels;

      outfile->asampsize = 16;
      outfile->signed_endian = get_signed_endian(TRUE, TRUE);

      asigned = !(outfile->signed_endian & AFORM_UNSIGNED);
      aendian = !(outfile->signed_endian & AFORM_BIG_ENDIAN);

      mainw->jackd_read->frames_written = fsize / (outfile->achans * (outfile->asampsize >> 3));
    } else {
      mainw->jackd->reverse_endian = FALSE;
      outfile->arate = outfile->arps = mainw->jackd->sample_out_rate;
      outfile->achans = mainw->jackd->num_output_channels;

      outfile->asampsize = 16;
      outfile->signed_endian = get_signed_endian(TRUE, TRUE);

      asigned = !(outfile->signed_endian & AFORM_UNSIGNED);
      aendian = !(outfile->signed_endian & AFORM_BIG_ENDIAN);
    }

    save_clip_value(fileno, CLIP_DETAILS_ACHANS, &outfile->achans);
    save_clip_value(fileno, CLIP_DETAILS_ARATE, &outfile->arps);
    save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &outfile->arate);
    save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &outfile->asampsize);
    save_clip_value(fileno, CLIP_DETAILS_AENDIAN, &aendian);
    save_clip_value(fileno, CLIP_DETAILS_ASIGNED, &asigned);

  } else {
    int out_bendian = outfile->signed_endian & AFORM_BIG_ENDIAN;

    if ((!out_bendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
      mainw->jackd_read->reverse_endian = TRUE;
    else mainw->jackd_read->reverse_endian = FALSE;

    // start jack recording
    mainw->jackd_read = jack_get_driver(0, FALSE);
    jack_create_client_reader(mainw->jackd_read);
    jack_read_driver_activate(mainw->jackd_read, TRUE);
    mainw->jackd_read->is_paused = TRUE;
    jack_time_reset(mainw->jackd_read, 0);
  }

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type == RECA_WINDOW_GRAB || rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED) return;

  mainw->cancelled = CANCEL_NONE;
  mainw->cancel_type = CANCEL_SOFT;
  // show countdown/stop dialog
  mainw->suppress_dprint = FALSE;
  d_print(_("Recording audio..."));
  mainw->suppress_dprint = TRUE;
  if (rec_type == RECA_NEW_CLIP) {
    mainw->jackd_read->in_use = TRUE;
    do_auto_dialog(_("Recording audio..."), 1);
  } else {
    int current_file = mainw->current_file;
    mainw->current_file = old_file;
    on_playsel_activate(NULL, NULL);
    mainw->current_file = current_file;
  }
  mainw->cancel_type = CANCEL_KILL;
  jack_rec_audio_end(!(prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT), TRUE);
}


void jack_rec_audio_end(boolean close_device, boolean close_fd) {
  // recording ended

  pthread_mutex_lock(&mainw->audio_filewriteend_mutex);
  if (mainw->jackd_read->playing_file > -1)
    jack_flush_read_data(0, NULL);

  if (close_device) {
    // stop recording
    if (mainw->jackd_read != NULL) jack_close_device(mainw->jackd_read);
    mainw->jackd_read = NULL;
  } else {
    mainw->jackd_read->in_use = FALSE;
    mainw->jackd_read->playing_file = -1;
  }
  pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);

  if (close_fd && mainw->aud_rec_fd != -1) {
    // close file
    close(mainw->aud_rec_fd);
    mainw->aud_rec_fd = -1;
  }
}
#endif


#ifdef HAVE_PULSE_AUDIO

void pulse_rec_audio_to_clip(int fileno, int old_file, lives_rec_audio_type_t rec_type) {
  // open audio file for writing
  lives_clip_t *outfile;
  int retval;

  if (fileno == -1) {
    if (mainw->pulsed_read == NULL) {
      mainw->pulsed_read = pulse_get_driver(FALSE);
      mainw->pulsed_read->playing_file = -1;
      mainw->pulsed_read->frames_written = 0;
      mainw->pulsed_read->reverse_endian = FALSE;
      mainw->aud_rec_fd = -1;
      pulse_driver_activate(mainw->pulsed_read);
    }
    return;
  }

  outfile = mainw->files[fileno];

  if (mainw->aud_rec_fd == -1) {
    char *outfilename = lives_get_audio_file_name(fileno);
    do {
      retval = 0;
      mainw->aud_rec_fd = lives_open3(outfilename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
      if (mainw->aud_rec_fd < 0) {
        retval = do_write_failed_error_s_with_retry(outfilename, lives_strerror(errno), NULL);
        if (retval == LIVES_RESPONSE_CANCEL) {
          lives_free(outfilename);
          return;
        }
      }
    } while (retval == LIVES_RESPONSE_RETRY);
    lives_free(outfilename);
    if (fileno == mainw->ascrap_file) {
      mainw->files[mainw->ascrap_file]->cb_src = mainw->aud_rec_fd;
      /*      if (mainw->pulsed_read != NULL) {
        // flush all data from buffer; this seems like the only way
        void *data;
        size_t rbytes;
        pa_mloop_lock();
        do {
          pa_stream_peek(mainw->pulsed_read->pstream, (const void **)&data, &rbytes);
          if (rbytes > 0) pa_stream_drop(mainw->pulsed_read->pstream);
        } while (rbytes > 0);
        pa_mloop_unlock();
      }*/
    }
  }

  if (rec_type == RECA_GENERATED) {
    mainw->pulsed->playing_file = fileno;
  } else {
    mainw->pulsed_read = pulse_get_driver(FALSE);
    mainw->pulsed_read->playing_file = fileno;
    mainw->pulsed_read->frames_written = 0;
  }

  if (rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED) {
    int asigned;
    int aendian;
    uint64_t fsize = get_file_size(mainw->aud_rec_fd);

    if (rec_type == RECA_EXTERNAL) {
      mainw->pulsed_read->reverse_endian = FALSE;

      pulse_driver_activate(mainw->pulsed_read);

      outfile->arate = outfile->arps = mainw->pulsed_read->out_arate;
      outfile->achans = mainw->pulsed_read->out_achans;
      outfile->asampsize = mainw->pulsed_read->out_asamps;
      outfile->signed_endian = get_signed_endian(mainw->pulsed_read->out_signed != AFORM_UNSIGNED,
                               mainw->pulsed_read->out_endian != AFORM_BIG_ENDIAN);

      asigned = !(outfile->signed_endian & AFORM_UNSIGNED);
      aendian = !(outfile->signed_endian & AFORM_BIG_ENDIAN);

      mainw->pulsed_read->frames_written = fsize / (outfile->achans * (outfile->asampsize >> 3));
    } else {
      mainw->pulsed->reverse_endian = FALSE;
      outfile->arate = outfile->arps = mainw->pulsed->out_arate;
      outfile->achans = mainw->pulsed->out_achans;
      outfile->asampsize = mainw->pulsed->out_asamps;
      outfile->signed_endian = get_signed_endian(mainw->pulsed->out_signed != AFORM_UNSIGNED,
                               mainw->pulsed->out_endian != AFORM_BIG_ENDIAN);

      asigned = !(outfile->signed_endian & AFORM_UNSIGNED);
      aendian = !(outfile->signed_endian & AFORM_BIG_ENDIAN);
    }

    outfile->header_version = LIVES_CLIP_HEADER_VERSION;
    save_clip_value(fileno, CLIP_DETAILS_HEADER_VERSION, &outfile->header_version);
    save_clip_value(fileno, CLIP_DETAILS_ACHANS, &outfile->achans);
    save_clip_value(fileno, CLIP_DETAILS_ARATE, &outfile->arps);
    save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &outfile->arate);
    save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &outfile->asampsize);
    save_clip_value(fileno, CLIP_DETAILS_AENDIAN, &aendian);
    save_clip_value(fileno, CLIP_DETAILS_ASIGNED, &asigned);

  } else {
    int out_bendian = outfile->signed_endian & AFORM_BIG_ENDIAN;

    if ((!out_bendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
      mainw->pulsed_read->reverse_endian = TRUE;
    else mainw->pulsed_read->reverse_endian = FALSE;

    // start pulse recording
    pulse_driver_activate(mainw->pulsed_read);
  }

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type == RECA_WINDOW_GRAB || rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED) return;

  mainw->cancelled = CANCEL_NONE;
  mainw->cancel_type = CANCEL_SOFT;
  // show countdown/stop dialog
  mainw->suppress_dprint = FALSE;
  d_print(_("Recording audio..."));
  mainw->suppress_dprint = TRUE;
  if (rec_type == RECA_NEW_CLIP) {
    mainw->pulsed_read->in_use = TRUE;
    do_auto_dialog(_("Recording audio..."), 1);
  } else {
    int current_file = mainw->current_file;
    mainw->current_file = old_file;
    mainw->pulsed_read->is_paused = TRUE;
    mainw->pulsed_read->in_use = TRUE;
    on_playsel_activate(NULL, NULL);
    mainw->current_file = current_file;
  }
  mainw->cancel_type = CANCEL_KILL;
  pulse_rec_audio_end(!(prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT), TRUE);
}


void pulse_rec_audio_end(boolean close_device, boolean close_fd) {
  // recording ended

  // stop recording

  if (mainw->pulsed_read != NULL) {
    pthread_mutex_lock(&mainw->audio_filewriteend_mutex);
    if (mainw->pulsed_read->playing_file > -1)
      pulse_flush_read_data(mainw->pulsed_read, mainw->pulsed_read->playing_file, 0, mainw->pulsed_read->reverse_endian, NULL);

    if (close_device) pulse_close_client(mainw->pulsed_read);

    if (close_device) mainw->pulsed_read = NULL;
    else {
      mainw->pulsed_read->in_use = FALSE;
      mainw->pulsed_read->playing_file = -1;
    }
    pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
  }

  if (mainw->aud_rec_fd != -1 && close_fd) {
    // close file
    close(mainw->aud_rec_fd);
    mainw->aud_rec_fd = -1;
  }
}

#endif

/////////////////////////////////////////////////////////////////

// playback via memory buffers (e.g. in multitrack)

////////////////////////////////////////////////////////////////

static lives_audio_track_state_t *resize_audstate(lives_audio_track_state_t *ostate, int nostate, int nstate) {
  // increase the element size of the audstate array (ostate)
  // from nostate elements to nstate elements

  lives_audio_track_state_t *audstate
    = (lives_audio_track_state_t *)lives_calloc(nstate, sizeof(lives_audio_track_state_t));
  int i;

  for (i = 0; i < nstate; i++) {
    if (i < nostate) {
      audstate[i].afile = ostate[i].afile;
      audstate[i].seek = ostate[i].seek;
      audstate[i].vel = ostate[i].vel;
    } else {
      audstate[i].afile = 0;
      audstate[i].seek = audstate[i].vel = 0.;
    }
  }

  lives_freep((void **)&ostate);

  return audstate;
}


static lives_audio_track_state_t *aframe_to_atstate(weed_plant_t *event) {
  // parse an audio frame, and set the track file, seek and velocity values

  int error, atrack;
  int num_aclips = weed_leaf_num_elements(event, WEED_LEAF_AUDIO_CLIPS);
  int *aclips = weed_get_int_array(event, WEED_LEAF_AUDIO_CLIPS, &error);
  double *aseeks = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, &error);
  int naudstate = 0;
  lives_audio_track_state_t *atstate = NULL;

  register int i;

  int btoffs = mainw->multitrack != NULL ? mainw->multitrack->opts.back_audio_tracks : 1;

  for (i = 0; i < num_aclips; i += 2) {
    if (aclips[i + 1] > 0) { // else ignore
      atrack = aclips[i];
      if (atrack + btoffs >= naudstate - 1) {
        atstate = resize_audstate(atstate, naudstate, atrack + btoffs + 2);
        naudstate = atrack + btoffs + 1;
        atstate[naudstate].afile = -1;
      }
      atstate[atrack + btoffs].afile = aclips[i + 1];
      atstate[atrack + btoffs].seek = aseeks[i];
      atstate[atrack + btoffs].vel = aseeks[i + 1];
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

  weed_plant_t *nevent = get_first_event(event_list), *event;
  lives_audio_track_state_t *atstate = NULL, *audstate = NULL;
  weed_plant_t *deinit_event;
  int error, nfiles, nnfiles;
  weed_timecode_t last_tc = 0, fill_tc;
  int i;

  // gets effects state (initing any effects which should be active)

  // optionally: gets audio state at audio frame prior to st_event, sets atstate[0].tc
  // and initialises audio buffers

  mainw->filter_map = NULL;
  mainw->afilter_map = NULL;

  mainw->audio_event = st_event;

  fill_tc = get_event_timecode(st_event);

  do {
    event = nevent;
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      mainw->afilter_map = mainw->filter_map = event;
    } else if (WEED_EVENT_IS_FILTER_INIT(event)) {
      deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, &error);
      if (get_event_timecode(deinit_event) >= fill_tc) {
        // this effect should be activated
        process_events(event, FALSE, get_event_timecode(event));
        process_events(event, TRUE, get_event_timecode(event));
      }
    } else if (get_audstate && WEED_EVENT_IS_AUDIO_FRAME(event)) {
      atstate = aframe_to_atstate(event);

      if (audstate == NULL) audstate = atstate;
      else {
        // have an existing audio state, update with current
        for (nfiles = 0; audstate[nfiles].afile != -1; nfiles++);

        for (i = 0; i < nfiles; i++) {
          // increase seek values up to current frame
          audstate[i].seek += audstate[i].vel * (get_event_timecode(event) - last_tc) / TICKS_PER_SECOND_DBL;
        }

        for (nnfiles = 0; atstate[nnfiles].afile != -1; nnfiles++);

        if (nnfiles > nfiles) {
          audstate = resize_audstate(audstate, nfiles, nnfiles + 1);
          audstate[nnfiles].afile = -1;
        }

        for (i = 0; i < nnfiles; i++) {
          if (atstate[i].afile > 0) {
            audstate[i].afile = atstate[i].afile;
            audstate[i].seek = atstate[i].seek;
            audstate[i].vel = atstate[i].vel;
          }
        }
        lives_free(atstate);
      }

      last_tc = get_event_timecode(event);
    }
    nevent = get_next_event(event);
  } while (event != st_event);

  if (audstate != NULL) {
    for (nfiles = 0; audstate[nfiles].afile != -1; nfiles++);

    for (i = 0; i < nfiles; i++) {
      // increase seek values
      audstate[i].seek += audstate[i].vel * (fill_tc - last_tc) / TICKS_PER_SECOND_DBL;
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

  lives_audio_track_state_t *atstate = NULL;
  int nnfiles, i;
  double chvols[MAX_AUDIO_TRACKS]; // TODO - use list

  static weed_timecode_t last_tc;
  static weed_timecode_t fill_tc;
  static weed_plant_t *event;
  static int nfiles;

  static int *from_files = NULL;
  static double *aseeks = NULL, *avels = NULL;

  boolean is_cont = FALSE;
  if (abuf == NULL) return;

  abuf->samples_filled = 0; // write fill level of buffer
  abuf->start_sample = 0; // read level

  if (st_event != NULL) {
    // this is only called for the first buffered read
    event = st_event;
    last_tc = get_event_timecode(event);

    lives_freep((void **)&from_files);
    lives_freep((void **)&avels);
    lives_freep((void **)&aseeks);

    if (mainw->multitrack != NULL && mainw->multitrack->avol_init_event != NULL)
      nfiles = weed_leaf_num_elements(mainw->multitrack->avol_init_event, WEED_LEAF_IN_TRACKS);

    else nfiles = 1;

    from_files = (int *)lives_calloc(nfiles, sizint);
    avels = (double *)lives_calloc(nfiles, sizdbl);
    aseeks = (double *)lives_calloc(nfiles, sizdbl);

    for (i = 0; i < nfiles; i++) {
      from_files[i] = 0;
      avels[i] = aseeks[i] = 0.;
    }

    // TODO - actually what we should do here is get the audio state for
    // the *last* frame in the buffer and then adjust the seeks back to the
    // beginning of the buffer, in case an audio track starts during the
    // buffering period. The current way is fine for a preview, but when we
    // implement rendering of *partial* event lists we will need to do this

    // a negative seek value would mean that we need to pad silence at the
    // start of the track buffer

    if (event != get_first_event(event_list))
      atstate = get_audio_and_effects_state_at(event_list, event, TRUE, exact);

    // process audio updates at this frame
    else atstate = aframe_to_atstate(event);

    mainw->audio_event = event;

    if (atstate != NULL) {
      for (nnfiles = 0; atstate[nnfiles].afile != -1; nnfiles++);

      for (i = 0; i < nnfiles; i++) {
        if (atstate[i].afile > 0) {
          from_files[i] = atstate[i].afile;
          avels[i] = atstate[i].vel;
          aseeks[i] = atstate[i].seek;
        }
      }

      lives_free(atstate);
    }
  } else {
    is_cont = TRUE;
  }

  if (mainw->multitrack != NULL) {
    // get channel volumes from the mixer
    for (i = 0; i < nfiles; i++) {
      if (mainw->multitrack != NULL && mainw->multitrack->audio_vols != NULL) {
        chvols[i] = (double)LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->multitrack->audio_vols, i)) / 1000000.;
      }
    }
  } else chvols[0] = 1.;

  fill_tc = last_tc + (double)(abuf->samp_space) / (double)abuf->arate * TICKS_PER_SECOND_DBL;

  // continue until either we have a full buffer, or we reach next audio frame
  while (event != NULL && get_event_timecode(event) <= fill_tc) {
    if (!is_cont) event = get_next_frame_event(event);
    if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
      // got next audio frame
      weed_timecode_t tc = get_event_timecode(event);
      if (tc >= fill_tc) break;

      tc += (TICKS_PER_SECOND_DBL / cfile->fps * !is_blank_frame(event, FALSE));

      mainw->read_failed = FALSE;
      lives_freep((void **)&mainw->read_failed_file);

      render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, tc, chvols, 1., 1., abuf);

      if (mainw->read_failed) {
        do_read_failed_error_s(mainw->read_failed_file, NULL);
      }

      for (i = 0; i < nfiles; i++) {
        // increase seek values
        aseeks[i] += avels[i] * (tc - last_tc) / TICKS_PER_SECOND_DBL;
      }

      last_tc = tc;

      // process audio updates at this frame
      atstate = aframe_to_atstate(event);

      if (atstate != NULL) {
        for (nnfiles = 0; atstate[nnfiles].afile != -1; nnfiles++);

        for (i = 0; i < nnfiles; i++) {
          if (atstate[i].afile > 0) {
            from_files[i] = atstate[i].afile;
            avels[i] = atstate[i].vel;
            aseeks[i] = atstate[i].seek;
          }
        }
        lives_free(atstate);
      }
    }
    is_cont = FALSE;
  }

  if (last_tc < fill_tc) {
    // flush the rest of the audio

    mainw->read_failed = FALSE;
    lives_freep((void **)&mainw->read_failed_file);

    render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, fill_tc, chvols, 1., 1., abuf);
    for (i = 0; i < nfiles; i++) {
      // increase seek values
      aseeks[i] += avels[i] * (fill_tc - last_tc) / TICKS_PER_SECOND_DBL;
    }
  }

  if (mainw->read_failed) {
    do_read_failed_error_s(mainw->read_failed_file, NULL);
  }

  mainw->write_abuf++;
  if (mainw->write_abuf >= prefs->num_rtaudiobufs) mainw->write_abuf = 0;

  last_tc = fill_tc;

  if (mainw->abufs_to_fill > 0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    mainw->abufs_to_fill--;
    pthread_mutex_unlock(&mainw->abuf_mutex);
  }
}


void init_jack_audio_buffers(int achans, int arate, boolean exact) {
#ifdef ENABLE_JACK

  int i, chan;

  mainw->jackd->abufs = (lives_audio_buf_t **)lives_calloc(prefs->num_rtaudiobufs, sizeof(lives_audio_buf_t *));

  for (i = 0; i < prefs->num_rtaudiobufs; i++) {
    mainw->jackd->abufs[i] = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));

    mainw->jackd->abufs[i]->out_achans = achans;
    mainw->jackd->abufs[i]->arate = arate;
    mainw->jackd->abufs[i]->samp_space = XSAMPLES / prefs->num_rtaudiobufs;
    mainw->jackd->abufs[i]->bufferf = (float **)lives_calloc(achans, sizeof(float *));
    for (chan = 0; chan < achans; chan++) {
      mainw->jackd->abufs[i]->bufferf[chan] = (float *)lives_calloc_safety(XSAMPLES / prefs->num_rtaudiobufs, sizeof(float));
    }
  }
#endif
}


void init_pulse_audio_buffers(int achans, int arate, boolean exact) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  mainw->pulsed->abufs = (lives_audio_buf_t **)lives_calloc(prefs->num_rtaudiobufs, sizeof(lives_audio_buf_t *));

  for (i = 0; i < prefs->num_rtaudiobufs; i++) {
    mainw->pulsed->abufs[i] = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));

    mainw->pulsed->abufs[i]->out_achans = achans;
    mainw->pulsed->abufs[i]->arate = arate;
    mainw->pulsed->abufs[i]->start_sample = 0;
    mainw->pulsed->abufs[i]->samp_space = XSAMPLES / prefs->num_rtaudiobufs; // samp_space here is in stereo samples
    mainw->pulsed->abufs[i]->buffer16 = (short **)lives_calloc(1, sizeof(short *));
    mainw->pulsed->abufs[i]->buffer16[0] = (short *)lives_calloc_safety(XSAMPLES / prefs->num_rtaudiobufs , achans * sizeof(short));
  }
#endif
}


void free_jack_audio_buffers(void) {
#ifdef ENABLE_JACK

  int i, chan;

  if (mainw->jackd == NULL) return;

  if (mainw->jackd->abufs == NULL) return;

  for (i = 0; i < prefs->num_rtaudiobufs; i++) {
    if (mainw->jackd->abufs[i] != NULL) {
      for (chan = 0; chan < mainw->jackd->abufs[i]->out_achans; chan++) {
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

  if (mainw->pulsed == NULL) return;

  if (mainw->pulsed->abufs == NULL) return;

  for (i = 0; i < prefs->num_rtaudiobufs; i++) {
    if (mainw->pulsed->abufs[i] != NULL) {
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

  if (!(prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) return FALSE;

  // if recording external audio, we are intrinsically in sync
  if (mainw->record && prefs->audio_src == AUDIO_SRC_EXT) return TRUE;

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) {
    if (!mainw->is_rendering) {
      if (mainw->jackd->playing_file != -1 && !jack_audio_seek_frame(mainw->jackd, frameno)) {
        if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd, frameno);
      }
      if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && prefs->audio_src == AUDIO_SRC_INT) {
        jack_get_rec_avals(mainw->jackd);
      }
    }

    return TRUE;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL) {
    if (!mainw->is_rendering) {
      if (mainw->pulsed->playing_file != -1 && !pulse_audio_seek_frame(mainw->pulsed, frameno)) {
        if (pulse_try_reconnect()) pulse_audio_seek_frame(mainw->pulsed, frameno);
      }
      if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && prefs->audio_src == AUDIO_SRC_INT) {
        pulse_get_rec_avals(mainw->pulsed);
      }
    }
    return TRUE;
  }
#endif
  return FALSE;
}


//////////////////////////////////////////////////////////////////////////
static lives_audio_buf_t *cache_buffer = NULL;
static pthread_t athread;

static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *cache_my_audio(void *arg) {
  // run as a thread (from audio_cache_init())
  //
  // read audio from file into cache
  // must be done in real time since other threads may be waiting on the cache

  // currently only jack audio player uses this during playback

  // currently the output is always in the s16 buffer, resampling is done

  // TODO: should also serve requests from fill_abuffer_from

  lives_audio_buf_t *cbuffer = (lives_audio_buf_t *)arg;
  char *filename;
  int rc = 0;
  register int i;

  if (mainw->multitrack == NULL)
    cbuffer->is_ready = TRUE;

  while (!cbuffer->die) {
    // wait for request from client (setting cbuffer->is_ready or cbuffer->die)
    pthread_mutex_lock(&cond_mutex);
    rc = pthread_cond_wait(&cond, &cond_mutex);
    pthread_mutex_unlock(&cond_mutex);

    if (cbuffer->die) {
      if (mainw->multitrack != NULL && cbuffer->_fd != -1) lives_close_buffered(cbuffer->_fd);
      return cbuffer;
    }

    // read from file and process data
    //lives_printerr("got buffer request !\n");

#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL && mainw->abufs_to_fill > 0) {
      mainw->jackd->abufs[mainw->write_abuf]->samples_filled = 0;
      fill_abuffer_from(mainw->jackd->abufs[mainw->write_abuf], mainw->event_list, NULL, FALSE);
      continue;
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL && mainw->abufs_to_fill > 0) {
      mainw->pulsed->abufs[mainw->write_abuf]->samples_filled = 0;
      fill_abuffer_from(mainw->pulsed->abufs[mainw->write_abuf], mainw->event_list, NULL, FALSE);
      continue;
    }
#endif

    if (cbuffer->operation != LIVES_READ_OPERATION) {
      cbuffer->is_ready = TRUE;
      continue;
    }

    cbuffer->eof = FALSE;

    // TODO - if out_asamps changed, we need to free all buffers and set _cachans==0
    if (cbuffer->out_asamps != cbuffer->_casamps) {
      if (cbuffer->bufferf != NULL) {
        // free float channels
        for (i = 0; i < (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i++) {
          lives_free(cbuffer->bufferf[i]);
        }
        lives_free(cbuffer->bufferf);
        cbuffer->bufferf = NULL;
      }

      if (cbuffer->buffer16 != NULL) {
        // free 16bit channels
        for (i = 0; i < (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i++) {
          lives_free(cbuffer->buffer16[i]);
        }
        lives_free(cbuffer->buffer16);
        cbuffer->buffer16 = NULL;
      }

      cbuffer->_cachans = 0;
      cbuffer->_cout_interleaf = FALSE;
      cbuffer->_csamp_space = 0;
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
      if (cbuffer->bufferf != NULL) {
        // free float channels
        for (i = 0; i < (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i++) {
          lives_free(cbuffer->bufferf[i]);
        }
        lives_free(cbuffer->bufferf);
        cbuffer->bufferf = NULL;
      }

      if ((cbuffer->out_interleaf ? 1 : cbuffer->out_achans) != (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)
          || (cbuffer->samp_space / (cbuffer->out_interleaf ? 1 : cbuffer->out_achans) !=
              (cbuffer->_csamp_space / (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)))) {
        // channels or samp_space changed

        if ((cbuffer->out_interleaf ? 1 : cbuffer->out_achans) > (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)) {
          // ouput channels increased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16, (cbuffer->out_interleaf ? 1 : cbuffer->out_achans) * sizeof(short *));
          for (i = (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i < (cbuffer->out_interleaf ? 1 : cbuffer->out_achans); i++) {
            cbuffer->buffer16[i] = NULL;
          }
        }

        for (i = 0; i < (cbuffer->out_interleaf ? 1 : cbuffer->out_achans); i++) {
          // realloc existing channels and add new ones
          cbuffer->buffer16[i] = (short *)lives_realloc(cbuffer->buffer16[i], cbuffer->samp_space * sizeof(short) *
                                 (cbuffer->out_interleaf ? cbuffer->out_achans : 1) + EXTRA_BYTES);
        }

        // free any excess channels

        for (i = (cbuffer->out_interleaf ? 1 : cbuffer->out_achans); i < (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i++) {
          lives_free(cbuffer->buffer16[i]);
        }

        if ((cbuffer->out_interleaf ? 1 : cbuffer->out_achans) < (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)) {
          // output channels decreased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16, (cbuffer->out_interleaf ? 1 : cbuffer->out_achans) * sizeof(short *));
        }
      }

      break;
    case -32:
      // we need 16 bit buffer(s) and float buffer(s)

      // 16 bit buffers follow in out_achans but in_interleaf...

      if ((cbuffer->in_interleaf ? 1 : cbuffer->out_achans) != (cbuffer->_cin_interleaf ? 1 : cbuffer->_cachans)
          || (cbuffer->samp_space / (cbuffer->in_interleaf ? 1 : cbuffer->out_achans) !=
              (cbuffer->_csamp_space / (cbuffer->_cin_interleaf ? 1 : cbuffer->_cachans)))) {
        // channels or samp_space changed

        if ((cbuffer->in_interleaf ? 1 : cbuffer->out_achans) > (cbuffer->_cin_interleaf ? 1 : cbuffer->_cachans)) {
          // ouput channels increased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16, (cbuffer->in_interleaf ? 1 : cbuffer->out_achans) * sizeof(short *));
          for (i = (cbuffer->_cin_interleaf ? 1 : cbuffer->_cachans); i < (cbuffer->in_interleaf ? 1 : cbuffer->out_achans); i++) {
            cbuffer->buffer16[i] = NULL;
          }
        }

        for (i = 0; i < (cbuffer->in_interleaf ? 1 : cbuffer->out_achans); i++) {
          // realloc existing channels and add new ones
          cbuffer->buffer16[i] = (short *)lives_realloc(cbuffer->buffer16[i], cbuffer->samp_space * sizeof(short) *
                                 (cbuffer->in_interleaf ? cbuffer->out_achans : 1) + EXTRA_BYTES);
        }

        // free any excess channels

        for (i = (cbuffer->in_interleaf ? 1 : cbuffer->out_achans); i < (cbuffer->_cin_interleaf ? 1 : cbuffer->_cachans); i++) {
          lives_free(cbuffer->buffer16[i]);
        }

        if ((cbuffer->in_interleaf ? 1 : cbuffer->out_achans) < (cbuffer->_cin_interleaf ? 1 : cbuffer->_cachans)) {
          // output channels decreased
          cbuffer->buffer16 = (short **)
                              lives_realloc(cbuffer->buffer16, (cbuffer->in_interleaf ? 1 : cbuffer->out_achans) * sizeof(short *));
        }
      }

      if ((cbuffer->out_interleaf ? 1 : cbuffer->out_achans) != (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)
          || (cbuffer->samp_space / (cbuffer->out_interleaf ? 1 : cbuffer->out_achans) !=
              (cbuffer->_csamp_space / (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)))) {
        // channels or samp_space changed

        if ((cbuffer->out_interleaf ? 1 : cbuffer->out_achans) > (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)) {
          // ouput channels increased
          cbuffer->bufferf = (float **)
                             lives_realloc(cbuffer->bufferf, (cbuffer->out_interleaf ? 1 : cbuffer->out_achans) * sizeof(float *));
          for (i = (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i < (cbuffer->out_interleaf ? 1 : cbuffer->out_achans); i++) {
            cbuffer->bufferf[i] = NULL;
          }
        }

        for (i = 0; i < (cbuffer->out_interleaf ? 1 : cbuffer->out_achans); i++) {
          // realloc existing channels and add new ones
          cbuffer->bufferf[i] = (float *)lives_realloc(cbuffer->bufferf[i], cbuffer->samp_space * sizeof(float) *
                                (cbuffer->out_interleaf ? cbuffer->out_achans : 1) + EXTRA_BYTES);
        }

        // free any excess channels

        for (i = (cbuffer->out_interleaf ? 1 : cbuffer->out_achans); i < (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans); i++) {
          lives_free(cbuffer->bufferf[i]);
        }

        if ((cbuffer->out_interleaf ? 1 : cbuffer->out_achans) > (cbuffer->_cout_interleaf ? 1 : cbuffer->_cachans)) {
          // output channels decreased
          cbuffer->bufferf = (float **)
                             lives_realloc(cbuffer->bufferf, (cbuffer->out_interleaf ? 1 : cbuffer->out_achans) * sizeof(float *));
        }
      }

      break;
    default:
      break;
    }

    // update _cinterleaf, etc.
    cbuffer->_cin_interleaf = cbuffer->in_interleaf;
    cbuffer->_cout_interleaf = cbuffer->out_interleaf;
    cbuffer->_csamp_space = cbuffer->samp_space;
    cbuffer->_cachans = cbuffer->out_achans;
    cbuffer->_casamps = cbuffer->out_asamps;

    // open new file if necessary

    if (cbuffer->fileno != cbuffer->_cfileno) {
      lives_clip_t *afile = mainw->files[cbuffer->fileno];

      if (cbuffer->_fd != -1) lives_close_buffered(cbuffer->_fd);

      if (afile->opening)
        filename = lives_strdup_printf("%s/%s/audiodump.pcm", prefs->workdir, mainw->files[cbuffer->fileno]->handle);
      else filename = lives_strdup_printf("%s/%s/audio", prefs->workdir, mainw->files[cbuffer->fileno]->handle);

      cbuffer->_fd = lives_open_buffered_rdonly(filename);
      if (cbuffer->_fd == -1) {
        lives_printerr("audio cache thread: error opening %s\n", filename);
        cbuffer->in_achans = 0;
        cbuffer->fileno = -1; ///< let client handle this
        cbuffer->is_ready = TRUE;
        continue;
      }

      lives_free(filename);
    }

    if (cbuffer->fileno != cbuffer->_cfileno || cbuffer->seek != cbuffer->_cseek) {
#ifdef HAVE_POSIX_FADVISE
      if (cbuffer->sequential) {
        posix_fadvise(cbuffer->_fd, cbuffer->seek, 0, POSIX_FADV_SEQUENTIAL);
      }
#endif
      lives_lseek_buffered_rdonly_absolute(cbuffer->_fd, cbuffer->seek);
    }

    cbuffer->_cfileno = cbuffer->fileno;

    // prepare file read buffer

    if (cbuffer->bytesize != cbuffer->_cbytesize) {
      cbuffer->_filebuffer = (uint8_t *)lives_realloc(cbuffer->_filebuffer, cbuffer->bytesize);

      if (cbuffer->_filebuffer == NULL) {
        cbuffer->_cbytesize = cbuffer->bytesize = 0;
        cbuffer->in_achans = 0;
        cbuffer->is_ready = TRUE;
        continue;
      }
    }

    // read from file
    cbuffer->_cbytesize = lives_read_buffered(cbuffer->_fd, cbuffer->_filebuffer, cbuffer->bytesize, TRUE);

    if (cbuffer->_cbytesize < 0) {
      // there is not much we can do if we get a read error, since we are running in a realtime thread here
      // just mark it as 0 channels, 0 bytes
      cbuffer->bytesize = cbuffer->_cbytesize = 0;
      cbuffer->in_achans = 0;
      cbuffer->is_ready = TRUE;
      continue;
    }

    if (cbuffer->_cbytesize < cbuffer->bytesize) {
      cbuffer->eof = TRUE;
      cbuffer->_csamp_space = (int64_t)((double)cbuffer->samp_space / (double)cbuffer->bytesize * (double)cbuffer->_cbytesize);
      cbuffer->samp_space = cbuffer->_csamp_space;
    }

    cbuffer->bytesize = cbuffer->_cbytesize;
    cbuffer->_cseek = (cbuffer->seek += cbuffer->bytesize);

    // do conversion

    // convert from 8 bit to 16 bit and mono to stereo if necessary
    // resample as we go
    if (cbuffer->in_asamps == 8) {
      // TODO - error on non-interleaved
      sample_move_d8_d16(cbuffer->buffer16[0], (uint8_t *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize,
                         cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans, 0);
    }
    // 16 bit input samples
    // resample as we go
    else {
      sample_move_d16_d16(cbuffer->buffer16[0], (short *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize,
                          cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans,
                          cbuffer->swap_endian ? SWAP_X_TO_L : 0, 0);
    }

    // if our out_asamps is 16, we are done

    cbuffer->is_ready = TRUE;
  }
  return cbuffer;
}


void wake_audio_thread(void) {
  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);
}


lives_audio_buf_t *audio_cache_init(void) {
  cache_buffer = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));
  cache_buffer->is_ready = FALSE;
  cache_buffer->die = FALSE;

  if (mainw->multitrack == NULL) {
    cache_buffer->in_achans = 0;

    // NULLify all pointers of cache_buffer

    cache_buffer->buffer8 = NULL;
    cache_buffer->buffer16 = NULL;
    cache_buffer->buffer24 = NULL;
    cache_buffer->buffer32 = NULL;
    cache_buffer->bufferf = NULL;
    cache_buffer->_filebuffer = NULL;
    cache_buffer->_cbytesize = 0;
    cache_buffer->_csamp_space = 0;
    cache_buffer->_cachans = 0;
    cache_buffer->_casamps = 0;
    cache_buffer->_cout_interleaf = FALSE;
    cache_buffer->_cin_interleaf = FALSE;
    cache_buffer->eof = FALSE;
    cache_buffer->sequential = FALSE;

    cache_buffer->_cfileno = -1;
    cache_buffer->_cseek = -1;
    cache_buffer->_fd = -1;
  }

  // init the audio caching thread for rt playback
  pthread_create(&athread, NULL, cache_my_audio, cache_buffer);

  return cache_buffer;
}


void audio_cache_end(void) {
  lives_audio_buf_t *xcache_buffer;
  register int i;

  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  cache_buffer->die = TRUE; ///< tell cache thread to exit when possible
  wake_audio_thread();
  pthread_join(athread, NULL);
  pthread_mutex_unlock(&mainw->cache_buffer_mutex);

  if (mainw->event_list == NULL) {
    // free all buffers

    for (i = 0; i < (cache_buffer->_cin_interleaf ? 1 : cache_buffer->_cachans); i++) {
      if (cache_buffer->buffer8 != NULL && cache_buffer->buffer8[i] != NULL) lives_free(cache_buffer->buffer8[i]);
      if (cache_buffer->buffer16 != NULL && cache_buffer->buffer16[i] != NULL) lives_free(cache_buffer->buffer16[i]);
      if (cache_buffer->buffer24 != NULL && cache_buffer->buffer24[i] != NULL) lives_free(cache_buffer->buffer24[i]);
      if (cache_buffer->buffer32 != NULL && cache_buffer->buffer32[i] != NULL) lives_free(cache_buffer->buffer32[i]);
      if (cache_buffer->bufferf != NULL && cache_buffer->bufferf[i] != NULL) lives_free(cache_buffer->bufferf[i]);
    }

    if (cache_buffer->buffer8 != NULL) lives_free(cache_buffer->buffer8);
    if (cache_buffer->buffer16 != NULL) lives_free(cache_buffer->buffer16);
    if (cache_buffer->buffer24 != NULL) lives_free(cache_buffer->buffer24);
    if (cache_buffer->buffer32 != NULL) lives_free(cache_buffer->buffer32);
    if (cache_buffer->bufferf != NULL) lives_free(cache_buffer->bufferf);

    if (cache_buffer->_filebuffer != NULL) lives_free(cache_buffer->_filebuffer);
  }
  // make this threadsafe (kind of)
  xcache_buffer = cache_buffer;
  cache_buffer = NULL;
  lives_free(xcache_buffer);
}


lives_audio_buf_t *audio_cache_get_buffer(void) {
  return cache_buffer;
}


///////////////////////////////////////

// plugin handling

boolean get_audio_from_plugin(float **fbuffer, int nchans, int arate, int nsamps, boolean is_audio_thread) {
  // get audio from an audio generator; fbuffer is filled with non-interleaved float
  weed_plant_t *inst = rte_keymode_get_instance(mainw->agen_key, rte_key_getmode(mainw->agen_key));
  weed_plant_t *orig_inst = inst;
  weed_plant_t *filter;
  weed_plant_t *channel;
  weed_plant_t *ctmpl;
  weed_process_f process_func;
  weed_timecode_t tc;
  weed_error_t retval;
  int flags, cflags;
  int xnchans = 0, xchans = 0, xrate = 0, xarate;
  boolean rvary = FALSE, lvary = FALSE;

  if (mainw->agen_needs_reinit) {
    weed_instance_unref(inst);
    return FALSE; // wait for other thread to reinit us
  }
  tc = (double)mainw->agen_samps_count / (double)arate * TICKS_PER_SECOND_DBL;
  filter = weed_instance_get_filter(inst, FALSE);
  flags = weed_filter_get_flags(filter);

  if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
  if (flags & WEED_FILTER_AUDIO_LAYOUTS_MAY_VARY) lvary = TRUE;

getaud1:

  channel = get_enabled_channel(inst, 0, FALSE);
  if (channel != NULL) {
    xnchans = nchans; // preferred value
    ctmpl = weed_channel_get_template(channel);
    cflags = weed_chantmpl_get_flags(ctmpl);
    if (lvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_MINCHANS))
      xnchans = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_MINCHANS, NULL);
    else if (weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_MINCHANS))
      xnchans = weed_get_int_value(filter, WEED_LEAF_AUDIO_MINCHANS, NULL);
    if (xnchans > nchans) {
      weed_instance_unref(orig_inst);
      return FALSE;
    }
    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, NULL) != nchans
        && (cflags & WEED_CHANNEL_REINIT_ON_AUDIO_LAYOUT_CHANGE))
      mainw->agen_needs_reinit = TRUE;
    else weed_set_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, nchans);

    xrate = arate;
    if (rvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_RATE))
      xrate = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_RATE, NULL);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_AUDIO_RATE))
      xrate = weed_get_int_value(filter, WEED_LEAF_AUDIO_RATE, NULL);
    if (arate != xrate) {
      weed_instance_unref(orig_inst);
      return FALSE;
    }

    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_RATE, NULL) != arate) {
      if (cflags & WEED_CHANNEL_REINIT_ON_AUDIO_RATE_CHANGE) {
        mainw->agen_needs_reinit = TRUE;
      }
    }

    weed_set_int_value(channel, WEED_LEAF_AUDIO_RATE, arate);

    if (mainw->agen_needs_reinit) {
      // allow main thread to complete the reinit so we do not delay; just return silence
      weed_instance_unref(orig_inst);
      return FALSE;
    }

    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);

    weed_channel_set_audio_data(channel, fbuffer, arate, xnchans, nsamps);
    weed_set_double_value(inst, WEED_LEAF_FPS, cfile->pb_fps);
  }

  if (mainw->pconx != NULL && !(mainw->preview || mainw->is_rendering)) {
    // chain any data pipelines
    if (!pthread_mutex_trylock(&mainw->fx_mutex[mainw->agen_key - 1])) {
      mainw->agen_needs_reinit = pconx_chain_data(mainw->agen_key - 1, rte_key_getmode(mainw->agen_key), is_audio_thread);
      filter_mutex_unlock(mainw->agen_key - 1);

      if (mainw->agen_needs_reinit) {
        // allow main thread to complete the reinit so we do not delay; just return silence
        weed_instance_unref(orig_inst);
        return FALSE;
      }
    }
  }

  retval = run_process_func(inst, tc, mainw->agen_key);

  if (retval != WEED_SUCCESS) {
    if (retval == WEED_ERROR_REINIT_NEEDED) mainw->agen_needs_reinit = TRUE;
    weed_instance_unref(orig_inst);
    return FALSE;
  }

  if (channel != NULL && xnchans == 1 && nchans == 2) {
    // if we got mono but we wanted stereo, copy to right channel
    lives_memcpy(fbuffer[1], fbuffer[0], nsamps * sizeof(float));
  }

  if (weed_plant_has_leaf(inst, WEED_LEAF_HOST_NEXT_INSTANCE)) {
    // handle compound fx
    inst = weed_get_plantptr_value(inst, WEED_LEAF_HOST_NEXT_INSTANCE, NULL);
    goto getaud1;
  }

  mainw->agen_samps_count += nsamps;

  weed_instance_unref(orig_inst);
  return TRUE;
}


void reinit_audio_gen(void) {
  int agen_key = mainw->agen_key;
  int ret;

  weed_plant_t *inst = rte_keymode_get_instance(agen_key, rte_key_getmode(mainw->agen_key));

  ret = weed_reinit_effect(inst, TRUE);
  if (ret == FILTER_SUCCESS || ret == FILTER_INFO_REINITED) {
    mainw->agen_needs_reinit = FALSE;
    mainw->agen_key = agen_key;
  }
  weed_instance_unref(inst);
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
    mainw->com_failed = FALSE;
    com = lives_strdup_printf("%s backup_audio %s", prefs->backend_sync, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      d_print_failed();
      return FALSE;
    }
  }

  audio_pos = (double)((cfile->start - 1) * cfile->arate * cfile->achans * cfile->asampsize / 8) / cfile->fps;
  audio_file = lives_get_audio_file_name(mainw->current_file);

  audio_fd = lives_open3(audio_file, O_RDWR | O_CREAT, DEF_FILE_PERMS);

  if (audio_fd == -1) return FALSE;

  if (audio_pos > cfile->afilesize) {
    off64_t audio_end_pos = (double)((cfile->start - 1) * cfile->arate * cfile->achans * cfile->asampsize / 8) / cfile->fps;
    pad_with_silence(audio_fd, audio_pos, audio_end_pos, cfile->asampsize, cfile->signed_endian & AFORM_UNSIGNED,
                     cfile->signed_endian & AFORM_BIG_ENDIAN);
  } else lseek64(audio_fd, audio_pos, SEEK_SET);

  aud_tc = 0;

  return TRUE;
}


void apply_rte_audio_end(boolean del) {
  close(audio_fd);
  if (del) lives_rm(audio_file);
  lives_free(audio_file);
}


boolean apply_rte_audio(int nframes) {
  // CALLED When we are rendering audio to a file

  // - read nframes from clip or generator
  // - convert to float if necessary
  // - send to rte audio effects
  // - convert back to s16 or s8
  // - save to audio_fd
  weed_plant_t *layer;
  size_t tbytes;
  uint8_t *in_buff;
  float **fltbuf, *fltbufni = NULL;
  short *shortbuf = NULL;
  boolean rev_endian = FALSE;

  register int i;

  int abigendian = cfile->signed_endian & AFORM_BIG_ENDIAN;
  int onframes;

  // read nframes of audio from clip or generator

  if ((abigendian && capable->byte_order == LIVES_LITTLE_ENDIAN) || (!abigendian &&
      capable->byte_order == LIVES_BIG_ENDIAN)) rev_endian = TRUE;

  tbytes = nframes * cfile->achans * cfile->asampsize / 8;

  if (mainw->agen_key == 0) {
    if (tbytes + audio_pos > cfile->afilesize) tbytes = cfile->afilesize - audio_pos;
    if (tbytes <= 0) return TRUE;
    nframes = tbytes / cfile->achans / (cfile->asampsize / 8);
  }

  onframes = nframes;

  in_buff = (uint8_t *)lives_calloc_safety(tbytes, 1);
  if (in_buff == NULL) return FALSE;

  if (cfile->asampsize == 8) {
    shortbuf = (short *)lives_calloc_safety(tbytes / sizeof(short), sizeof(short));
    if (shortbuf == NULL) {
      lives_free(in_buff);
      return FALSE;
    }
  }

  fltbuf = (float **)lives_calloc(cfile->achans, sizeof(float *));

  if (mainw->agen_key == 0) {
    // read from audio_fd

    mainw->read_failed = FALSE;

    tbytes = lives_read(audio_fd, in_buff, tbytes, FALSE);

    if (mainw->read_failed) {
      do_read_failed_error_s(audio_file, NULL);
      lives_freep((void **)&mainw->read_failed_file);
      lives_free(fltbuf);
      lives_free(in_buff);
      lives_freep((void **)&shortbuf);
      return FALSE;
    }

    if (cfile->asampsize == 8) {
      sample_move_d8_d16(shortbuf, in_buff, nframes, tbytes,
                         1.0, cfile->achans, cfile->achans, 0);
    } else shortbuf = (short *)in_buff;

    nframes = tbytes / cfile->achans / (cfile->asampsize / 8);

    // convert to float

    for (i = 0; i < cfile->achans; i++) {
      // convert s16 to non-interleaved float
      fltbuf[i] = (float *)lives_calloc(nframes, sizeof(float));
      if (fltbuf[i] == NULL) {
        for (--i; i >= 0; i--) {
          lives_free(fltbuf[i]);
        }
        lives_free(fltbuf);
        if (shortbuf != (short *)in_buff) lives_free(shortbuf);
        lives_free(in_buff);
        return FALSE;
      }
      lives_memset(fltbuf[i], 0, nframes * sizeof(float));
      if (nframes > 0) sample_move_d16_float(fltbuf[i], shortbuf + i, nframes, cfile->achans, \
                                               (cfile->signed_endian & AFORM_UNSIGNED), rev_endian,
                                               1.0);
    }

  } else {
    // read from plugin. This should already be float.
    get_audio_from_plugin(fltbuf, cfile->achans, cfile->arate, nframes, FALSE);
  }

  // apply any audio effects

  aud_tc += (double)onframes / (double)cfile->arate * TICKS_PER_SECOND_DBL;
  // apply any audio effects with in_channels

  layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
  weed_layer_set_audio_data(layer, fltbuf, cfile->arate, cfile->achans, onframes);
  weed_apply_audio_effects_rt(layer, aud_tc, FALSE, FALSE);
  lives_free(fltbuf);
  fltbuf = weed_layer_get_audio_data(layer, NULL);
  weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
  weed_layer_free(layer);

  if (!(has_audio_filters(AF_TYPE_NONA) || mainw->agen_key != 0)) {
    // analysers only - no need to save (just render as normal)
    // or, audio is being generated (we rendered it to ascrap file)

    audio_pos += tbytes;

    if (fltbufni == NULL) {
      for (i = 0; i < cfile->achans; i++) {
        lives_free(fltbuf[i]);
      }
    } else lives_free(fltbufni);

    lives_free(fltbuf);

    if (shortbuf != (short *)in_buff) lives_free(shortbuf);
    lives_free(in_buff);

    return TRUE;
  }

  // convert float audio back to int
  sample_move_float_int(in_buff, fltbuf, onframes, 1.0, cfile->achans, cfile->asampsize, (cfile->signed_endian & AFORM_UNSIGNED),
                        !(cfile->signed_endian & AFORM_BIG_ENDIAN), FALSE, 1.0);

  if (fltbufni == NULL) {
    for (i = 0; i < cfile->achans; i++) {
      lives_free(fltbuf[i]);
    }
  } else lives_free(fltbufni);

  lives_free(fltbuf);

  if (audio_fd >= 0) {
    // save to file
    mainw->write_failed = FALSE;
    lseek64(audio_fd, audio_pos, SEEK_SET);
    tbytes = onframes * cfile->achans * cfile->asampsize / 8;
    lives_write(audio_fd, in_buff, tbytes, FALSE);
    audio_pos += tbytes;
  }

  if (shortbuf != (short *)in_buff) lives_free(shortbuf);
  lives_free(in_buff);

  if (mainw->write_failed) {
    do_write_failed_error_s(audio_file, NULL);
    return FALSE;
  }

  return TRUE;
}


/**
  @brief fill the audio channel(s) for effects with mixed audio / video

  push audio from abuf into an audio channel
  audio will be formatted to the channel requested format

  when we have audio effects running, the buffer is constantly fiiled from the audio thread (we have two buffers so that
  we dont hang the player during read). We keep track of how many audio clients (filters) require audio and
  only when all reads have been satisfied we swap the read and write buffers (with a mutex lock / unlock)

  if player is jack, we will have non-interleaved float, if player is pulse, we will have interleaved S16 so we have to convert to float and
  then back again.

  (this is currently only used to push audio to generators, since there are currently no filters which allow both video and audio input)

  (Filters which are purely audio are run from the audio thread, since the need to return audio back to the player.
  This is less than optimal, and likely in the future, a separate thread will be used  to run the audio filters).

 */
boolean push_audio_to_channel(weed_plant_t *filter, weed_plant_t *achan, lives_audio_buf_t *abuf) {
  float **dst, *src;

  weed_plant_t *ctmpl;

  float scale;

  size_t samps;

  boolean rvary = FALSE, lvary = FALSE;
  int trate, tchans, flags;
  int alen;

  register int i;
  if (abuf->samples_filled == 0) {
    weed_layer_set_audio_data(achan, NULL, 0, 0, 0);
    return FALSE;
  }

  ctmpl = weed_get_plantptr_value(achan, WEED_LEAF_TEMPLATE, NULL);

  flags = weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
  if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
  if (flags & WEED_FILTER_AUDIO_LAYOUTS_MAY_VARY) lvary = TRUE;

  // TODO: can be list
  if (rvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_RATE))
    trate = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_RATE, NULL);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_AUDIO_RATE))
    trate = weed_get_int_value(filter, WEED_LEAF_AUDIO_RATE, NULL);
  else trate = DEFAULT_AUDIO_RATE;

  if (lvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_MINCHANS))
    tchans = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_CHANNELS, NULL);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_AUDIO_MINCHANS))
    tchans = weed_get_int_value(filter, WEED_LEAF_AUDIO_MINCHANS, NULL);
  else tchans = DEFAULT_AUDIO_CHANS;

#ifdef DEBUG_AFB
  g_print("push from afb %d\n", abuf->samples_filled);
#endif

  // plugin will get float, so we first convert to that
  if (abuf->bufferf == NULL) {

    // try 8 bit -> 16
    if (abuf->buffer8 != NULL && abuf->buffer16 == NULL) {
      int swap = 0;
      if (!abuf->s8_signed) swap = SWAP_U_TO_S;
      abuf->s16_signed = TRUE;
      abuf->buffer16 = (short **)lives_calloc(abuf->out_achans, sizeof(short *));
      for (i = 0; i < abuf->out_achans; i++) {
        abuf->buffer16[i] = (short *)lives_calloc_safety(abuf->samples_filled, sizeof(short));
        sample_move_d8_d16(abuf->buffer16[i], abuf->buffer8[i], abuf->samples_filled, abuf->samples_filled * sizeof(short),
                           1.0, abuf->out_achans, abuf->out_achans, swap);

      }
    }

    // try convert S16 -> float
    if (abuf->buffer16 != NULL) {
      size_t offs = 0;
      abuf->bufferf = (float **)lives_calloc(abuf->out_achans, sizeof(float *));
      for (i = 0; i < abuf->out_achans; i++) {
        if (!abuf->in_interleaf) {
          abuf->bufferf[i] = (float *)lives_calloc_safety(abuf->samples_filled, sizeof(float));
          sample_move_d16_float(abuf->bufferf[i], &abuf->buffer16[0][offs], abuf->samples_filled, 1,
                                (abuf->s16_signed ? AFORM_SIGNED : AFORM_UNSIGNED), abuf->swap_endian, 1.0);
          offs += abuf->samples_filled;
        } else {
          abuf->bufferf[i] = (float *)lives_calloc_safety(abuf->samples_filled / abuf->out_achans, sizeof(float));
          sample_move_d16_float(abuf->bufferf[i], &abuf->buffer16[0][i], abuf->samples_filled / abuf->out_achans, abuf->out_achans,
                                (abuf->s16_signed ? AFORM_SIGNED : AFORM_UNSIGNED), abuf->swap_endian, 1.0);
        }
      }
      abuf->out_interleaf = FALSE;
    }
  }

  if (abuf->bufferf == NULL) return FALSE;
  // now we should have float

  samps = abuf->samples_filled;
  if (abuf->in_interleaf) samps /= abuf->out_achans;

  // push to achan "audio_data", taking into account "audio_data_length" and "audio_channels"

  alen = samps;

  if (abuf->arate == 0) return FALSE;
  scale = (float)trate / (float)abuf->arate;
  alen = (int)((float)alen * scale);

  // malloc audio_data
  dst = (float **)lives_calloc(tchans, sizeof(float *));

  // copy data from abuf->bufferf[] to "audio_data"
  for (i = 0; i < tchans; i++) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    src = abuf->bufferf[i % abuf->out_achans];
    if (src != NULL) {
      dst[i] = lives_calloc(alen, sizeof(float));
      if (abuf->arate == trate) {
        lives_memcpy(dst[i], src, alen * sizeof(float));
      } else {
        // needs resample
        sample_move_float_float(dst[i], src, alen, scale, 1);
      }
    } else dst[i] = NULL;
    pthread_mutex_unlock(&mainw->abuf_mutex);
  }

  // set channel values
  weed_channel_set_audio_data(achan, dst, trate, tchans, alen);
  lives_free(dst);
  return TRUE;
}


////////////////////////////////////////
// audio streaming, older API

lives_pgid_t astream_pgid = 0;

boolean start_audio_stream(void) {
  const char *playername = "audiostreamer.pl";
  char *astream_name = NULL;
  char *astream_name_out = NULL;

  // playback plugin wants an audio stream - so fork and run the stream
  // player
  char *astname = lives_strdup_printf("livesaudio-%d.pcm", capable->mainpid);
  char *astname_out = lives_strdup_printf("livesaudio-%d.stream", capable->mainpid);
  char *astreamer, *com;

  int arate = 0;
  int afd;
  lives_alarm_t alarm_handle;

  ticks_t timeout = 0;

  astream_name = lives_build_filename(prefs->workdir, astname, NULL);

#ifndef IS_MINGW
  mkfifo(astream_name, S_IRUSR | S_IWUSR);
#endif

  astream_name_out = lives_build_filename(prefs->workdir, astname_out, NULL);

  lives_free(astname);
  lives_free(astname_out);

  if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    arate = (int)mainw->pulsed->out_arate;
    // TODO - chans, samps, signed, endian
#endif
  }

  if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    arate = (int)mainw->jackd->sample_out_rate;
    // TODO - chans, samps, signed, endian

#endif
  }

  astreamer = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_AUDIO_STREAM, playername, NULL);
  com = lives_strdup_printf("%s play %d \"%s\" \"%s\" %d", astreamer, mainw->vpp->audio_codec, astream_name, astream_name_out, arate);
  lives_free(astreamer);

  astream_pgid = lives_fork(com);

  alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

  do {
    // wait for other thread to create stream (or timeout)
    afd = lives_open2(astream_name, O_WRONLY | O_SYNC);
    if (afd != -1) break;
    lives_usleep(prefs->sleep_time);
  } while ((timeout = lives_alarm_check(alarm_handle)) > 0);
  lives_alarm_clear(alarm_handle);

  if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    mainw->pulsed->astream_fd = afd;
#endif
  }

  if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    mainw->jackd->astream_fd = afd;
#endif
  }

  lives_free(astream_name);
  lives_free(astream_name_out);

  return TRUE;
}


void stop_audio_stream(void) {
  if (astream_pgid > 0) {
    // if we were streaming audio, kill it
    const char *playername = "audiostreamer.pl";
    char *astname = lives_strdup_printf("livesaudio-%d.pcm", capable->mainpid);
    char *astname_out = lives_strdup_printf("livesaudio-%d.stream", capable->mainpid);
    char *astreamer = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_AUDIO_STREAM, playername, NULL);

    char *astream_name = lives_build_filename(prefs->workdir, astname, NULL);
    char *astream_name_out = lives_build_filename(prefs->workdir, astname_out, NULL);

    char *com;

    lives_free(astname);
    lives_free(astname_out);

    if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed->astream_fd > -1) close(mainw->pulsed->astream_fd);
      mainw->pulsed->astream_fd = -1;
#endif
    }
    if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (mainw->jackd->astream_fd > -1) close(mainw->jackd->astream_fd);
      mainw->jackd->astream_fd = -1;
#endif
    }

    lives_killpg(astream_pgid, LIVES_SIGKILL);
    lives_rm(astream_name);
    lives_free(astream_name);

    // astreamer should remove cooked stream
    com = lives_strdup_printf("\"%s\" cleanup %d \"%s\"", astreamer, mainw->vpp->audio_codec, astream_name_out);
    lives_system(com, FALSE);
    lives_free(astreamer);
    lives_free(com);
    lives_free(astream_name_out);
  }
}


void clear_audio_stream(void) {
  // remove raw and cooked streams
  char *astname = lives_strdup_printf("livesaudio-%d.pcm", capable->mainpid);
  char *astream_name = lives_build_filename(prefs->workdir, astname, NULL);
  char *astname_out = lives_strdup_printf("livesaudio-%d.stream", capable->mainpid);
  char *astream_name_out = lives_build_filename(prefs->workdir, astname_out, NULL);
  lives_rm(astream_name);
  lives_rm(astream_name_out);
  lives_free(astname);
  lives_free(astream_name);
  lives_free(astname_out);
  lives_free(astream_name_out);
}


LIVES_GLOBAL_INLINE void audio_stream(void *buff, size_t nbytes, int fd) {
  if (fd != -1) {
    lives_write(fd, buff, nbytes, TRUE);
  }
}


LIVES_GLOBAL_INLINE lives_cancel_t handle_audio_timeout(void) {
  char *msg2 = (prefs->audio_player == AUD_PLAYER_PULSE) ? lives_strdup(_("\nClick Retry to attempt to restart the audio server.\n")) :
               lives_strdup("");

  char *msg = lives_strdup_printf(
                _("LiVES was unable to connect to %s.\nPlease check your audio settings and restart %s\nand LiVES if necessary.\n%s"),
                audio_player_get_display_name(prefs->aplayer),
                audio_player_get_display_name(prefs->aplayer), msg2);
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    int retval = do_abort_cancel_retry_dialog(msg, NULL);
    if (retval == LIVES_RESPONSE_RETRY) pulse_try_reconnect();
    else {
      mainw->aplayer_broken = TRUE;
      switch_aud_to_none(FALSE);
    }
#endif
  } else {
    do_blocking_error_dialog(msg);
    mainw->aplayer_broken = TRUE;
    switch_aud_to_none(FALSE);
  }
  lives_free(msg);
  lives_free(msg2);
  return CANCEL_ERROR;
}


#if 0
void nullaudio_time_reset(int64_t offset) {
  mainw->nullaudo_startticks = lives_get_current_ticks();
  mainw->nullaudio_seek_posn = 0;
  mainw->nullaudio_arate = DEF_AUDIO_RATE;
  mainw->nullaudio_playing_file = -1;
  mainw->deltaticks = mainw->startticks = 0;
}


void nullaudio_arate_set(int arate) {
  mainw->nullaudio_arate = arate;
}


void nullaudio_seek_set(offs64_t seek_posn) {
  // (virtual) seek posn in bytes
  mainw->nullaudio_seek_posn = seek_posn;
}


void nullaudio_clip_set(int clipno) {
  mainw->nullaudio_playing_file = clipno;
}


int64_t nullaudio_update_seek_posn() {
  if (!CURRENT_CLIP_HAS_AUDIO) return mainw->nullaudio_seek_posn;
  else {
    ticks_t current_ticks = lives_get_current_ticks();
    mainw->nullaudio_seek_posn += ((int64_t)((double)(current_ticks - mainw->nullaudio_start_ticks) / USEC_TO_TICKS / 1000000. *
                                   mainw->nullaudio_arate)) * afile->achans * (afile->asampsize >> 3);
    mainw->nullaudo_startticks = current_ticks;
    if (mainw->nullaudio_seek_posn < 0) {
      if (mainw->nullaudio_loop == AUDIO_LOOP_NONE) {
        if (mainw->whentostop == STOP_ON_AUD_END) {
          mainw->cancelled = CANCEL_AUD_END;
        }
      } else {
        if (mainw->nullaudio_loop == AUDIO_LOOP_PINGPONG) {
          mainw->nullaudio_arate = -mainw->nullaudio->arate;
          mainw->nullaudio_seek_posn = 0;
        } else mainw->nullaudio_seek_posn += mainw->nullaudio_seek_end;
      }
    } else {
      if (mainw->nullaudio_seek_posn > mainw->nullaudio_seek_end) {
        // reached the end, forwards
        if (mainw->nullaudio_loop == AUDIO_LOOP_NONE) {
          if (mainw->whentostop == STOP_ON_AUD_END) {
            mainw->cancelled = CANCEL_AUD_END;
          }
        } else {
          if (mainw->nullaudio_loop == AUDIO_LOOP_PINGPONG) {
            mainw->nullaudio_arate = -mainw->nullaudio_arate;
            mainw->nullaudio_seek_posn = mainw->nullaudio_seek_end - (mainw->nullaudio_seek_pos - mainw->nullaudio_seek_end);
          } else {
            mainw->nullaudio_seek_posn -= mainw->nullaudio_seek_end;
          }
          nullaudio_set_rec_avals();
        }
      }
    }
  }
}


void nullaudio_get_rec_avals(void) {
  lives_clip_t *afile = mainw->files[mainw->nullaudio_playing_file];
  mainw->rec_aclip = mainw->nulllaudio_playing_file;

  if (mainw->rec_aclip != -1) {
    mainw->rec_aseek = (double)mainw->nullaudio_seek_pos / (double)(afile->arps * afile->achans * afile->asampsize / 8);
    mainw->rec_avel = SIGNED_DIVIDE((double)pulsed->in_arate, (double)afile->arate);
  }
}


static void nullaudio_set_rec_avals(boolean is_forward) {
  // record direction change (internal)
  mainw->rec_aclip = mainw->nullaudio_playing_file;
  if (mainw->rec_aclip != -1) {
    nullaudio_get_rec_avals();
  }
}

#endif
