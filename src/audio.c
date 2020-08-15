// audio.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2020
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "audio.h"
#include "events.h"
#include "callbacks.h"
#include "effects.h"
#include "resample.h"

static char *storedfnames[NSTOREDFDS];
static int storedfds[NSTOREDFDS];
static boolean storedfdsset = FALSE;


static void audio_reset_stored_fnames(void) {
  for (register int i = 0; i < NSTOREDFDS; i++) {
    storedfnames[i] = NULL;
    storedfds[i] = -1;
  }
  storedfdsset = TRUE;
}


LIVES_GLOBAL_INLINE char *get_achannel_name(int totchans, int idx) {
  if (totchans == 1) return (_("Mono"));
  if (totchans == 2) {
    if (idx == 0) return (_("Left channel"));
    if (idx == 1) return (_("Right channel"));
  }
  return lives_strdup_printf(_("Audio channel %d"), idx);
}


LIVES_GLOBAL_INLINE char *get_audio_file_name(int fnum, boolean opening) {
  char *fname;
  if (!opening) {
    if (IS_VALID_CLIP(fnum))
      fname = lives_build_filename(prefs->workdir, mainw->files[fnum]->handle, CLIP_AUDIO_FILENAME, NULL);
    else
      fname = lives_build_filename(prefs->workdir, CLIP_AUDIO_FILENAME, NULL);
  } else {
    if (IS_VALID_CLIP(fnum))
      fname = lives_build_filename(prefs->workdir, mainw->files[fnum]->handle, CLIP_TEMP_AUDIO_FILENAME, NULL);
    else
      fname = lives_build_filename(prefs->workdir, CLIP_TEMP_AUDIO_FILENAME, NULL);
  }
  return fname;
}


LIVES_GLOBAL_INLINE char *lives_get_audio_file_name(int fnum) {
  char *fname = get_audio_file_name(fnum, FALSE);
  if (mainw->files[fnum]->opening && !lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
    lives_free(fname);
    fname = get_audio_file_name(fnum, TRUE);
  }
  return fname;
}


LIVES_GLOBAL_INLINE const char *audio_player_get_display_name(const char *aplayer) {
  if (!strcmp(aplayer, AUDIO_PLAYER_PULSE)) return AUDIO_PLAYER_PULSE_AUDIO;
  return aplayer;
}


void audio_free_fnames(void) {
  // cleanup stored filehandles after playback/fade/render
  if (!storedfdsset) return;
  for (int i = 0; i < NSTOREDFDS; i++) {
    lives_freep((void **)&storedfnames[i]);
    if (storedfds[i] > -1) lives_close_buffered(storedfds[i]);
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

  nsampsize = (abuf->samples_filled + nsamples);

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


boolean audiofile_is_silent(int fnum, double start, double end) {
  double atime = start;
  lives_clip_t *afile = mainw->files[fnum];
  char *filename = lives_get_audio_file_name(mainw->current_file);
  int afd = lives_open_buffered_rdonly(filename);
  float xx;
  int c;
  lives_free(filename);
  while (atime <= end) {
    for (c = 0; c < afile->achans; c++) {
      if (afd == -1) {
        THREADVAR(read_failed) = -2;
        return TRUE;
      }
      if ((xx = get_float_audio_val_at_time(fnum, afd, atime, c, afile->achans)) != 0.) {
        lives_close_buffered(afd);
        return FALSE;
      }
    }
    atime += 1. / afile->arate;
  }
  lives_close_buffered(afd);
  return TRUE;
}


float get_float_audio_val_at_time(int fnum, int afd, double secs, int chnum, int chans) {
  // return audio level between -1.0 and +1.0
  // afd must be opened with lives_open_buffered_rdonly()
  lives_clip_t *afile = mainw->files[fnum];
  off_t apos;
  uint8_t val8, val8b;
  uint16_t val16;
  float val;
  size_t quant = afile->achans * afile->asampsize / 8;
  size_t bytes = (size_t)(secs * (double)afile->arate) * quant;

  if (!bytes) return 0.;

  apos = ((size_t)(bytes / quant) * quant); // quantise

  apos += afile->asampsize / 8 * chnum;
  lives_lseek_buffered_rdonly_absolute(afd, apos);

  if (afile->asampsize == 8) {
    // 8 bit sample size
    if (!lives_read_buffered(afd, &val8, 1, TRUE)) return 0.;
    if (!(afile->signed_endian & AFORM_UNSIGNED)) val = val8 >= 128 ? val8 - 256 : val8;
    else val = val8 - 127;
    val /= 127.;
  } else {
    // 16 bit sample size
    if (!lives_read_buffered(afd, &val8, 1, TRUE) || !lives_read_buffered(afd, &val8b, 1, TRUE)) return 0.;
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


void sample_silence_stream(int nchans, int64_t nframes) {
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


/* //void normalise(float rms) */
/*   if (i == 0) { */
/*     int dlen = weed_layer_get_audio_length(layer); */
/*     for (ch =0; ch < achans; ch++) { */
/*       for (smp = 0; smp < dlen; smp ++) { */
/* 	avg += adata[ch][smp]; */
/*       } */
/*       avg /= dlen; */
/*       for (smp = 0; smp < dlen; smp ++) { */
/* 	avg += adata[ch][smp]; */
/*       } */
/*     }}}       */



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
      ptr = ptr > src ? (ptr < (src_end + ccount) ? ptr : (src_end + ccount)) : src;

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


/**
   @brief convert from any number of source channels to any number of destination channels - both interleaved
*/
void sample_move_d16_d16(int16_t *dst, int16_t *src,
                         uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_endian, int swap_sign) {
  // TODO: going from >1 channels to 1, we should average
  register int nSrcCount, nDstCount;
  register int src_offset_i = 0;
  register int ccount = 0;
  static float rem = 0.;
  register float src_offset_f = rem;
  int16_t *ptr;
  int16_t *src_end;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_f = ((float)(nsamples) * (-scale) - 1.f - rem);
    src_offset_i = (int)src_offset_f * nSrcChannels;
  }


  // take care of rounding errors
  src_end = src + tbytes / 2 - nSrcChannels;

  if ((size_t)((fabsf(scale) * (float)nsamples) + rem) * nSrcChannels * 2 > tbytes)
    scale = scale > 0. ? ((float)(tbytes  / nSrcChannels / 2) - rem) / (float)nsamples
            :  -(((float)(tbytes  / nSrcChannels / 2) - rem) / (float)nsamples);

  while (nsamples--) {
    if ((nSrcCount = nSrcChannels) == (nDstCount = nDstChannels) && !swap_endian && !swap_sign) {
      // same number of channels

      if (scale == 1.f) {
        lives_memcpy((void *)dst, (void *)src, nsamples * 2 * nSrcChannels);
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
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(((*ptr & 0x00FF) << 8) + (*ptr >> 8)
                + SAMPLE_MAX_16BITI);
          else *(dst++) = ((*ptr & 0x00FF) << 8) + (*ptr >> 8) - SAMPLE_MAX_16BITI;
        } else {
          if (!swap_sign) *(dst++) = (((*ptr) & 0x00FF) << 8) + ((*ptr) >> 8);
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) =
              (uint16_t)(((((uint16_t)(*ptr + SAMPLE_MAX_16BITI)) & 0x00FF) << 8) +
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
    src_offset_i = (int)((src_offset_f += scale) + .4999) * nSrcChannels;
  }
  if (src_offset_f > tbytes) rem = src_offset_f - (float)tbytes;
  else rem = 0.;
}


/**
   @brief convert from any number of source channels to any number of destination channels - 8 bit output
*/
void sample_move_d16_d8(uint8_t *dst, short *src,
                        uint64_t nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  // TODO: going from >1 channels to 1, we should average
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


float sample_move_d16_float(float *dst, short *src, uint64_t nsamples, uint64_t src_skip, int is_unsigned, boolean rev_endian,
                            float vol) {
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
  xa = 2. * vol / (SAMPLE_MAX_16BIT_P + SAMPLE_MAX_16BIT_N);
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


#define CLIP_DECAY ((double)16535. / (double)16536.)

/**
   @brief convert float samples back to int
   interleaved is for the float buffer; output int is always interleaved
   scale is out_sample_rate / in_sample_rate (so 2.0 would play twice as fast, etc.)
   nsamps is number of out samples, asamps is out sample bit size (8 or 16)
   output is in holding_buff which can be cast to uint8_t *, int16_t *, or uint16_t *
   returns number of frames out

   clipping is applied so that -1.0 <= fval <= 1.0
   the clipping value is applied linearly to vol (as a divisor), and if not reset it will decay so
   clip = 1.0 + (clip - 1.0) * CLIP_DECAY each sample
   --> clip = 1.0 + clip * CLIP_DECAY - CLIP_DECAY
   --> clip = (clip * CLIP_DECAY) + (1.0 - CLIP_DECAY)
   --> clip *= CLIP_DECAY; clip += (1.0 - CLIP_DECAY)
*/
int64_t sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, float scale, int chans, int asamps,
                              int usigned, boolean rev_endian, boolean interleaved, float vol) {
  int64_t frames_out = 0l;
  register int i;
  register int offs = 0, coffs = 0, lcoffs = -1;
  static float coffs_f = 0.f;
  const double add = (1.0 - CLIP_DECAY);

  short *hbuffs = (short *)holding_buff;
  unsigned short *hbuffu = (unsigned short *)holding_buff;
  unsigned char *hbuffc = (unsigned char *)holding_buff;
  short val[chans];
  unsigned short valu[chans];
  static float clip = 1.0;
  float ovalf[chans], valf[chans], fval;
  double volx = (double)vol, ovolx = -1.;
  boolean checklim = FALSE;

  asamps >>= 3;

  if (clip > 1.0) checklim = TRUE;

  while ((nsamps - frames_out) > 0) {
    frames_out++;
    if (checklim) {
      if (clip > 1.0)  {
        clip = (float)((double)clip * CLIP_DECAY + add);
        volx = ((double)vol / (double)clip);
      } else {
        checklim = FALSE;
        clip = 1.0;
        volx = (double)vol;
      }
    }

    for (i = 0; i < chans; i++) {
      if (coffs != lcoffs) {
        if ((fval = fabsf((ovalf[i] = *(float_buffer[i] + (interleaved ? (coffs * chans) : coffs))))) > clip) {
          clip = fval;
          checklim = TRUE;
          volx = ((double)vol / (double)clip);
          i = -1;
          continue;
        }
      }
      if (volx != ovolx || coffs != lcoffs) {
        valf[i] = ovalf[i] * volx;
        if (valf[i] > vol) valf[i] = vol;
        else if (valf[i] < -vol) valf[i] = -vol;
        ovolx = volx;
        val[i] = (short)(valf[i] * (valf[i] > 0. ? SAMPLE_MAX_16BIT_P : SAMPLE_MAX_16BIT_N));
        if (usigned) valu[i] = (val[i] + SAMPLE_MAX_16BITI);
      }

      if (asamps == 2) {
        if (!rev_endian) {
          if (usigned) *(hbuffu + offs) = valu[i];
          else *(hbuffs + offs) = val[i];
        } else {
          if (usigned) {
            *(hbuffc + offs) = valu[i] & 0x00FF;
            *(hbuffc + (++offs)) = (valu[i] & 0xFF00) >> 8;
          } else {
            *(hbuffc + offs) = val[i] & 0x00FF;
            *(hbuffc + (++offs)) = (val[i] & 0xFF00) >> 8;
          }
        }
      } else {
        *(hbuffc + offs) = (unsigned char)(valu[i] >> 8);
      }
      offs++;
    }
    lcoffs = coffs;
    coffs = (int)(coffs_f += scale);
  }
  coffs_f -= (float)coffs;
  if (prefs->show_dev_opts) {
    if (frames_out != nsamps) {
      char *msg = lives_strdup_printf("audio float -> int: buffer mismatch of %ld samples\n", frames_out - nsamps);
      LIVES_WARN(msg);
      lives_free(msg);
    }
  }
  return frames_out;
}


// play from memory buffer

/**
   @brief copy audio data from cache into audio sound buffer
   - float32 version (e.g. jack)
   nchans, nsamps. out_arate all refer to player values
*/
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
      // request caching thread to fill another buffer
      mainw->abufs_to_fill++;
      if (mainw->jackd->read_abuf < 0) {
        // playback ended while we were processing
        pthread_mutex_unlock(&mainw->abuf_mutex);
        return samples_out;
      }
      mainw->jackd->read_abuf++;
      wake_audio_thread();

      if (mainw->jackd->read_abuf >= prefs->num_rtaudiobufs) mainw->jackd->read_abuf = 0;

      abuf = mainw->jackd->abufs[mainw->jackd->read_abuf];

      pthread_mutex_unlock(&mainw->abuf_mutex);
    }
  }
#endif

  return samples_out;
}


/**
   @brief copy audio data from cache into audio sound buffer
   - int 16 version (e.g. pulseaudio)
   nchans, nsamps. out_arate all refer to player values
*/
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
      wake_audio_thread();
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


// for pulse audio we use S16LE interleaved, and the volume is adjusted later

static size_t chunk_to_int16_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int64_t frames_out;
  int chans = abuf->out_achans;
  register size_t offs = abuf->samples_filled * chans;

  frames_out = sample_move_float_int(abuf->buffer16[0] + offs, float_buffer, nsamps, 1., chans, 16,
                                     0, 0, 0, 1.0);

  return (size_t)frames_out;
}


//#define DEBUG_ARENDER

boolean pad_with_silence(int out_fd, void *buff, off64_t oins_size, int64_t ins_size, int asamps, int aunsigned,
                         boolean big_endian) {
  // asamps is sample size in BYTES
  // fill to ins_pt with zeros (or 0x80.. for unsigned)
  // oins_size is the current file length, ins_size is the point where we want append to (both in bytes)
  // if ins size < oins_size we just seek to ins_size
  // otherwise we pad from oins_size to ins_size

  static uint64_t *zero_buff = NULL;
  static int oasamps = 0;
  static int ounsigned = 0;
  static int orevendian = 0;
  size_t sbytes;
  size_t sblocksize = SILENCE_BLOCK_SIZE;
  register int i;

  boolean retval = TRUE;
  boolean revendian = FALSE;

  if (ins_size <= oins_size) return FALSE;
  sbytes = ins_size - oins_size;

#ifdef DEBUG_ARENDER
  g_print("sbytes is %d\n", sbytes);
#endif
  if (sbytes > 0) {
    if ((big_endian && capable->byte_order == LIVES_LITTLE_ENDIAN)
        || (!big_endian && capable->byte_order == LIVES_LITTLE_ENDIAN)) revendian = TRUE;
    if (out_fd >= 0) lives_lseek_buffered_writer(out_fd, oins_size);
    else {
      if (!buff) return FALSE;
      buff += oins_size;
    }
    if (zero_buff) {
      if (ounsigned != aunsigned || oasamps != asamps || orevendian != revendian) {
        lives_free(zero_buff);
        zero_buff = NULL;
      }
    }
    if (!zero_buff) {
      sblocksize >>= 3;
      zero_buff = (uint64_t *)lives_calloc_safety(sblocksize, 8);
      if (aunsigned) {
        if (asamps > 1) {
          uint64_t theval = (revendian ? 0x0080008000800080ul : 0x8000800080008000ul);
          for (i = 0; i < sblocksize; i ++) {
            zero_buff[i] = theval;
          }
        } else lives_memset(zero_buff, 0x80, SILENCE_BLOCK_SIZE);
      }
      sblocksize <<= 3;
      ounsigned = aunsigned;
      oasamps = asamps;
      orevendian = revendian;
    }
    for (i = 0; i < sbytes; i += SILENCE_BLOCK_SIZE) {
      if (sbytes - i < SILENCE_BLOCK_SIZE) sblocksize = sbytes - i;
      if (out_fd >= 0) {
        lives_write_buffered(out_fd, (const char *)zero_buff, sblocksize, TRUE);
        if (THREADVAR(write_failed) == out_fd + 1) {
          THREADVAR(write_failed) = 0;
          retval = FALSE;
        }
      } else {
        lives_memcpy(buff, zero_buff, sblocksize);
        buff += sblocksize;
      }
    }
  } else if (out_fd >= 0) {
    lives_lseek_buffered_writer(out_fd, ins_size);
  }
  return retval;
}


LIVES_LOCAL_INLINE void audio_process_events_to(weed_timecode_t tc) {
  if (tc >= get_event_timecode(mainw->audio_event)) {
#ifdef DEBUG_ARENDER
    g_print("smallblock %ld to %ld\n", weed_event_get_timecode(mainw->audio_event), tc);
#endif
    get_audio_and_effects_state_at(NULL, mainw->audio_event, tc, LIVES_PREVIEW_TYPE_AUDIO_ONLY, FALSE);
  }
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
  off_t seek;

  int in_fd[nfiles];
  int in_asamps[nfiles];
  int in_achans[nfiles];
  int in_arate[nfiles];
  int in_arps[nfiles];
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
  double zavel, zzavel, zavel_max = 0.;

  boolean out_reverse_endian = FALSE;
  boolean is_fade = FALSE;
  boolean use_live_chvols = FALSE;

  int out_asamps = to_file > -1 ? outfile->asampsize / 8 : obuf->out_asamps / 8;
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

  register int i;

  int64_t frames_out = 0;
  int64_t ins_size = 0l, cur_size;
  int64_t tsamples = ((double)(tc_end - tc_start) / TICKS_PER_SECOND_DBL * (double)out_arate + .5);
  int64_t blocksize, zsamples, xsamples;
  int64_t tot_frames = 0l;

  float *float_buffer[out_achans * nfiles];
  float *chunk_float_buffer[out_achans * nfiles];
  float clip_vol;

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
    out_fd = lives_open_buffered_writer(outfilename, S_IRUSR | S_IWUSR, FALSE);
    lives_free(outfilename);

    if (out_fd < 0) {
      lives_freep((void **)&THREADVAR(write_failed_file));
      THREADVAR(write_failed_file) = lives_strdup(outfilename);
      THREADVAR(write_failed) = -2;
      return 0l;
    }

    cur_size = lives_buffered_orig_size(out_fd);

    if (opvol_start == opvol_end && opvol_start == 0.) ins_pt = tc_end / TICKS_PER_SECOND_DBL;
    ins_pt *= out_achans * out_arate * out_asamps;
    ins_size = ((int64_t)(ins_pt / out_achans / out_asamps + .5)) * out_achans * out_asamps;

    if ((!out_bendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
      out_reverse_endian = TRUE;
    else out_reverse_endian = FALSE;

    if (ins_size > cur_size) {
      // fill to ins_pt with zeros
      pad_with_silence(out_fd, NULL, cur_size, ins_size, out_asamps, out_unsigned, out_bendian);
    } else lives_lseek_buffered_writer(out_fd, ins_size);

    if (opvol_start == opvol_end && opvol_start == 0.) {
      lives_close_buffered(out_fd);
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
    in_arps[track] = infile->arps;
    in_unsigned[track] = infile->signed_endian & AFORM_UNSIGNED;
    in_bendian = infile->signed_endian & AFORM_BIG_ENDIAN;

    if (LIVES_UNLIKELY(in_achans[track] == 0)) is_silent[track] = TRUE;
    else {
      if ((!in_bendian && (capable->byte_order == LIVES_BIG_ENDIAN)) ||
          (in_bendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
        in_reverse_endian[track] = TRUE;
      else in_reverse_endian[track] = FALSE;

      /// this is not the velocity we will use for reading, but we need to estimate how many bytes we will read in
      /// so we can calculate how many buffers to allocate
      zavel = avels[track] * (double)in_arate[track] / (double)out_arate * in_asamps[track] * in_achans[track];
      if (fabs(zavel) > zavel_max) zavel_max = fabs(zavel);

      infilename = lives_get_audio_file_name(from_files[track]);

      // try to speed up access by keeping some files open
      if (track < NSTOREDFDS && storedfnames[track] != NULL && !strcmp(infilename, storedfnames[track])) {
        in_fd[track] = storedfds[track];
      } else {
        if (track < NSTOREDFDS && storedfds[track] > -1) lives_close_buffered(storedfds[track]);
        in_fd[track] = lives_open_buffered_rdonly(infilename);
        if (in_fd[track] < 0) {
          lives_freep((void **)&THREADVAR(read_failed_file));
          THREADVAR(read_failed_file) = lives_strdup(infilename);
          THREADVAR(read_failed) = -2;
        } else {
          if (track < NSTOREDFDS) {
            storedfds[track] = in_fd[track];
            storedfnames[track] = lives_strdup(infilename);
          }
        }
      }
      seek = lives_buffered_offset(in_fd[track]);
      seekstart[track] = quant_abytes(fromtime[track], in_arps[track], in_achans[track], in_asamps[track]);
      if (labs(seekstart[track] - seek) > AUD_DIFF_MIN) {
        lives_lseek_buffered_rdonly_absolute(in_fd[track], seekstart[track]);
      }
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
      pad_with_silence(out_fd, NULL, oins_size, ins_size, out_asamps, out_unsigned, out_bendian);
      lives_close_buffered(out_fd);
    } else {
      if (prefs->audio_player == AUD_PLAYER_JACK) {
        for (i = 0; i < out_achans; i++) {
          lives_memset((void *)obuf->bufferf[i] + obuf->samples_filled * sizeof(float), 0, tsamples * sizeof(float));
        }
      } else {
        pad_with_silence(-1, (void *)obuf->buffer16[0], obuf->samples_filled * out_asamps * out_achans,
                         (obuf->samples_filled + tsamples) * out_asamps * out_achans, out_asamps, obuf->s16_signed
                         ? AFORM_SIGNED : AFORM_UNSIGNED,
                         ((capable->byte_order == LIVES_LITTLE_ENDIAN && obuf->swap_endian == SWAP_L_TO_X)
                          || (capable->byte_order == LIVES_LITTLE_ENDIAN && obuf->swap_endian != SWAP_L_TO_X)));
      }
    }
    obuf->samples_filled += tsamples;
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

  // TODO - need to check amixer != NULL and get vals from sliders
  /* if (mainw->multitrack != NULL && mainw->multitrack->audio_vols != NULL && obuf != NULL) { */
  /*   use_live_chvols = TRUE; */
  /*   audio_vols = mainw->multitrack->audio_vols; */
  /* } */

  while (tsamples > 0) {
    tsamples -= xsamples;

    for (track = 0; track < nfiles; track++) {
      if (is_silent[track]) {
        // zero float_buff
        for (c = 0; c < out_achans; c++) {
          lives_memset(float_buffer[track * out_achans + c], 0, xsamples * sizeof(float));
        }
        continue;
      }

      /// calculate tbytes for xsamples
      zavel = avels[track] * (double)in_arate[track] / (double)out_arate;

      //g_print("zavel is %f\n", zavel);
      /// tbytes: how many bytes we want to read in. This is xsamples * the track velocity.
      /// we add a small random factor here, so half the time we round up, half the time we round down
      /// otherwise we would be gradually losing or gaining samples
      tbytes = (int)((double)xsamples * fabs(zavel) + ((double)fastrand() / (double)LIVES_MAXUINT64)) *
               in_asamps[track] * in_achans[track];

      if (tbytes <= 0) {
        for (c = 0; c < out_achans; c++) {
          lives_memset(float_buffer[track * out_achans + c], 0, xsamples * sizeof(float));
        }
        continue;
      }

      in_buff = (uint8_t *)lives_calloc_safety(tbytes, 1);

      if (in_fd[track] > -1) {
        if (zavel < 0.) {
          lives_buffered_rdonly_set_reversed(in_fd[track], TRUE);
          lives_lseek_buffered_rdonly(in_fd[track], - tbytes);
        } else {
          lives_buffered_rdonly_set_reversed(in_fd[track], FALSE);
          //lives_buffered_rdonly_slurp(in_fd[track], seekstart[track]);
        }
      }

      bytes_read = 0;

      if (in_fd[track] > -1) bytes_read = lives_read_buffered(in_fd[track], in_buff, tbytes, TRUE);

      if (bytes_read < 0) bytes_read = 0;

      if (in_fd[track] > -1) {
        if (zavel < 0.) {
          lives_lseek_buffered_rdonly(in_fd[track], -tbytes);
        }
      }

      fromtime[track] = (double)lives_buffered_offset(in_fd[track]) / (double)(in_asamps[track] * in_achans[track] * in_arate[track]);

      if (from_files[track] == mainw->ascrap_file) {
        // be forgiving with the ascrap file
        if (THREADVAR(read_failed) == in_fd[track] + 1) {
          THREADVAR(read_failed) = 0;
        }
      }

      if (bytes_read < tbytes && bytes_read >= 0)  {
        pad_with_silence(-1, in_buff, bytes_read, tbytes, in_asamps[track], mainw->files[from_files[track]]->signed_endian
                         & AFORM_UNSIGNED, mainw->files[from_files[track]]->signed_endian & AFORM_BIG_ENDIAN);
        //lives_memset(in_buff + bytes_read, 0, tbytes - bytes_read);
      }

      nframes = (tbytes / (in_asamps[track]) / in_achans[track] / fabs(zavel) + .001);

      /// convert to float
      /// - first we convert to 16 bit stereo (if it was 8 bit and / or mono) and we resample
      /// input is tbytes bytes at rate * velocity, and we should get out nframes audio frames at out_arate. out_achans
      /// result is in holding_buff
      zzavel = zavel;
      if (in_asamps[track] == 1) {
        if (zavel < 0.) {
          if (reverse_buffer(in_buff, tbytes, in_achans[track]))
            zavel = -zavel;
        }
        sample_move_d8_d16(holding_buff, (uint8_t *)in_buff, nframes, tbytes, zavel, out_achans, in_achans[track], 0);
      } else {
        if (zavel < 0.) {
          if (reverse_buffer(in_buff, tbytes, in_achans[track] * 2))
            zavel = -zavel;
        }
        sample_move_d16_d16(holding_buff, (short *)in_buff, nframes, tbytes, zavel, out_achans,
                            in_achans[track], in_reverse_endian[track] ? SWAP_X_TO_L : 0, 0);
      }
      zavel = zzavel;
      lives_free(in_buff);
      /// if we are previewing a rendering, we would get double the volume adjustment, once from the rendering and again from
      /// the audio player, so in that case we skip the adjustment here
      if (!mainw->preview_rendering) clip_vol = lives_vol_from_linear(mainw->files[from_files[track]]->vol);
      else clip_vol = 1.;
      for (c = 0; c < out_achans; c++) {
        /// now we convert to holding_buff to float in float_buffer and adjust the track volume
        sample_move_d16_float(float_buffer[c + track * out_achans], holding_buff + c, nframes,
                              out_achans, in_unsigned[track], FALSE, clip_vol * (use_live_chvols ? 1. : chvol[track]));
      }
    }

    // next we send small chunks at a time to the audio vol/pan effect + any other audio effects
    shortcut = NULL;
    blocksize = render_block_size; ///< this is our chunk size

    for (i = 0; i < xsamples; i += render_block_size) {
      if (i + render_block_size > xsamples) blocksize = xsamples - i;

      for (track = 0; track < nfiles; track++) {
        /* if (use_live_chvols) { */
        /*   chvol[track] = giw_vslider_get_value(GIW_VSLIDER(amixer->ch_sliders[track])); */
        /* } */
        for (c = 0; c < out_achans; c++) {
          //g_print("xvals %.4f\n",*(float_buffer[track*out_achans+c]+i));
          chunk_float_buffer[track * out_achans + c] = float_buffer[track * out_achans + c] + i;
          /* if (use_live_chvols) { */
          /*   for (smp = 0; smp < blocksize; smp++) chunk_float_buffer[track * out_achans + c][smp] *= chvol[track]; */
          /* } */
        }
      }

      if (mainw->event_list != NULL && !(mainw->multitrack == NULL && from_files[0] == mainw->ascrap_file)) {
        // we need to apply all audio effects with output here.
        // even in clipedit mode (for preview/rendering with an event list)
        // also, we will need to keep updating mainw->afilter_map from mainw->event_list,
        // as filters may switched on and off during the block

        int nbtracks = 0;

        // process events up to current tc:
        // filter inits and deinits, and filter maps will update the current fx state
        if (tc > 0) audio_process_events_to(tc);

        if (mainw->multitrack != NULL || mainw->afilter_map != NULL) {

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
      }

      if (mainw->multitrack == NULL && opvol_end != opvol_start) {
        time += (double)frames_out / (double)out_arate;
        opvol = opvol_start + (opvol_end - opvol_start) * (time / (double)((tc_end - tc_start) / TICKS_PER_SECOND_DBL));
        opvol = lives_vol_from_linear(opvol);
      }

      if (is_fade) {
        // output to file
        // convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
        frames_out = sample_move_float_int((void *)finish_buff, chunk_float_buffer, blocksize, 1., out_achans,
                                           out_asamps * 8, out_unsigned, out_reverse_endian, FALSE, opvol);
        lives_write_buffered(out_fd, finish_buff, frames_out * out_asamps * out_achans, TRUE);
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

        lives_write_buffered(out_fd, finish_buff, frames_out * out_asamps * out_achans, TRUE);
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
      if (track >= NSTOREDFDS && in_fd[track] > -1) lives_close_buffered(in_fd[track]);
    }
  }
  //#define DEBUG_ARENDER
  if (to_file > -1) {
#ifdef DEBUG_ARENDER
    g_print("fs is %ld %s\n", get_file_size(out_fd), cfile->handle);
#endif
    lives_close_buffered(out_fd);
  }

  return tot_frames;
}


void aud_fade(int fileno, double startt, double endt, double startv, double endv) {
  double vel = 1., vol = 1.;

  render_audio_segment(1, &fileno, fileno, &vel, &startt, (weed_timecode_t)(startt * TICKS_PER_SECOND_DBL),
                       (weed_timecode_t)(endt * TICKS_PER_SECOND_DBL), &vol, startv, endv, NULL);

  if (THREADVAR(write_failed)) {
    char *outfilename = lives_get_audio_file_name(fileno);
    THREADVAR(write_failed) = 0;
    do_write_failed_error_s(outfilename, NULL);
    lives_free(outfilename);
  }

  if (THREADVAR(read_failed)) {
    char *infilename = lives_get_audio_file_name(fileno);
    THREADVAR(read_failed) = 0;
    do_read_failed_error_s(infilename, NULL);
    lives_free(infilename);
  }
}


void preview_audio(void) {
  // start a minimalistic player with only audio
  mainw->play_start = cfile->start;
  mainw->play_end = cfile->end;
  mainw->playing_sel = TRUE;

  if (cfile->achans > 0) {
    cfile->aseek_pos = (off64_t)(cfile->real_pointer_time * cfile->arate) * cfile->achans * (cfile->asampsize / 8);
    if (mainw->playing_sel) {
      off64_t apos = (off64_t)((double)(mainw->play_start - 1.) / cfile->fps * cfile->arate) * cfile->achans *
                     (cfile->asampsize / 8);
      if (apos > cfile->aseek_pos) cfile->aseek_pos = apos;
    }
    if (cfile->aseek_pos > cfile->afilesize) cfile->aseek_pos = 0.;
    if (mainw->current_file == 0 && cfile->arate < 0) cfile->aseek_pos = cfile->afilesize;
  }
  // start up our audio player (jack or pulse)
  if (prefs->audio_player == AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    if (mainw->jackd != NULL) jack_aud_pb_ready(mainw->current_file);
#endif
  } else if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed != NULL) pulse_aud_pb_ready(mainw->current_file);
#endif

#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      mainw->write_abuf = 0;

      // fill our audio buffers now
      // this will also get our effects state
      pthread_mutex_lock(&mainw->abuf_mutex);
      mainw->jackd->read_abuf = 0;
      mainw->abufs_to_fill = 0;
      pthread_mutex_unlock(&mainw->abuf_mutex);
      if (mainw->event_list != NULL)
        mainw->jackd->is_paused = mainw->jackd->in_use = TRUE;
    }
#endif
    mainw->playing_file = mainw->current_file;
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && cfile->achans > 0 && cfile->laudio_time > 0. &&
        !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->jackd != NULL
        && mainw->jackd->playing_file > -1) {
      if (!jack_audio_seek_frame(mainw->jackd, mainw->aframeno)) {
        if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd, mainw->aframeno);
        else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
      }
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && cfile->achans > 0 && cfile->laudio_time > 0. &&
        !mainw->is_rendering && !(cfile->opening && !mainw->preview) && mainw->pulsed != NULL
        && mainw->pulsed->playing_file > -1) {
      if (!pulse_audio_seek_frame(mainw->pulsed, mainw->aframeno)) {
        handle_audio_timeout();
        return;
      }
    }
#endif

    while (mainw->cancelled == CANCEL_NONE) {
      mainw->video_seek_ready = TRUE;
      lives_nanosleep(1000);
    }
    mainw->playing_file = -1;

    // reset audio buffers
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) {
      // must do this before deinit fx
      pthread_mutex_lock(&mainw->abuf_mutex);
      mainw->jackd->read_abuf = -1;
      mainw->jackd->in_use = FALSE;
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL) {
      pthread_mutex_lock(&mainw->abuf_mutex);
      mainw->pulsed->read_abuf = -1;
      mainw->pulsed->in_use = FALSE;
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }
#endif

    mainw->playing_sel = FALSE;
    lives_ce_update_timeline(0, cfile->real_pointer_time);
    if (mainw->cancelled == CANCEL_AUDIO_ERROR) {
      handle_audio_timeout();
    }

#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && (mainw->jackd != NULL || mainw->jackd_read != NULL)) {
      // tell jack client to close audio file
      if (mainw->jackd != NULL && mainw->jackd->playing_file > 0) {
        ticks_t timeout = 0;
        if (mainw->cancelled != CANCEL_AUDIO_ERROR) {
          lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
          while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jack_get_msgq(mainw->jackd) != NULL) {
            sched_yield(); // wait for seek
            lives_usleep(prefs->sleep_time);
          }
          lives_alarm_clear(alarm_handle);
        }
        if (mainw->cancelled == CANCEL_AUDIO_ERROR) mainw->cancelled = CANCEL_ERROR;
        jack_message.command = ASERVER_CMD_FILE_CLOSE;
        jack_message.data = NULL;
        jack_message.next = NULL;
        mainw->jackd->msgq = &jack_message;
        if (timeout == 0) handle_audio_timeout();
      }
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && (mainw->pulsed != NULL || mainw->pulsed_read != NULL)) {
      // tell pulse client to close audio file
      if (mainw->pulsed != NULL) {
        if (mainw->pulsed->playing_file > 0 || mainw->pulsed->fd > 0) {
          ticks_t timeout = 0;
          if (mainw->cancelled != CANCEL_AUDIO_ERROR) {
            lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);
            while ((timeout = lives_alarm_check(alarm_handle)) > 0 && pulse_get_msgq(mainw->pulsed) != NULL) {
              sched_yield(); // wait for seek
              lives_usleep(prefs->sleep_time);
            }
            lives_alarm_clear(alarm_handle);
          }
          if (mainw->cancelled == CANCEL_AUDIO_ERROR) mainw->cancelled = CANCEL_ERROR;
          pulse_message.command = ASERVER_CMD_FILE_CLOSE;
          pulse_message.data = NULL;
          pulse_message.next = NULL;
          mainw->pulsed->msgq = &pulse_message;
          if (timeout == 0)  {
            handle_audio_timeout();
            mainw->pulsed->playing_file = -1;
            mainw->pulsed->fd = -1;
          } else {
            while (mainw->pulsed->playing_file > -1 || mainw->pulsed->fd > 0) {
              sched_yield();
              lives_usleep(prefs->sleep_time);
            }
            pulse_driver_cork(mainw->pulsed);
          }
        } else {
          pulse_driver_cork(mainw->pulsed);
        }
      }
    }
#endif
  }
}


void preview_aud_vol(void) {
  float ovol = cfile->vol;
  cfile->vol = (float)mainw->fx1_val;
  preview_audio();
  cfile->vol = ovol;
  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;
}


boolean adjust_clip_volume(int fileno, float newvol, boolean make_backup) {
  double dvol = (double)newvol;
  if (make_backup) {
    char *com = lives_strdup_printf("%s backup_audio \"%s\"", prefs->backend_sync, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      return FALSE;
    }
  }
  aud_fade(fileno, 0., CLIP_AUDIO_TIME(fileno), dvol, dvol);
  return TRUE;
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
    off_t fsize = get_file_size(mainw->aud_rec_fd);

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
        retval = do_write_failed_error_s_with_retry(outfilename, lives_strerror(errno));
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
    off_t fsize = get_file_size(mainw->aud_rec_fd);

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
/// TODO - move these to events.c

static lives_audio_track_state_t *resize_audstate(lives_audio_track_state_t *ostate, int nostate, int nstate) {
  // increase the element size of the audstate array (ostate)
  // from nostate elements to nstate elements
  lives_audio_track_state_t *audstate = (lives_audio_track_state_t *)lives_calloc(nstate, sizeof(lives_audio_track_state_t));
  int n = MIN(nostate, nstate);
  if (n > 0)
    lives_memcpy(audstate, ostate, n * sizeof(lives_audio_track_state_t));
  lives_freep((void **)&ostate);
  return audstate;
}


static lives_audio_track_state_t *aframe_to_atstate_inner(weed_plant_t *event, int *ntracks) {
  // parse an audio frame, and set the track file, seek and velocity values
  int num_aclips, atrack;
  int *aclips = NULL;
  double *aseeks = NULL;
  int naudstate = 0;
  lives_audio_track_state_t *atstate = NULL;

  register int i;

  int btoffs = mainw->multitrack ? mainw->multitrack->opts.back_audio_tracks : 1;
  num_aclips = weed_frame_event_get_audio_tracks(event, &aclips, &aseeks);
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

  lives_freep((void **)&aclips);
  lives_freep((void **)&aseeks);

  return atstate;
}


LIVES_LOCAL_INLINE lives_audio_track_state_t *aframe_to_atstate(weed_plant_t *event) {
  return aframe_to_atstate_inner(event, NULL);
}


LIVES_GLOBAL_INLINE lives_audio_track_state_t *audio_frame_to_atstate(weed_event_t *event, int *ntracks) {
  return aframe_to_atstate_inner(event, ntracks);
}


/**
   @brief get audio (and optionally video) state at timecode tc OR before event st_event

   if st_event is not NULL, we get the state just prior to it
   (state being effects and filter maps, audio tracks / positions)

   if st_event is NULL, this is a continuation, and we get the audio state only at timecode tc
   similar to quantise_events(), except we don't produce output frames
*/
lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event,
    weed_timecode_t fill_tc, int what_to_get, boolean exact) {
  // if exact is set, we must rewind back to first active stateful effect,
  // and play forwards from there (not yet implemented - TODO)
  lives_audio_track_state_t *atstate = NULL, *audstate = NULL;
  weed_timecode_t last_tc = 0;
  weed_event_t *event, *nevent;
  weed_event_t *deinit_event;
  int nfiles, nnfiles, etype;

  // gets effects state immediately prior to start_event. (initing any effects which should be active, and applying param changes
  // if not in multrack)

  // optionally: gets audio state, sets atstate[0].tc
  // and initialises audio buffers

  if (fill_tc == 0) {
    if (what_to_get != LIVES_PREVIEW_TYPE_AUDIO_ONLY)
      mainw->filter_map = NULL;
    if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY)
      mainw->afilter_map = NULL;
    event = get_first_event(event_list);
  } else {
    event = st_event;
    st_event = NULL;
    last_tc = get_event_timecode(event);
  }

  while ((st_event != NULL && event != st_event) || (st_event == NULL && get_event_timecode(event) < fill_tc)) {
    etype = weed_event_get_type(event);
    if (what_to_get == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || (etype != 1 && etype != 5)) {
      switch (etype) {
      case WEED_EVENT_HINT_FILTER_MAP:
        if (what_to_get != LIVES_PREVIEW_TYPE_AUDIO_ONLY)
          mainw->filter_map = event;
        if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY) {
          mainw->afilter_map = event;
        }
        break;
      case WEED_EVENT_HINT_FILTER_INIT:
        deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, NULL);
        if (deinit_event == NULL || get_event_timecode(deinit_event) >= fill_tc) {
          // this effect should be activated
          if (what_to_get != LIVES_PREVIEW_TYPE_AUDIO_ONLY)
            process_events(event, FALSE, get_event_timecode(event));
          if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY)
            process_events(event, TRUE, get_event_timecode(event));
          /// TODO: if exact && non-stateless, silently process audio / video until st_event
        }
        break;
      case WEED_EVENT_HINT_FILTER_DEINIT:
        if (what_to_get == LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
          weed_event_t *init_event = weed_get_voidptr_value((weed_plant_t *)event, WEED_LEAF_INIT_EVENT, NULL);
          if (get_event_timecode(init_event) >= last_tc) break;
          process_events(event, TRUE, get_event_timecode(event));
        }
        break;
      case WEED_EVENT_HINT_PARAM_CHANGE:
        if (mainw->multitrack == NULL) {
          weed_event_t *init_event = weed_get_voidptr_value((weed_plant_t *)event, WEED_LEAF_INIT_EVENT, NULL);
          deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, NULL);
          if (deinit_event != NULL && get_event_timecode(deinit_event) < fill_tc) break;

          if (weed_plant_has_leaf((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG)) {
            char *key_string = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, NULL);
            int key = atoi(key_string);
            char *filter_name = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, NULL);
            int idx = weed_get_idx_for_hashname(filter_name, TRUE);
            weed_event_t *filter = get_weed_filter(idx), *inst;
            lives_free(filter_name);
            lives_free(key_string);

            if (!is_pure_audio(filter, FALSE)) {
              if (what_to_get == LIVES_PREVIEW_TYPE_AUDIO_ONLY)
                break;
            } else {
              if (what_to_get == LIVES_PREVIEW_TYPE_VIDEO_ONLY)
                break;
            }
            if ((inst = rte_keymode_get_instance(key + 1, 0)) != NULL) {
              int pnum = weed_get_int_value(event, WEED_LEAF_INDEX, NULL);
              weed_plant_t *param = weed_inst_in_param(inst, pnum, FALSE, FALSE);
              weed_leaf_dup(param, event, WEED_LEAF_VALUE);
            }
          }
        }
        break;
      case WEED_EVENT_HINT_FRAME:
        if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_AUDIO) break;

        if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
          /// update audio state
          atstate = aframe_to_atstate(event);
          if (audstate == NULL) audstate = atstate;
          else {
            // have an existing audio state, update with current
            weed_timecode_t delta = get_event_timecode(event) - last_tc;
            for (nfiles = 0; audstate[nfiles].afile != -1; nfiles++) {
              if (delta > 0) {
                // increase seek values up to current frame
                audstate[nfiles].seek += audstate[nfiles].vel * delta / TICKS_PER_SECOND_DBL;
              }
            }
            last_tc += delta;
            for (nnfiles = 0; atstate[nnfiles].afile != -1; nnfiles++);
            if (nnfiles > nfiles) {
              audstate = resize_audstate(audstate, nfiles, nnfiles + 1);
              audstate[nnfiles].afile = -1;
            }

            for (int i = 0; i < nnfiles; i++) {
              if (atstate[i].afile > 0) {
                audstate[i].afile = atstate[i].afile;
                audstate[i].seek = atstate[i].seek;
                audstate[i].vel = atstate[i].vel;
              }
            }
            lives_free(atstate);
          }
        }
        break;
      default:
        break;
      }
    }
    nevent = get_next_event(event);
    if (nevent == NULL) break;
    event = nevent;
    if (what_to_get == LIVES_PREVIEW_TYPE_AUDIO_ONLY && WEED_EVENT_IS_AUDIO_FRAME(event)) break;
  }
  if (what_to_get == LIVES_PREVIEW_TYPE_VIDEO_AUDIO) {
    if (audstate != NULL) {
      weed_timecode_t delta = get_event_timecode(event) - last_tc;
      if (delta > 0) {
        for (nfiles = 0; audstate[nfiles].afile != -1; nfiles++) {
          // increase seek values up to current frame
          audstate[nfiles].seek += audstate[nfiles].vel * delta / TICKS_PER_SECOND_DBL;
        }
      }
    }
  }
  if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY)
    mainw->audio_event = event;
  return audstate;
}


void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, boolean exact) {
  // fill audio buffer with audio samples, using event_list as a guide
  // if st_event!=NULL, that is our start event, and we will calculate the audio state at that
  // point

  // otherwise, we continue from where we left off the last time

  // all we really do here is set from_files, aseeks and avels arrays and call render_audio_segment
  // effects are ignored here; they are applied in smaller chunks in render_audio_segment, so that parameter interpolation can be done

  lives_audio_track_state_t *atstate = NULL;
  double chvols[MAX_AUDIO_TRACKS]; // TODO - use list

  static weed_timecode_t last_tc, tc;
  static weed_timecode_t fill_tc;
  static weed_plant_t *event;
  static int nfiles;

  static int *from_files = NULL;
  static double *aseeks = NULL, *avels = NULL;

  register int i;

  if (abuf == NULL) return;

  abuf->samples_filled = 0; // write fill level of buffer
  abuf->start_sample = 0; // read level

  if (st_event != NULL) {
    // this is only called for the first buffered read
    //
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

    // get audio and fx state at pt immediately before st_event
    atstate = get_audio_and_effects_state_at(event_list, event, 0, LIVES_PREVIEW_TYPE_VIDEO_AUDIO, exact);

    if (atstate != NULL) {
      for (i = 0; atstate[i].afile != -1; i++) {
        if (atstate[i].afile > 0) {
          from_files[i] = atstate[i].afile;
          avels[i] = atstate[i].vel;
          aseeks[i] = atstate[i].seek;
        }
      }
      lives_free(atstate);
    }
  }

  if (mainw->multitrack != NULL) {
    // get channel volumes from the mixer
    for (i = 0; i < nfiles; i++) {
      if (mainw->multitrack != NULL && mainw->multitrack->audio_vols != NULL) {
        chvols[i] = (double)LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->multitrack->audio_vols, i)) / ONE_MILLION_DBL;
      }
    }
  } else chvols[0] = 1.;

  fill_tc = last_tc + fabs((double)(abuf->samp_space) / (double)abuf->arate * TICKS_PER_SECOND_DBL);

  // continue until we have a full buffer
  // if we get an audio frame we render up to that point
  // then we render what is left to fill abuf
  while (event != NULL && (tc = get_event_timecode(event)) < fill_tc) {
    if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
      // got next audio frame
      render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, tc, chvols, 1., 1., abuf);

      /* for (i = 0; i < nfiles; i++) { */
      /*   // increase seek values */
      /*   aseeks[i] += avels[i] * (tc - last_tc) / TICKS_PER_SECOND_DBL; */
      /* } */

      last_tc = tc;
      // process audio updates at this frame
      atstate = aframe_to_atstate(event);

      if (atstate != NULL) {
        int nnfiles;
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
    event = get_next_audio_frame_event(event);
  }

  if (last_tc < fill_tc) {
    // fill the rest of the buffer
    render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, fill_tc, chvols, 1., 1., abuf);
    /* for (i = 0; i < nfiles; i++) { */
    /*   // increase seek values */
    /*   aseeks[i] += avels[i] * (fill_tc - last_tc) / TICKS_PER_SECOND_DBL; */
    /* } */
  }

  if (THREADVAR(read_failed) > 0) {
    THREADVAR(read_failed) = 0;
    do_read_failed_error_s(THREADVAR(read_failed_file), NULL);
  }

  mainw->write_abuf++;
  if (mainw->write_abuf >= prefs->num_rtaudiobufs) mainw->write_abuf = 0;

  last_tc = fill_tc;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->abufs_to_fill > 0) {
    mainw->abufs_to_fill--;
  }
  pthread_mutex_unlock(&mainw->abuf_mutex);
}


void init_jack_audio_buffers(int achans, int arate, boolean exact) {
#ifdef ENABLE_JACK

  int chan;

  mainw->jackd->abufs = (lives_audio_buf_t **)lives_calloc(prefs->num_rtaudiobufs, sizeof(lives_audio_buf_t *));

  for (int i = 0; i < prefs->num_rtaudiobufs; i++) {
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
  mainw->pulsed->abufs = (lives_audio_buf_t **)lives_calloc(prefs->num_rtaudiobufs, sizeof(lives_audio_buf_t *));

  for (int i = 0; i < prefs->num_rtaudiobufs; i++) {
    mainw->pulsed->abufs[i] = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));

    mainw->pulsed->abufs[i]->out_achans = achans;
    mainw->pulsed->abufs[i]->arate = arate;
    mainw->pulsed->abufs[i]->start_sample = 0;
    mainw->pulsed->abufs[i]->samp_space = XSAMPLES / prefs->num_rtaudiobufs; // samp_space here is in stereo samples
    mainw->pulsed->abufs[i]->buffer16 = (short **)lives_calloc(1, sizeof(short *));
    mainw->pulsed->abufs[i]->buffer16[0] = (short *)lives_calloc_safety(XSAMPLES / prefs->num_rtaudiobufs, achans * sizeof(short));
  }
#endif
}


void free_jack_audio_buffers(void) {
#ifdef ENABLE_JACK
  int chan;

  if (mainw->jackd == NULL) return;
  if (mainw->jackd->abufs == NULL) return;

  for (int i = 0; i < prefs->num_rtaudiobufs; i++) {
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

  if (mainw->pulsed == NULL) return;
  if (mainw->pulsed->abufs == NULL) return;

  for (int i = 0; i < prefs->num_rtaudiobufs; i++) {
    if (mainw->pulsed->abufs[i] != NULL) {
      lives_free(mainw->pulsed->abufs[i]->buffer16[0]);
      lives_free(mainw->pulsed->abufs[i]->buffer16);
      lives_free(mainw->pulsed->abufs[i]);
    }
  }
  lives_free(mainw->pulsed->abufs);
#endif
}


LIVES_GLOBAL_INLINE void avsync_force(void) {
#ifdef RESEEK_ENABLE
  /// force realignment of video and audio at current file->frameno / player->seek_pos
  if (mainw->foreign || !LIVES_IS_PLAYING || prefs->audio_src == AUDIO_SRC_EXT || prefs->force_system_clock
      || (mainw->event_list && !(mainw->record || mainw->record_paused)) || prefs->audio_player == AUD_PLAYER_NONE
      || !is_realtime_aplayer(prefs->audio_player)) {
    mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    return;
  }

  if (!pthread_mutex_trylock(&mainw->avseek_mutex)) {
    if (mainw->audio_seek_ready && mainw->video_seek_ready) {
      mainw->video_seek_ready = mainw->audio_seek_ready = FALSE;
      mainw->force_show = TRUE;
      mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);
    }
    pthread_mutex_unlock(&mainw->avseek_mutex);
  }
#endif
}


/**
   @brief resync audio playback to the current video frame

   if we are using a realtime audio player, resync to frameno
   and return TRUE

   otherwise return FALSE

   this is called internally - for example when the play position jumps, either due
   to external transport changes, (e.g. jack transport, osc retrigger / goto)
   or if we are looping a video selection, or it may be triggered from the keyboard

   this is only active if "audio follows video rate/fps changes" is set
   and various other conditions are met.
*/
boolean resync_audio(double frameno) {
  if (!(prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) return FALSE;
  // if recording external audio, we are intrinsically in sync
  if (mainw->record && prefs->audio_src == AUDIO_SRC_EXT) return TRUE;

  // if we are playing an audio generator or an event_list, then resync is meaningless
  if ((mainw->event_list && !mainw->multitrack && !mainw->record && !mainw->record_paused)
      || mainw->agen_key != 0 || mainw->agen_needs_reinit) return FALSE;

  // also can't resync if the playing file has no audio, or prefs dont allow it
  if (cfile->achans == 0
      || (0
#ifdef HAVE_PULSE_AUDIO
          || (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL
              && mainw->current_file != mainw->pulsed->playing_file)
#endif
          ||
#ifdef ENABLE_JACK
          (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL
           && mainw->current_file != mainw->jackd->playing_file) ||
#endif
          0))
    return FALSE;

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL) {
    if (!jack_audio_seek_frame(mainw->jackd, frameno)) {
      if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd, frameno);
      else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    }
    if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && prefs->audio_src == AUDIO_SRC_INT) {
      jack_get_rec_avals(mainw->jackd);
    }
    return TRUE;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
    /* if (mainw->files[mainw->pulsed->playing_file]->pb_fps != 0.) */
    /*   frameno += (double)(mainw->startticks - mainw->currticks) / TICKS_PER_SECOND_DBL */
    /* 	/ mainw->files[mainw->pulsed->playing_file]->pb_fps; */
    if (!pulse_audio_seek_frame(mainw->pulsed, frameno)) {
      if (pulse_try_reconnect()) pulse_audio_seek_frame(mainw->pulsed, frameno);
      else mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    }
    mainw->startticks = mainw->currticks = lives_get_current_playback_ticks(mainw->origsecs, mainw->orignsecs, NULL);

    if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && prefs->audio_src == AUDIO_SRC_INT) {
      pulse_get_rec_avals(mainw->pulsed);
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


/**
   @brief audio caching worker thread function

   run as a thread (from audio_cache_init())

   read audio from file into cache
   must be done in real time since other threads may be waiting on the cache

   during free playback, this is only used by jack (thus far it has proven too complex to implement for pulse, since it uses
   variable sized buffers.

   This function is also used to cache audio when playing from an event_list. Effects may be applied and the mixer volumes
   adjusted.
*/
static void *cache_my_audio(void *arg) {
  lives_audio_buf_t *cbuffer = (lives_audio_buf_t *)arg;
  char *filename;
  register int i;

  while (!cbuffer->die) {
    // wait for request from client (setting cbuffer->is_ready or cbuffer->die)
    while (cbuffer->is_ready && !cbuffer->die && mainw->abufs_to_fill <= 0) {
      pthread_mutex_lock(&cond_mutex);
      pthread_cond_wait(&cond, &cond_mutex);
      pthread_mutex_unlock(&cond_mutex);
    }

    if (cbuffer->die) {
      if (mainw->event_list == NULL
          || (mainw->record || (mainw->is_rendering && mainw->preview && !mainw->preview_rendering))) {
        if (cbuffer->_fd != -1)
          lives_close_buffered(cbuffer->_fd);
      }
      return cbuffer;
    }

    // read from file and process data
    //lives_printerr("got buffer request !\n");

    ////  for multitrack:

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

    //// for jack audio, free playback

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

      // 16 bit buffers follow in out_achans but in_interleaf

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

      filename = get_audio_file_name(cbuffer->fileno, afile->opening);

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

    if (cbuffer->fileno != cbuffer->_cfileno || cbuffer->seek != cbuffer->_cseek ||
        cbuffer->shrink_factor != cbuffer->_shrink_factor) {
      if (cbuffer->sequential || cbuffer->shrink_factor > 0.) {
        lives_buffered_rdonly_set_reversed(cbuffer->_fd, FALSE);
      } else {
        lives_buffered_rdonly_set_reversed(cbuffer->_fd, TRUE);
      }
      if (cbuffer->fileno != cbuffer->_cfileno || cbuffer->seek != cbuffer->_cseek) {
        lives_lseek_buffered_rdonly_absolute(cbuffer->_fd, cbuffer->seek);
      }
    }

    cbuffer->_cfileno = cbuffer->fileno;
    cbuffer->_shrink_factor = cbuffer->shrink_factor;

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
      if (cbuffer->shrink_factor < 0.) {
        if (reverse_buffer(cbuffer->_filebuffer, cbuffer->bytesize, cbuffer->in_achans))
          cbuffer->shrink_factor = -cbuffer->shrink_factor;
      }
      sample_move_d8_d16(cbuffer->buffer16[0], (uint8_t *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize,
                         cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans, 0);
    }
    // 16 bit input samples
    // resample as we go
    else {
      if (cbuffer->shrink_factor < 0.) {
        if (reverse_buffer(cbuffer->_filebuffer, cbuffer->bytesize, cbuffer->in_achans * 2))
          cbuffer->shrink_factor = -cbuffer->shrink_factor;
      }
      sample_move_d16_d16(cbuffer->buffer16[0], (short *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize,
                          cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans,
                          cbuffer->swap_endian ? SWAP_X_TO_L : 0, 0);
    }
    cbuffer->shrink_factor = cbuffer->_shrink_factor;

    // if our out_asamps is 16, we are done

    cbuffer->is_ready = TRUE;
  }
  return cbuffer;
}


void wake_audio_thread(void) {
  cache_buffer->is_ready = FALSE;
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
    cache_buffer->_shrink_factor = 0.;
  }

  // init the audio caching thread for rt playback
  pthread_create(&athread, NULL, cache_my_audio, cache_buffer);

  return cache_buffer;
}


void audio_cache_end(void) {
  lives_audio_buf_t *xcache_buffer;
  register int i;

  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  if (cache_buffer == NULL) {
    pthread_mutex_unlock(&mainw->cache_buffer_mutex);
    return;
  }
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

  if (cache_buffer->_fd != -1) lives_close_buffered(cache_buffer->_fd);

  // make this threadsafe (kind of)
  xcache_buffer = cache_buffer;
  cache_buffer = NULL;
  lives_free(xcache_buffer);
}


LIVES_GLOBAL_INLINE lives_audio_buf_t *audio_cache_get_buffer(void) {
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
  weed_timecode_t tc;
  weed_error_t retval;
  int flags, cflags;
  int xnchans = 0, xxnchans, xrate = 0;
  boolean rvary = FALSE, lvary = FALSE;

  if (mainw->agen_needs_reinit) {
    weed_instance_unref(inst);
    return FALSE; // wait for other thread to reinit us
  }
  tc = (double)mainw->agen_samps_count / (double)arate * TICKS_PER_SECOND_DBL;
  filter = weed_instance_get_filter(inst, FALSE);
  flags = weed_filter_get_flags(filter);

  if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
  if (flags & WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY) lvary = TRUE;

getaud1:

  channel = get_enabled_channel(inst, 0, FALSE);
  if (channel != NULL) {
    xnchans = nchans; // preferred value
    ctmpl = weed_channel_get_template(channel);
    cflags = weed_chantmpl_get_flags(ctmpl);

    if (lvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_CHANNELS))
      xnchans = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_CHANNELS, NULL);
    else if (weed_plant_has_leaf(filter, WEED_LEAF_MAX_AUDIO_CHANNELS)) {
      xxnchans = weed_get_int_value(filter, WEED_LEAF_MAX_AUDIO_CHANNELS, NULL);
      if (xxnchans > 0 && xxnchans < nchans) xnchans = xxnchans;
    }
    if (xnchans > nchans) {
      weed_instance_unref(orig_inst);
      return FALSE;
    }
    if (weed_get_int_value(channel, WEED_LEAF_AUDIO_CHANNELS, NULL) != nchans
        && (cflags & WEED_CHANNEL_REINIT_ON_LAYOUT_CHANGE))
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
      if (cflags & WEED_CHANNEL_REINIT_ON_RATE_CHANGE) {
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
    com = lives_strdup_printf("%s backup_audio %s", prefs->backend_sync, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);

    if (THREADVAR(com_failed)) {
      d_print_failed();
      return FALSE;
    }
  }

  audio_pos = (double)((cfile->start - 1) * cfile->arate * cfile->achans * cfile->asampsize / 8) / cfile->fps;
  audio_file = lives_get_audio_file_name(mainw->current_file);

  audio_fd = lives_open_buffered_writer(audio_file, DEF_FILE_PERMS, TRUE);

  if (audio_fd == -1) return FALSE;

  if (audio_pos > cfile->afilesize) {
    off64_t audio_end_pos = (double)((cfile->start - 1) * cfile->arate * cfile->achans * cfile->asampsize / 8) / cfile->fps;
    pad_with_silence(audio_fd, NULL, audio_pos, audio_end_pos, cfile->asampsize, cfile->signed_endian & AFORM_UNSIGNED,
                     cfile->signed_endian & AFORM_BIG_ENDIAN);
  } else lives_lseek_buffered_writer(audio_fd, audio_pos);

  aud_tc = 0;

  return TRUE;
}


void apply_rte_audio_end(boolean del) {
  lives_close_buffered(audio_fd);
  if (del) lives_rm(audio_file);
  lives_free(audio_file);
}


boolean apply_rte_audio(int64_t nframes) {
  // CALLED When we are rendering audio to a file

  // - read nframes from clip or generator
  // - convert to float if necessary
  // - send to rte audio effects
  // - convert back to s16 or s8
  // - save to audio_fd
  weed_layer_t *layer;
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

    tbytes = lives_read_buffered(audio_fd, in_buff, tbytes, FALSE);

    if (THREADVAR(read_failed) == audio_fd + 1) {
      THREADVAR(read_failed) = 0;
      do_read_failed_error_s(audio_file, NULL);
      lives_freep((void **)&THREADVAR(read_failed_file));
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
                                               lives_vol_from_linear(cfile->vol));
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
    lives_lseek_buffered_writer(audio_fd, audio_pos);
    tbytes = onframes * cfile->achans * cfile->asampsize / 8;
    lives_write_buffered(audio_fd, (const char *)in_buff, tbytes, FALSE);
    audio_pos += tbytes;
  }

  if (shortbuf != (short *)in_buff) lives_free(shortbuf);
  lives_free(in_buff);

  if (THREADVAR(write_failed) == audio_fd + 1) {
    THREADVAR(write_failed) = 0;
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
   we dont hang the player during read). When the first client reads, the buffers are swapped.

   if player is jack, we will have non-interleaved float, if player is pulse, we will have interleaved S16 so we have to convert to float and
   then back again.

   (this is currently only used to push audio to generators, since there are currently no filters which allow both video and audio input)
*/
boolean push_audio_to_channel(weed_plant_t *filter, weed_plant_t *achan, lives_audio_buf_t *abuf) {
  float **dst, *src;

  weed_plant_t *ctmpl;

  float scale;

  size_t samps, offs = 0;
  boolean rvary = FALSE, lvary = FALSE;
  int trate, tchans, xnchans, flags;
  int alen;

  register int i;
  if (abuf->samples_filled == 0) {
    weed_layer_set_audio_data(achan, NULL, 0, 0, 0);
    return FALSE;
  }

  samps = abuf->samples_filled;
  //if (abuf->in_interleaf) samps /= abuf->in_achans;

  ctmpl = weed_get_plantptr_value(achan, WEED_LEAF_TEMPLATE, NULL);

  flags = weed_get_int_value(filter, WEED_LEAF_FLAGS, NULL);
  if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
  if (flags & WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY) lvary = TRUE;

  if (!has_audio_chans_out(filter, FALSE)) {
    int maxlen = weed_chantmpl_get_max_audio_length(ctmpl);
    if (abuf->in_interleaf) maxlen *= abuf->in_achans;
    if (maxlen < samps) {
      offs = samps - maxlen;
      samps = maxlen;
    }
  }

  // TODO: can be list
  if (rvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_RATE))
    trate = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_RATE, NULL);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_AUDIO_RATE))
    trate = weed_get_int_value(filter, WEED_LEAF_AUDIO_RATE, NULL);
  else trate = DEFAULT_AUDIO_RATE;

  tchans = DEFAULT_AUDIO_CHANS;
  if (lvary && weed_plant_has_leaf(ctmpl, WEED_LEAF_AUDIO_CHANNELS))
    tchans = weed_get_int_value(ctmpl, WEED_LEAF_AUDIO_CHANNELS, NULL);
  else if (weed_plant_has_leaf(filter, WEED_LEAF_MAX_AUDIO_CHANNELS)) {
    xnchans = weed_get_int_value(filter, WEED_LEAF_MAX_AUDIO_CHANNELS, NULL);
    if (xnchans > 0 && xnchans < tchans) tchans = xnchans;
  }

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
        abuf->buffer16[i] = (short *)lives_calloc_safety(samps, sizeof(short));
        sample_move_d8_d16(abuf->buffer16[i], &abuf->buffer8[i][offs], samps, samps * sizeof(short),
                           1.0, abuf->out_achans, abuf->out_achans, swap);

      }
    }

    // try convert S16 -> float
    if (abuf->buffer16 != NULL) {
      abuf->bufferf = (float **)lives_calloc(abuf->out_achans, sizeof(float *));
      for (i = 0; i < abuf->out_achans; i++) {
        if (!abuf->in_interleaf) {
          abuf->bufferf[i] = (float *)lives_calloc_safety(abuf->samples_filled, sizeof(float));
          sample_move_d16_float(abuf->bufferf[i], &abuf->buffer16[0][offs], samps, 1,
                                (abuf->s16_signed ? AFORM_SIGNED : AFORM_UNSIGNED), abuf->swap_endian, lives_vol_from_linear(cfile->vol));
          offs += abuf->samples_filled;
        } else {
          abuf->bufferf[i] = (float *)lives_calloc_safety(samps / abuf->out_achans, sizeof(float));
          sample_move_d16_float(abuf->bufferf[i], &abuf->buffer16[0][i], samps / abuf->in_achans, abuf->in_achans,
                                (abuf->s16_signed ? AFORM_SIGNED : AFORM_UNSIGNED), abuf->swap_endian, lives_vol_from_linear(cfile->vol));
        }
      }
      abuf->out_interleaf = FALSE;
    }
  }

  if (abuf->bufferf == NULL) return FALSE;
  // now we should have float

  if (abuf->in_interleaf) samps /= abuf->in_achans;

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
  com = lives_strdup_printf("%s play %d \"%s\" \"%s\" %d", astreamer, mainw->vpp->audio_codec, astream_name, astream_name_out,
                            arate);
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
  char *msg2 = (prefs->audio_player == AUD_PLAYER_PULSE) ? lives_strdup(
                 _("\nClick Retry to attempt to restart the audio server.\n")) :
               lives_strdup("");

  char *msg = lives_strdup_printf(
                _("LiVES was unable to connect to %s.\nPlease check your audio settings and restart %s\nand LiVES if necessary.\n%s"),
                audio_player_get_display_name(prefs->aplayer),
                audio_player_get_display_name(prefs->aplayer), msg2);
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    int retval = do_abort_cancel_retry_dialog(msg);
    if (retval == LIVES_RESPONSE_RETRY) pulse_try_reconnect();
    else {
      mainw->aplayer_broken = TRUE;
      switch_aud_to_none(FALSE);
      mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
    }
#endif
  } else {
    do_error_dialog(msg);
    mainw->aplayer_broken = TRUE;
    switch_aud_to_none(FALSE);
    mainw->video_seek_ready = mainw->audio_seek_ready = TRUE;
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
    mainw->nullaudio_seek_posn += ((int64_t)((double)(current_ticks - mainw->nullaudio_start_ticks) / USEC_TO_TICKS / ONE_MILLION. *
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
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
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
