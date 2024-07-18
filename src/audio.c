// audio.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2021
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "callbacks.h"
#include "effects.h"
#include "resample.h"
#include "threading.h"

static char *storedfnames[NSTOREDFDS];
static int storedfds[NSTOREDFDS];
static boolean storedfdsset = FALSE;
static const int std_arates[] =
{8000, 11025, 22050, 32000, 44100, 48000, 88200, 96000, 128000, 256000, 0};

static arec_details *rec_ext_dets = NULL;

#if HAVE_SWRESAMPLE
#include <libswresample/swresample.h>
//int in_asamps, boolean in_inter,
//, int out_asamps, boolean out_inter) {

static struct SwrContext *smbuf_ctx = NULL;

// forwards -> calc # in samps for at least given out size (out size can be adjusted up)
// backwards -> calc # out samps for up to given in size (in size can be adjusted down)
int get_swr_fmts(struct SwrContext **ctxp, int out_asampsz, int out_arate, int out_achans, int out_inter, int *out_nsamps,
                 int in_asampsz, int in_arate, int in_achans, int in_inter, enum AVSampleFormat *outfmt, enum AVSampleFormat *infmt,
                 boolean reversed) {
  if (!ctxp) return -1;

  struct SwrContext *swr_ctx = *ctxp;
  int insamps = av_rescale_rnd(*out_nsamps, in_arate, out_arate, reversed ? AV_ROUND_UP : AV_ROUND_DOWN);
  int outsamps = av_rescale_rnd(insamps, out_arate, in_arate, reversed ? AV_ROUND_DOWN : AV_ROUND_UP);

  if (infmt && outfmt) {
    *infmt = av_sample_format(in_asampsz, in_inter);
    *outfmt = av_sample_format(out_asampsz, out_inter);

    if (!swr_ctx) {
      swr_ctx = swr_alloc();
      if (!swr_ctx) return -2;
      swr_ctx = swr_alloc_set_opts(swr_ctx, out_achans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO,
                                   *outfmt, out_arate, in_achans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO,
                                   *infmt, in_arate, 0, 0);
      /* g_print("chck in %d %d %d %d %d\n", out_asampsz, out_arate, out_achans, out_inter, *out_nsamps); */
      /* g_print("chck2 in %d %d %d %d %d %d\n", in_asampsz, in_arate, in_achans, in_inter, *outfmt, *infmt); */

      if (swr_init(swr_ctx) < 0) return -3;
      *ctxp = swr_ctx;
    }
  }


  if (!swr_ctx) return -4;

  if (reversed) {
    struct SwrContext *rev_ctx = swr_alloc();
    if (!rev_ctx) return -2;
    rev_ctx = swr_alloc_set_opts(rev_ctx, out_achans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO,
                                 *outfmt, out_arate, in_achans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO,
                                 *infmt, in_arate, 0, 0);
    if (swr_init(rev_ctx) < 0) return -3;
    insamps = swr_get_out_samples(rev_ctx, outsamps);
    swr_free(&rev_ctx);
    return insamps;
  }

  while (1) {
    outsamps = swr_get_out_samples(swr_ctx, insamps);
    if (outsamps >= *out_nsamps) break;
    insamps++;
  }

  /* g_print("SWR chck %d %d %d %d %d\n", */
  /* 	  *infmt, *outfmt, *out_nsamps, outsamps, insamps); */

  //*out_nsamps = outsamps;
  return insamps;
}


int sw_resample(void **out_data, int out_samps_per_chan,
                void **in_data, int in_samps_per_chan,
                struct SwrContext *swr_ctx) {
  / g_print("SWR convert %p %p %p   %p %p %p %d -> %d\n",
            in_data, in_data[0], in_data[1], out_data, out_data[0], out_data[1], in_samps_per_chan, out_samps_per_chan);

  int ret = swr_convert(swr_ctx, (uint8_t **)out_data, out_samps_per_chan,
                        (const uint8_t **)in_data, in_samps_per_chan);
  if (swr_ctx != smbuf_ctx) swr_free(&swr_ctx);

  if (ret < 0) {
    fprintf(stderr, "Error while converting audio\n");
    return ret;
  }

  //g_print("got in %d out %d vs %d\n", in_samps_per_chan, ret, out_samps_per_chan);

  //  LIVES_ASSERT(ret == out_samps_per_chan);

  return ret;
}

#endif

LIVES_GLOBAL_INLINE lives_obj_instance_t *get_aplayer_instance(int source) {
  lives_obj_instance_t *aplayer = NULL;
  if (source == AUDIO_SRC_EXT) {
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed_read)
      aplayer = mainw->pulsed_read->inst;
#endif
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd_read)
      aplayer = mainw->jackd_read->inst;
#endif
  } else {
#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed)
      aplayer = mainw->pulsed->inst;
#endif
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd)
      aplayer = mainw->jackd->inst;
#endif
  }
  return aplayer;
}


LiVESList *get_std_arates(void) {
  LiVESList *list = NULL;
  char *str;
  for (int i = 0; std_arates[i]; i++) {
    str = lives_strdup_printf("%d", std_arates[i]);
    list = lives_list_append(list, str);
  }
  return list;
}

int find_standard_arate(int rate) {
  int mindist, minval = 0, dist, arate;
  if (rate) {
    for (int i = 0; std_arates[i]; i++) {
      arate = std_arates[i];
      dist = abs(arate - rate);
      if (!minval || dist <= mindist) {
        mindist = dist;
        minval = arate;
      }
      if (arate > rate) break;
    }
  }
  return minval;
}


LIVES_LOCAL_INLINE void audio_reset_stored_fnames(void) {
  for (int i = 0; i < NSTOREDFDS; i++) {
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
    if (IS_VALID_CLIP(fnum)) {
      char *clipdir = get_clip_dir(fnum);
      fname = lives_build_filename(clipdir, CLIP_AUDIO_FILENAME, NULL);
      lives_free(clipdir);
    } else fname = lives_build_filename(prefs->workdir, CLIP_AUDIO_FILENAME, NULL);
  } else {
    if (IS_VALID_CLIP(fnum)) {
      char *clipdir = get_clip_dir(fnum);
      fname = lives_build_filename(clipdir, CLIP_TEMP_AUDIO_FILENAME, NULL);
      lives_free(clipdir);
    } else fname = lives_build_filename(prefs->workdir, CLIP_TEMP_AUDIO_FILENAME, NULL);
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


static int arena_write(void *dst, void *src, int offset, int nsamples, int sampsize) {
  int space = (ABUF_ARENA_SIZE >> 2) - offset;
  if (space > nsamples) space = nsamples;

  lives_memcpy(dst + (off_t)(offset * sampsize), src, (size_t)(space * sampsize));
  nsamples -= space;
  offset += space;

  if (nsamples) {
    lives_memcpy(dst, src + (off_t)(space * sampsize), (size_t)(nsamples * sampsize));
    offset = nsamples;
  }
  return offset;
}


static int arena_read(void *dst, void *src, lives_audio_buf_t *abuf, int offset, int nsamples, int sampsize) {
  int space;
  if (!abuf) return 0;
  if (abuf->write_pos > offset) space = abuf->write_pos - offset;
  else space = nsamples;

  if (space > (ABUF_ARENA_SIZE >> 2) - offset) space = (ABUF_ARENA_SIZE >> 2) - offset;

  lives_memcpy(dst, src + (off_t)(offset * sampsize), (size_t)(space * sampsize));
  nsamples -= space;
  offset += space;
  if (nsamples) {
    lives_memcpy(dst + (off_t)(space * sampsize), src, (size_t)(nsamples * sampsize));
    offset = nsamples;
  }
  return offset;
}


static void append_to_audio_bufferf(float *src, int nsamples, lives_audio_buf_t *abuf, int arate, int channum) {
  // append float audio to the audio frame buffer
  // this needs to be done in descending channel order
  int write_offset;
  if (!abuf) return;

  channum++;

  if (!abuf->bufferf || channum > abuf->in_achans) {
    if (!abuf->bufferf) {
      abuf->write_pos = 0;
      abuf->bufferf = LIVES_CALLOC_SIZEOF(float *, channum);
      abuf->arate = arate;
    } else abuf->bufferf = (float **)lives_recalloc(abuf->bufferf, channum, abuf->out_achans, sizeof(float *));
    abuf->in_achans = channum;
  }

  channum--;

  if (!abuf->bufferf[channum])
    abuf->bufferf[channum] = (float *)lives_calloc_mapped(ABUF_ARENA_SIZE, TRUE);
  write_offset = arena_write(abuf->bufferf[channum], src, abuf->write_pos, nsamples, 4);
  // only update when all channels written
  if (!channum) {
    abuf->write_pos = write_offset;
    //g_print("WPOS is now %d\n", write_offset);
  }
}


lives_audio_buf_t *init_audio_frame_buffers(lives_obj_instance_t *aplayer) {
  // function should be called when the first video generator with audio input is enabled
  // (or audio player needing external audio)
  LIVES_CALLOC_TYPE(lives_audio_buf_t, abuf, 1);
  int nchans = lives_aplayer_get_achans(aplayer);
  abuf->in_asamps = 32;
  abuf->in_interleaf = abuf->out_interleaf = FALSE;
  abuf->in_achans = nchans;
  abuf->arate = DEFAULT_AUDIO_RATE;

  pthread_mutex_init(&abuf->nreader_mutex, NULL);
#ifdef DEBUG_AFB
  g_print("init afb\n");
#endif
  return abuf;
}


void free_audio_frame_buffer(lives_audio_buf_t *abuf) {
  // function should be called to clear samples
  // cannot use lives_freep, as abuf is a weak pointer
  int i;
  if (abuf) {
    if (abuf->bufferf) {
      for (i = 0; i < abuf->out_achans; i++) lives_uncalloc_mapped(abuf->bufferf[i], ABUF_ARENA_SIZE, TRUE);
      lives_free(abuf->bufferf);
      abuf->bufferf = NULL;
    }

    /* if (abuf->buffer32) { */
    /*   for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer32[i]); */
    /*   lives_free(abuf->buffer32); */
    /*   abuf->buffer32 = NULL; */
    /* } */
    /* if (abuf->buffer24) { */
    /*   for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer24[i]); */
    /*   lives_free(abuf->buffer24); */
    /*   abuf->buffer24 = NULL; */
    /* } */
    if (abuf->buffer16) {
      //for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer16[i]);
      for (i = 0; i < 1; i++) lives_free(abuf->buffer16[i]);
      lives_free(abuf->buffer16);
      abuf->buffer16 = NULL;
    }
    /* if (abuf->buffer8) { */
    /*   for (i = 0; i < abuf->out_achans; i++) lives_free(abuf->buffer8[i]); */
    /*   lives_free(abuf->buffer8); */
    /*   abuf->buffer8 = NULL; */
    /* } */

    abuf->samples_filled = 0;
    abuf->in_achans = 0;
    abuf->start_sample = 0;
    abuf->write_pos = 0;
    abuf->readers = 0;
  }
#ifdef DEBUG_AFB
  g_print("clear afb %p\n", abuf);
#endif
}


double audiofile_get_silent(int fnum, double start, double end, int dir, float thresh) {
  if (!IS_NORMAL_CLIP(fnum) || !mainw->files[fnum]->achans || start >= mainw->files[fnum]->laudio_time) return -1.;
  else {
    double atime;
    lives_clip_t *afile = mainw->files[fnum];
    char *filename = lives_get_audio_file_name(fnum);
    int afd = lives_open_buffered_rdonly(filename);
    float xf;
    int c, count = 0;
    lives_free(filename);
    if (end == 0. || end > afile->laudio_time) end = afile->laudio_time;
    if (dir == LIVES_DIRECTION_FORWARD) atime = start;
    else atime = end;
    while ((dir == LIVES_DIRECTION_FORWARD && atime <= end)
           || (dir == LIVES_DIRECTION_BACKWARD && atime >= start)) {
      for (c = 0; c < afile->achans; c++) {
        if (afd == -1) {
          THREADVAR(read_failed) = -2;
          return -1.;
        }
        xf = fabsf(get_float_audio_val_at_time(fnum, afd, atime, c, afile->achans));
        if (xf > thresh) {
          lives_close_buffered(afd);
          return atime;
        }
      }
      if (dir == LIVES_DIRECTION_FORWARD)
        atime += 1. / afile->arps;
      else
        atime -= 1. / afile->arps;
      if (count == afile->arps) count = 0;
      if (!count++) threaded_dialog_spin((atime - start) / 2. / (end - start));
    }
    lives_close_buffered(afd);
    return atime;
  }
}


float audiofile_get_maxvol(int fnum, double start, double end, float thresh) {
  if (!IS_NORMAL_CLIP(fnum) || !mainw->files[fnum]->achans || start >= mainw->files[fnum]->laudio_time) return -1.;
  else {
    double atime = start;
    lives_clip_t *afile = mainw->files[fnum];
    char *filename = lives_get_audio_file_name(fnum);
    int afd = lives_open_buffered_rdonly(filename);
    float xx = 0., xf;
    int c, count = 0;
    lives_free(filename);
    if (end == 0. || end > afile->laudio_time) end = afile->laudio_time;
    while (atime <= end) {
      for (c = 0; c < afile->achans; c++) {
        if (afd == -1) {
          THREADVAR(read_failed) = -2;
          return -1.;
        }
        xf = fabsf(get_float_audio_val_at_time(fnum, afd, atime, c, afile->achans));
        if (xf > xx) xx = xf;
        if (thresh >= 0. && xx > thresh) {
          lives_close_buffered(afd);
          return xx;
        }
      }
      atime += 1. / afile->arps;
      if (count == afile->arps) count = 0;
      if (!count++) threaded_dialog_spin((atime - start) / 2. / (end - start));
    }
    lives_close_buffered(afd);
    return xx;
  }
}


boolean normalise_audio(int fnum, double start, double end, float thresh) {
  if (!IS_NORMAL_CLIP(fnum)) return FALSE;
  else {
    float xx = audiofile_get_maxvol(fnum, start, end, -1.);
    if (xx <= 0.) return FALSE;
    if (1) {
      lives_clip_t *afile = mainw->files[fnum];
      double atime = start;
      float fact = thresh / xx, val;
      char *filename = lives_get_audio_file_name(fnum);
      boolean xsigned = !(afile->signed_endian & AFORM_UNSIGNED);
      boolean swap_endian = FALSE;
      int afd, afd2;
      int c, count = 0;

      THREADVAR(read_failed) = THREADVAR(write_failed) = 0;
      threaded_dialog_spin(0.);

      afd = lives_open_buffered_rdonly(filename);
      if (afd == -1) {
        lives_free(filename);
        THREADVAR(read_failed) = -2;
        return FALSE;
      }
      afd2 = lives_open_buffered_writer(filename, capable->umask, TRUE);
      if (afd2 == -1) {
        lives_close_buffered(afd);
        lives_free(filename);
        THREADVAR(write_failed) = -2;
        return FALSE;
      }
      lives_free(filename);

      if (((afile->signed_endian & AFORM_BIG_ENDIAN) && capable->hw.byte_order == LIVES_LITTLE_ENDIAN)
          || ((afile->signed_endian & AFORM_LITTLE_ENDIAN) && capable->hw.byte_order == LIVES_BIG_ENDIAN))
        swap_endian = TRUE;

      lives_lseek_buffered_writer(afd2, quant_abytes(start, afile->arps, afile->achans, afile->asampsize));

      if (end == 0. || end > afile->laudio_time) end = afile->laudio_time;
      while (atime <= end) {
        if (mainw->cancelled != CANCEL_NONE) break;
        if (count == afile->arps) count = 0;
        if (!count++) threaded_dialog_spin(.5 + (atime - start) / 2. / (end - start));
        for (c = 0; c < afile->achans; c++) {
          xx = get_float_audio_val_at_time(fnum, afd, atime, c, afile->achans) * fact;
          if (THREADVAR(read_failed)) break;
          if (afile->asampsize == 8) {
            if (!xsigned) {
              uint8_t ucval;
              val = xx * 127.4999 + 127.4999;
              ucval = (uint8_t)(127.4999 * xx) + 127.4999;
              lives_write_buffered(afd2, (const char *)&ucval, 1, FALSE);
            } else {
              char scval;
              val = xx * 255.499;
              scval = (char)(255.499 * xx - 128.);
              lives_write_buffered(afd2, &scval, 1, FALSE);
            }
          } else {
            if (!xsigned) {
              uint16_t usval;
              val = xx * SAMPLE_MAX_16BIT_P + SAMPLE_MAX_16BIT_P;
              usval = (val > 65535 ? 65535 : val < 0 ? 0 : val);
              if (swap_endian) swab2(&usval, &usval, 1);
              lives_write_buffered(afd2, (const char *)&usval, 2, FALSE);
            } else {
              float val = xx * SAMPLE_MAX_16BIT_P;
              int16_t ssval = (int16_t)(val > SAMPLE_MAX_16BIT_P ? SAMPLE_MAX_16BIT_P
                                        : val < -SAMPLE_MAX_16BIT_N ? -SAMPLE_MAX_16BIT_N : val);
              if (swap_endian) swab2(&ssval, &ssval, 1);
              lives_write_buffered(afd2, (const char *)(&ssval), 2, FALSE);
            }
          }
          if (THREADVAR(write_failed)) break;
        }
        if (THREADVAR(read_failed) || THREADVAR(write_failed)) {
          THREADVAR(read_failed) = THREADVAR(write_failed) = 0;
          break;
        }
        atime += 1. / afile->arps;
      }
      lives_close_buffered(afd);
      lives_close_buffered(afd2);
    }
    if (mainw->cancelled != CANCEL_NONE || THREADVAR(read_failed) || THREADVAR(write_failed)) {
      return FALSE;
    }
    return TRUE;
  }
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
    if (val > 0.) val /= 127.;
    else val /= 128.;
  } else {
    // 16 bit sample size
    if (!lives_read_buffered(afd, &val8, 1, TRUE) || !lives_read_buffered(afd, &val8b, 1, TRUE)) return 0.;
    if (afile->signed_endian & AFORM_BIG_ENDIAN) val16 = (uint16_t)(val8 << 8) + val8b;
    else val16 = (uint16_t)(val8b << 8) + val8;
    if (!(afile->signed_endian & AFORM_UNSIGNED)) val = (val16 >= 32768 ? val16 - 65536 : val16);
    else val = val16 - 32767;
    if (val > 0.) val /= 32767.;
    else val /= 32768.;

  }
  //printf("val is %f\n",val);
  return val;
}


LIVES_GLOBAL_INLINE void sample_silence_dS(float *dst, int nsamples) {
  // send silence to the jack player
  lives_memset(dst, 0, nsamples * sizeof(float));
}


void sample_silence_stream(int nchans, int nsamples) {
  float **fbuff = (float **)lives_calloc(nchans, sizeof(float *));
  boolean memok = TRUE;
  int i;

  for (i = 0; i < nchans; i++) {
    fbuff[i] = (float *)lives_calloc(nsamples, sizeof(float));
    if (!fbuff[i]) memok = FALSE;
  }
  if (memok) {
    pthread_mutex_lock(&mainw->vpp_stream_mutex);
    if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
      (*mainw->vpp->render_audio_frame_float)(fbuff, nsamples);
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
void sample_move_d8_d16(int16_t *dst, uint8_t *src,
                        int nsamples, size_t tbytes, double scale,
                        int nDstChannels, int nSrcChannels, int swap_sign) {
  // convert 8 bit audio to 16 bit audio

  // endianness will be machine endian
  double src_offset_d = 0.;
  uint8_t *ptr;
  uint8_t *src_end;
  off_t src_offset_i = 0;
  int ccount;
  int nSrcCount, nDstCount;

  // take care of rounding errors
  src_end = src + tbytes - nSrcChannels;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_d = ((double)nsamples * (-scale));
    src_offset_i = (off_t)src_offset_d * nSrcChannels;
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
      else if (swap_sign == SWAP_U_TO_S) *(dst++) = ((int16_t)(*(ptr)) - 128) << 8;
      else *((uint16_t *)(dst++)) = ((int16_t)(*(ptr)) + 128) << 8;
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if (!nSrcCount && nDstCount) {
        ccount = 0;
        nSrcCount = nSrcChannels;
      }
    }

    /* advance the position */
    if (scale < 0.) src_offset_i = (off_t)((src_offset_d += scale) - .4999) * nSrcChannels;
    else src_offset_i = (off_t)((src_offset_d += scale) + .4999) * nSrcChannels;
  }
}


/**
   @brief convert from any number of source channels to any number of destination channels - both interleaved
*/
void sample_move_d16_d16(int16_t *dst, int16_t *src,
                         int nsamples, size_t tbytes, double scale, int nDstChannels,
                         int nSrcChannels, int swap_endian, int swap_sign) {
  // TODO: going from >1 channels to 1, we should average
  // TODO: option to create non-interleaved output
  double src_offset_d = 0.;
  int16_t *ptr;
  int16_t *src_end;
  int nSrcCount, nDstCount;
  off_t src_offset_i = 0; // samaples * channels == bytes / 2
  int ccount = 0;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_d = ((double)nsamples * (-scale));
    src_offset_i = (off_t)src_offset_d * nSrcChannels;
  }

  // take care of rounding errors
  src_end = src + tbytes / 2;

  if ((off_t)((fabs(scale) * (double)nsamples)) * nSrcChannels * 2 > tbytes)
    scale = scale > 0. ? ((double)(tbytes  / nSrcChannels / 2)) / (double)nsamples
            :  -(((double)(tbytes  / nSrcChannels / 2)) / (double)nsamples);

  while (nsamples--) {
    if (src_offset_i * 2 > tbytes || src_offset_i < 0) break;
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
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(*ptr + SAMPLE_MAX_16BITI_P);
          else *(dst++) = *ptr - SAMPLE_MAX_16BITI_N;
        } else if (swap_endian == SWAP_X_TO_L) {
          if (!swap_sign) *(dst++) = (((*ptr) & 0x00FF) << 8) + ((*ptr) >> 8);
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(((*ptr & 0x00FF) << 8) + (*ptr >> 8)
                + SAMPLE_MAX_16BITI_P);
          else *(dst++) = ((*ptr & 0x00FF) << 8) + (*ptr >> 8) - SAMPLE_MAX_16BITI_N;
        } else {
          if (!swap_sign) *(dst++) = (((*ptr) & 0x00FF) << 8) + ((*ptr) >> 8);
          else if (swap_sign == SWAP_S_TO_U) *((uint16_t *)dst++) =
              (uint16_t)(((((uint16_t)(*ptr + SAMPLE_MAX_16BITI_P)) & 0x00FF) << 8) +
                         (((uint16_t)(*ptr + SAMPLE_MAX_16BITI_P)) >> 8));
          else *(dst++) = ((((int16_t)(*ptr - SAMPLE_MAX_16BITI_N)) & 0x00FF) << 8) + (((int16_t)(*ptr - SAMPLE_MAX_16BITI_N)) >> 8);
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
    /* advance the position */
    src_offset_d += scale;
    if (scale < 0.) src_offset_i = (off_t)(src_offset_d - .4999);
    else src_offset_i = (off_t)(src_offset_d + .49999);
    src_offset_i *= nSrcChannels;
  }
}


/**
   @brief convert from any number of source channels to any number of destination channels - 8 bit output
*/
void sample_move_d16_d8(uint8_t *dst, int16_t *src,
                        int nsamples, size_t tbytes, double scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  // TODO: going from >1 channels to 1, we should average
  double src_offset_d = 0.;
  int16_t *ptr;
  int16_t *src_end;
  off_t src_offset_i = 0;
  int ccount = 0;
  int nSrcCount, nDstCount;

  if (!nSrcChannels) return;

  if (scale < 0.f) {
    src_offset_d = ((double)nsamples * (-scale));
    src_offset_i = (off_t)src_offset_d * nSrcChannels;
  }

  src_end = src + tbytes / sizeof(int16_t) - nSrcChannels;

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

    /* advance the position */
    if (scale < 0.) src_offset_i = (off_t)((src_offset_d += scale) - .4999) * nSrcChannels;
    else src_offset_i = (off_t)((src_offset_d += scale) + .4999) * nSrcChannels;
  }
}


float sample_move_d16_float(float *dst, int16_t *src, int nsamples, uint64_t src_skip, int is_unsigned, boolean rev_endian,
                            float vol) {
  // convert 16 bit audio to float audio
  // NO RESAMPLING
  // returns abs(maxvol heard)

  float svolp, svoln;

#ifdef ENABLE_OIL
  float val = 0.; // set a value to stop valgrind complaining
  float maxval = 0.;
  double xn, xp, xa;
  double y = 0.f;
#else
  float val;
  float maxval = 0.;
  int16_t valss;
#endif

  uint8_t srcx[2];
  int16_t srcxs;
  int16_t *srcp;

  svolp = vol / SAMPLE_MAX_16BIT_P;
  svoln = vol / SAMPLE_MAX_16BIT_N;

#ifdef ENABLE_OIL
  xp = svolp;
  xn = svoln;
  xa = 2. * vol / (SAMPLE_MAX_16BIT_P + SAMPLE_MAX_16BIT_N);
#endif

  while (nsamples--) {
    if (rev_endian) {
      lives_memcpy(&srcx, src, 2);
      srcxs = ((srcx[1] & 0xFF)  << 8) + (srcx[0] & 0xFF);
      srcp = &srcxs;
    } else srcp = src;

    if (!is_unsigned) {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_s16(&val, srcp, 1, &y, val > 0 ? &xp : &xn);
#else
      val = (float)(*srcp) * (*srcp > 0 ? svolp : svoln);
#endif
    } else {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_u16(&val, (uint16_t *)srcp, 1, &y, &xa);
      val -= vol;
#else
      valss = (float)(*srcp / (1. * 0x8000));
      val = valss * *srcp >= 0 ? svolp : svoln;
#endif
    }

    if (1. / *srcp > maxval) maxval = 1. / *srcp;
    else if (-1. / *srcp > maxval) maxval = -1. / *srcp;

    *(dst++) = val;
    src += src_skip;
  }
  return maxval;
}


int sample_move_float_float(float *dst, float *src, int in_samples, double scale, int dst_skip, float vol,
                            int out_samples) {
  // copy one channel of float to a buffer, applying the scale (scale 2.0 to halve the rate, etc)
  // returns num samples written per out channel
  double offs_d = 0.;
  off64_t offs = 0;
  int outsamps = 0;

#ifdef USE_INTRINSICSx
  boolean can_intrin;
  off64_t xoffs;
#endif

  if (scale == 1. && dst_skip == 1 && vol == 1.) {
    lives_memcpy((void *)dst, (void *)src, in_samples * sizeof(float));
    return in_samples;
  }

  if (scale < 0.f) {
    offs_d = (1. - (double)in_samples * scale);
    offs = (off64_t)offs_d;
  }

  while (1) {
    if ((scale > 0. && offs > (off64_t)in_samples)
        || (scale < 0. && offs < 0)) break;

    if (out_samples && outsamps > out_samples) break;

    dst[dst_skip * outsamps] = src[offs] * vol;

    if (scale < 0.) {
      offs = (off64_t)((offs_d += scale) - .4999);
      if (offs < 0) break;
    } else {
      offs = (off64_t)((offs_d += scale) + .4999);
      if (offs >= in_samples) break;
    }
    outsamps++;
  }
  return outsamps;
}


#define CLIP_DECAY 1.6
#define SOFT_CLIP_THRESH 1.
#define HARD_CLIP_LIM 4.

/**
   @brief convert float samples to interleaved int
   interleaved is for the float buffer; output int is always interleaved
   scale is out_sample_rate / in_sample_rate (so 2.0 would play twice as fast, etc.)
   nsamps is number of out samples, asamps is out sample bit size (8 or 16)
   output is in holding_buff which can be cast to uint8_t *, int16_t *, or uint16_t *
   returns number of samples out (total for all chans)

   clipping is applied so that -1.0 <= fval <= 1.0
   the clipping value is applied linearly to vol (as a divisor), and if not reset it will decay so
   clip = 1.0 + (clip - 1.0) * CLIP_DECAY each sample
   --> clip = 1.0 + clip * CLIP_DECAY - CLIP_DECAY
   --> clip = (clip * CLIP_DECAY) + (1.0 - CLIP_DECAY)
   --> clip *= CLIP_DECAY; clip += (1.0 - CLIP_DECAY)
*/
int sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, double scale, int chans, int asamps,
                          int usigned, boolean rev_endian, boolean interleaved, float vol) {
  int samples_out = 0l;
  off_t offs = 0, coffs = 0, lcoffs = -1;

  static double coffs_d = 0.f;
  //eg. 1.1 and 0.9
  static float volx = 1.;

  int16_t *hbuffs = (int16_t *)holding_buff;
  uint16_t *hbuffu = (uint16_t *)holding_buff;
  uint8_t *hbuffc = (uint8_t *)holding_buff;
  int16_t val[chans];
  uint16_t valu[chans];
  float ovalf[chans], valf[chans], fval;
  float ovolx = -1.;

  asamps >>= 3;

  while (samples_out < nsamps) {
    if (volx < vol) volx *= CLIP_DECAY;
    if (volx > vol) volx = vol;

    for (int i = 0; i < chans; i++) {
      if (volx != ovolx || coffs != lcoffs) {
        valf[i] = float_buffer[i][interleaved ? (coffs * chans) : coffs];

        if (isnan(valf[i]) || valf[i] > HARD_CLIP_LIM || valf[i] < -HARD_CLIP_LIM) continue;

        valf[i] *= volx;
        fval = fabsf(valf[i]);

        if (fval > SOFT_CLIP_THRESH) {
          for (; i >= 0; i--) valf[i] /= volx;
          fval /= volx;
          volx = SOFT_CLIP_THRESH / fval;
          continue;
        }
        ovalf[i] = valf[i];
      } else valf[i] = ovalf[i];
      /////////
      ovolx = volx;

      val[i] = (int16_t)(valf[i] * (valf[i] > 0. ? SAMPLE_MAX_16BIT_P : SAMPLE_MAX_16BIT_N));
      if (usigned) valu[i] = (val[i] + SAMPLE_MAX_16BITI_P);

      if (asamps == 2) {
        if (!rev_endian) {
          if (usigned) hbuffu[offs] = valu[i];
          else hbuffs[offs] = val[i];
        } else {
          if (usigned) {
            hbuffc[offs] = valu[i] & 0x00FF;
            hbuffc[++offs] = (valu[i] & 0xFF00) >> 8;
          } else {
            hbuffc[offs] = val[i] & 0x00FF;
            hbuffc[++offs] = (val[i] & 0xFF00) >> 8;
          }
        }
      } else {
        hbuffc[offs] = (uint8_t)(valu[i] >> 8);
      }
      offs++;
    }

    samples_out++;
    lcoffs = coffs;

    if (scale < 0.) coffs = (off_t)((coffs_d += scale) - .4999);
    else coffs = (off_t)((coffs_d += scale));// + .4999);
  }

  coffs_d -= (double)coffs;
  if (prefs->show_dev_opts) {
    if (samples_out != nsamps) {
      char *msg = lives_strdup_printf("audio float -> int: buffer mismatch of %ld samples\n",
                                      samples_out - nsamps * chans);
      LIVES_WARN(msg);
      lives_free(msg);
    }
  }
  return samples_out;
}


// play from memory buffer

/**
   @brief copy audio data from cache into audio sound buffer
   - float32 version (e.g. jack)
   nchans, nsamps. out_arate all refer to player values
*/
/* int64_t sample_move_abuf_float(float **obuf, int nchans, int nsamps, int out_arate, float vol) { */
/*   int samples_out = 0; */

/* #ifdef ENABLE_JACK */

/*   int samps = 0; */

/*   lives_audio_buf_t *abuf; */
/*   int in_arate; */
/*   off_t offs = 0, ioffs, xchan; */

/*   double src_offset_d = 0.f; */
/*   off_t src_offset_i = 0; */

/*   int i, j; */

/*   double scale; */

/*   size_t curval; */

/*   /\* pthread_mutex_lock(&mainw->abuf_mutex); *\/ */
/*   /\* if (mainw->jackd->read_abuf == -1) { *\/ */
/*   /\*   pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*   /\*   return 0; *\/ */
/*   /\* } *\/ */
/*   /\* abuf = mainw->jackd->abufs[mainw->jackd->read_abuf]; *\/ */
/*   /\* in_arate = abuf->arate; *\/ */
/*   /\* pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*   scale = (double)in_arate / (double)out_arate; */

/*   while (nsamps > 0) { */
/*     /\* pthread_mutex_lock(&mainw->abuf_mutex); *\/ */
/*     /\* if (mainw->jackd->read_abuf == -1) { *\/ */
/*     /\*   pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*     /\*   return 0; *\/ */
/*     /\* } *\/ */

/*     ioffs = abuf->start_sample; */
/*     pthread_mutex_unlock(&mainw->abuf_mutex); */
/*     samps = 0; */

/*     src_offset_i = 0; */
/*     src_offset_d = 0.; */

/*     for (i = 0; i < nsamps; i++) { */
/*       // process each sample */

/*       if ((curval = ioffs + src_offset_i) >= abuf->samples_filled) { */
/*         // current buffer is consumed */
/*         break; */
/*       } */
/*       xchan = 0; */
/*       for (j = 0; j < nchans; j++) { */
/*         // copy channel by channel (de-interleave) */
/*         /\* pthread_mutex_lock(&mainw->abuf_mutex); *\/ */
/*         /\* if (mainw->jackd->read_abuf < 0) { *\/ */
/*         /\*   pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*         /\*   return samples_out; *\/ */
/*         /\* } *\/ */
/*         /\* if (xchan >= abuf->out_achans) xchan = 0; *\/ */
/*         /\* obuf[j][offs + i] = abuf->bufferf[xchan][curval] * vol; *\/ */
/*         /\* pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*         xchan++; */
/*       } */
/*       // resamplpe on the fly */
/*       if (scale < 0.) src_offset_i = (off_t)((src_offset_d += scale) - .4999); */
/*       else src_offset_i = (off_t)((src_offset_d += scale) + .4999); */
/*       samps++; */
/*       samples_out++; */
/*     } */

/*     abuf->start_sample = ioffs + src_offset_i; */
/*     nsamps -= samps; */
/*     offs += samps; */

/*     if (nsamps > 0) { */
/*       // buffer was consumed, move on to next buffer */
/*       /\* pthread_mutex_lock(&mainw->abuf_mutex); *\/ */
/*       /\* // request caching thread to fill another buffer *\/ */
/*       /\* mainw->abufs_to_fill++; *\/ */
/*       /\* if (mainw->jackd->read_abuf < 0) { *\/ */
/*       /\*   // playback ended while we were processing *\/ */
/*       /\*   pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*       /\*   return samples_out; *\/ */
/*       /\* } *\/ */
/*       /\* mainw->jackd->read_abuf++; *\/ */
/*       /\* wake_audio_thread(); *\/ */

/*       /\* if (mainw->jackd->read_abuf >= prefs->num_rtaudiobufs) mainw->jackd->read_abuf = 0; *\/ */

/*       /\* abuf = mainw->jackd->abufs[mainw->jackd->read_abuf]; *\/ */

/*       /\* pthread_mutex_unlock(&mainw->abuf_mutex); *\/ */
/*     } */
/*   } */
/* #endif */

/*   return samples_out; */



/// copy a memory chunk into an audio buffer

static int chunk_to_float_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int chans = abuf->out_achans;
  size_t offs = abuf->samples_filled;
  for (int i = 0; i < chans; i++) {
    lives_memcpy(&(abuf->bufferf[i][offs]), float_buffer[i], nsamps * sizeof(float));
  }
  return nsamps;
}


float float_deinterleave(float *dst, float *src, int in_samples, double scale, int in_chans, float vol) {
  // copy one channel of float to a buffer, applying the scale (scale 2.0 to double the rate, etc)
  // return maxvol
  double offs_d = 0.;
  off64_t offs = 0;
  int outsamps = 0;
  float val, maxval = 0.;

  if (scale < 0.f) {
    offs_d = (1. - (double)in_samples * scale);
    offs = (off64_t)offs_d;
  }

  while (1) {
    if ((scale > 0. && offs > (off64_t)in_samples)
        || (scale < 0. && offs < 0)) break;
    dst[outsamps] = (val = src[offs * in_chans]) * vol;
    if (scale < 0.) {
      offs = (off64_t)((offs_d += scale) - .4999);
      if (offs < 0) break;
    } else {
      offs = (off64_t)((offs_d += scale) + .4999);
      if (offs >= in_samples) break;
    }
    if (val > maxval) maxval = val;
    else if (-val > maxval) maxval = -val;
    outsamps++;
  }
  return maxval;
}


int float_interleave(float *out, float **in, int nsamps, double scale, int nchans, float vol) {
  // interleave a float buffer
  // (scale 2.0 to double the rate, etc)
  int tot = 0;
  for (int i = 0; i < nchans; i++) {
    tot += sample_move_float_float(&out[i], in[i], nsamps, scale, nchans, vol, 0);
  }
  return tot;
}


// for pulse audio we use S16LE interleaved, and the volume is adjusted later

static int chunk_to_int16_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int64_t samples_out;
  int chans = abuf->out_achans;
  size_t offs = abuf->samples_filled * chans;

  samples_out = sample_move_float_int(abuf->buffer16[0] + offs, float_buffer, nsamps, 1., chans, 16,
                                      0, 0, 0, 1.0);

  return samples_out / chans;
}


//#define DEBUG_ARENDER

boolean append_silence(int out_fd, void *buff, off64_t oins_size, int64_t ins_size, int asamps, int aunsigned,
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
  int i;

  boolean retval = TRUE;
  boolean revendian = FALSE;

  //#define DEBUG_ARENDER

  if (ins_size <= oins_size) {
#ifdef DEBUG_ARENDER
    g_print("sbytes is l.t zero\n");
#endif
    return FALSE;
  }
  sbytes = ins_size - oins_size;

#ifdef DEBUG_ARENDER
  g_print("sbytes is %ld\n", sbytes);
#endif
  if (sbytes > 0) {
    if ((big_endian && capable->hw.byte_order == LIVES_LITTLE_ENDIAN)
        || (!big_endian && capable->hw.byte_order == LIVES_LITTLE_ENDIAN)) revendian = TRUE;
    if (out_fd >= 0) lives_lseek_buffered_writer(out_fd, oins_size);
    else {
      if (!buff) return FALSE;
      buff += oins_size;
    }
    if (asamps == 4) {
      aunsigned = FALSE;
      revendian = FALSE;
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
    get_audio_and_effects_state_at(NULL, mainw->audio_event, tc, LIVES_PREVIEW_TYPE_AUDIO_ONLY, FALSE, NULL);
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
  // return (audio) samples rendered

  weed_plant_t *shortcut = NULL;
  lives_clip_t *outfile = to_file > -1 ? mainw->files[to_file] : NULL;
  uint8_t *in_buff;
  void *finish_buff = NULL;  ///< only used if we are writing output to a file
  double *vis = NULL;
  int16_t *holding_buff;
  weed_layer_t **layers = NULL;
  char *infilename, *outfilename;
  off64_t seekstart[nfiles];
  off_t seek;

  int in_fd[nfiles];
  int in_asamps[nfiles];
  int in_achans[nfiles];
  int in_arps[nfiles];
  int in_unsigned[nfiles];

  boolean in_reverse_endian[nfiles];
  boolean is_silent[nfiles];

  size_t max_aud_mem, bytes_to_read, aud_buffer;
  size_t tbytes;

  ssize_t bytes_read;

  int nsamples;

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
  int out_arate = to_file > -1 ? outfile->arps : obuf->arate;
  int out_unsigned = to_file > -1 ? outfile->signed_endian & AFORM_UNSIGNED : 0;
  int out_bendian = to_file > -1 ? outfile->signed_endian & AFORM_BIG_ENDIAN : 0;

  int track;
  int in_bendian;
  int first_nonsilent = -1;
  int max_segments;
  int render_block_size = RENDER_BLOCK_SIZE;
  int c, x, y;
  int out_fd = -1;

  int i;

  int64_t samples_out = 0;
  int64_t ins_size = 0l, cur_size;
  int64_t tsamples = ((double)(tc_end - tc_start) / TICKS_PER_SECOND_DBL * (double)out_arate + .5);
  int64_t blocksize, zsamples, xsamples;
  int64_t tot_samples = 0l;

  float *float_buffer[out_achans * nfiles];
  float *chunk_float_buffer[out_achans * nfiles];
  float clip_vol;

  if (out_achans * nfiles * tsamples == 0) return 0l;

  if (to_file > -1 && !mainw->multitrack && opvol_start != opvol_end) is_fade = TRUE;

  if (!storedfdsset) audio_reset_stored_fnames();

  if (!is_fade && !mainw->event_list) {
    render_block_size *= 100;
  }

  if (to_file > -1) {
    // prepare outfile stuff
    outfilename = lives_get_audio_file_name(to_file);
#ifdef DEBUG_ARENDER
    g_print("writing to %s\n", outfilename);
#endif
    out_fd = lives_open_buffered_writer(outfilename, S_IRUSR | S_IWUSR, FALSE);
    lives_buffered_set_ringmode(out_fd, 2);
    lives_free(outfilename);

    if (out_fd < 0) {
      lives_freep((void **)&THREADVAR(write_failed_file));
      THREADVAR(write_failed_file) = lives_strdup(outfilename);
      THREADVAR(write_failed) = -2;
      return 0l;
    }

    cur_size = lives_buffered_orig_size(out_fd);

    if (opvol_start == opvol_end && opvol_start == 0.) ins_pt = tc_end / TICKS_PER_SECOND_DBL;
    if (opvol_start != opvol_end) {
      ins_pt = tc_start / TICKS_PER_SECOND_DBL;
      if (ins_pt < 0.) ins_pt = 0.;
    }
    ins_pt *= out_achans * out_arate * out_asamps;
    ins_size = ((int64_t)(ins_pt / out_achans / out_asamps + .5)) * out_achans * out_asamps;

    if ((!out_bendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
      out_reverse_endian = TRUE;
    else out_reverse_endian = FALSE;

    if (ins_size > cur_size) {
      // fill to ins_pt with zeros
      append_silence(out_fd, NULL, cur_size, ins_size, out_asamps, out_unsigned, out_bendian);
    } else {
      lives_lseek_buffered_writer(out_fd, ins_size);
    }
    if (opvol_start == opvol_end && opvol_start == 0.) {
      lives_close_buffered(out_fd);
      return tsamples;
    }
  } else {
    if (mainw->event_list) cfile->aseek_pos = fromtime[0];

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

    if (from_files[track] == -1 || avels[track] == 0.) {
      is_silent[track] = TRUE;
      continue;
    }

    is_silent[track] = FALSE;
    infile = mainw->files[from_files[track]];

    in_asamps[track] = infile->asampsize / 8;
    in_achans[track] = infile->achans;
    in_arps[track] = infile->arps;
    in_unsigned[track] = infile->signed_endian & AFORM_UNSIGNED;
    in_bendian = infile->signed_endian & AFORM_BIG_ENDIAN;

    if (LIVES_UNLIKELY(in_achans[track] == 0)) is_silent[track] = TRUE;
    else {
      if ((!in_bendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN)) ||
          (in_bendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
        in_reverse_endian[track] = TRUE;
      else in_reverse_endian[track] = FALSE;

      /// this is not the velocity we will use for reading, but we need to estimate how many bytes we will read in
      /// so we can calculate how many buffers to allocate
      zavel = avels[track] * (double)in_arps[track] / (double)out_arate * in_asamps[track] * in_achans[track];

      if (fabs(zavel) > zavel_max) zavel_max = fabs(zavel);

      infilename = lives_get_audio_file_name(from_files[track]);

      seekstart[track] = quant_abytes(fromtime[track], in_arps[track], in_achans[track], in_asamps[track]);

      // try to speed up access by keeping some files open
      if (track < NSTOREDFDS && storedfnames[track] && !strcmp(infilename, storedfnames[track])) {
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
          lives_buffered_rdonly_slurp(in_fd[track], 0);
        }
      }
      seek = lives_buffered_offset(in_fd[track]);
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
      append_silence(out_fd, NULL, oins_size, ins_size, out_asamps, out_unsigned, out_bendian);
      lives_close_buffered(out_fd);
    } else {
      if (prefs->audio_player == AUD_PLAYER_JACK) {
        for (i = 0; i < out_achans; i++) {
          lives_memset((void *)obuf->bufferf[i] + obuf->samples_filled * sizeof(float), 0, tsamples * sizeof(float));
        }
      } else {
        append_silence(-1, (void *)obuf->buffer16[0], obuf->samples_filled * out_asamps * out_achans,
                       (obuf->samples_filled + tsamples) * out_asamps * out_achans, out_asamps, obuf->s16_signed
                       ? AFORM_SIGNED : AFORM_UNSIGNED,
                       ((capable->hw.byte_order == LIVES_LITTLE_ENDIAN && obuf->swap_endian == SWAP_L_TO_X)
                        || (capable->hw.byte_order == LIVES_LITTLE_ENDIAN && obuf->swap_endian != SWAP_L_TO_X)));
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

  holding_buff = (int16_t *)lives_calloc_safety(xsamples * out_achans,  sizeof(int16_t));

  for (i = 0; i < out_achans * nfiles; i++) {
    float_buffer[i] = (float *)lives_calloc_safety(xsamples, sizeof(float));
  }

  if (to_file > -1)
    finish_buff = lives_calloc_safety(tsamples, out_achans * out_asamps);

#ifdef DEBUG_ARENDER
  g_print("  rendering %ld samples %f\n", tsamples, opvol);
#endif

  // TODO - need to check amixer and get vals from sliders
  /* if (mainw->multitrack && mainw->multitrack->audio_vols && obuf) { */
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
      zavel = avels[track] * (double)in_arps[track] / (double)out_arate;

      //g_print("zavel is %f\n", zavel);
      /// tbytes: how many bytes we want to read in. This is xsamples * the track velocity.
      /// we add a small random factor here, so half the time we round up, half the time we round down
      /// otherwise we would be gradually losing or gaining samples
      tbytes = (int)((double)xsamples * fabs(zavel) + fastrand_dbl(1.)) * in_asamps[track] * in_achans[track];

      if (tbytes <= 0) {
        for (c = 0; c < out_achans; c++) {
          lives_memset(float_buffer[track * out_achans + c], 0, xsamples * sizeof(float));
        }
        continue;
      }

      in_buff = (uint8_t *)lives_calloc_safety(tbytes * 2, 1);

      if (in_fd[track] > -1) {
        if (zavel < 0.) {
          lives_buffered_rdonly_set_reversed(in_fd[track], TRUE);
          lives_lseek_buffered_rdonly(in_fd[track], - tbytes);
        } else {
          lives_buffered_rdonly_set_reversed(in_fd[track], FALSE);
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

      fromtime[track] = (double)lives_buffered_offset(in_fd[track])
                        / (double)(in_asamps[track] * in_achans[track] * in_arps[track]);

      if (from_files[track] == mainw->ascrap_file) {
        // be forgiving with the ascrap file
        if (THREADVAR(read_failed) == in_fd[track] + 1) {
          THREADVAR(read_failed) = 0;
        }
      }

      if (bytes_read < tbytes && bytes_read >= 0)  {
        append_silence(-1, in_buff, bytes_read, tbytes, in_asamps[track], mainw->files[from_files[track]]->signed_endian
                       & AFORM_UNSIGNED, mainw->files[from_files[track]]->signed_endian & AFORM_BIG_ENDIAN);
      }

      // should be approximately = xsamples, I believe
      nsamples = (tbytes / (in_asamps[track]) / in_achans[track] / fabs(zavel) + .001);

      /// convert to float
      zzavel = zavel;
      if (!mainw->multitrack) {
        clip_vol = lives_vol_from_linear(mainw->files[from_files[track]]->vol);
      } else clip_vol = mainw->files[from_files[track]]->vol;

      if (in_asamps[track] == 4) {
        // for float -> float
        if (zavel < 0.) {
          if (reverse_buffer(in_buff, tbytes, in_achans[track] * 4))
            zavel = -zavel;
        }
        for (c = 0; c < out_achans; c++) {
          float_deinterleave(float_buffer[track * out_achans + c], ((float *)in_buff + (c % in_achans[track])),
                             (double)nsamples * zavel, zavel, in_achans[track], clip_vol * (use_live_chvols ? 1. : chvol[track]));
        }
      } else {
        /// - first we convert to 16 bit stereo (if it was 8 bit and / or mono) and we resample
        /// input is tbytes bytes at rate * velocity, and we should get out nsamples audio samples at out_arate. out_achans
        /// result is in holding_buff
        if (in_asamps[track] == 1) {
          if (zavel < 0.) {
            if (reverse_buffer(in_buff, tbytes, in_achans[track]))
              zavel = -zavel;
          }
          sample_move_d8_d16(holding_buff, (uint8_t *)in_buff, nsamples, tbytes, zavel, out_achans, in_achans[track], 0);
        } else {
          if (zavel < 0.) {
            if (reverse_buffer(in_buff, tbytes, in_achans[track] * 2))
              zavel = -zavel;
          }
          sample_move_d16_d16(holding_buff, (int16_t *)in_buff, nsamples, tbytes, zavel, out_achans,
                              in_achans[track], in_reverse_endian[track] ? SWAP_X_TO_L : 0, 0);
        }
        /// if we are previewing a rendering, we would get double the volume adjustment, once from the rendering and again from
        /// the audio player, so in that case we skip the adjustment here
        //if (!mainw->preview_rendering)

        for (c = 0; c < out_achans; c++) {
          /// now we convert to holding_buff to float in float_buffer and adjust the track volume
          sample_move_d16_float(float_buffer[track * out_achans + c], holding_buff + c, nsamples,
                                out_achans, in_unsigned[track], FALSE, clip_vol * (use_live_chvols ? 1. : chvol[track]));
        }
      }
      zavel = zzavel;
      lives_free(in_buff);
    }

    // next we send small chunks at a time to the audio vol/pan effect + any other audio effects
    shortcut = NULL;
    blocksize = render_block_size; ///< this is our chunk size

    for (i = 0; i < xsamples; i += render_block_size) {
      if (i + blocksize > xsamples) blocksize = xsamples - i;

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

      if (mainw->event_list) {
        // we need to apply all audio effects with output here.
        // even in clipedit mode (for preview/rendering with an event list)
        // also, we will need to keep updating mainw->afilter_map from mainw->event_list,
        // as filters may switched on and off during the block

        int nbtracks = 0;

        // process events up to current tc:
        // filter inits and deinits, and filter maps will update the current fx state
        if (tc > 0) audio_process_events_to(tc);

        if (mainw->multitrack || mainw->afilter_map) {

          // apply audio filter(s)
          if (mainw->multitrack) {
            /// here we work out the "visibility" of each track at tc (i.e we only get audio from the front track + backing audio)
            /// any transitions will combine audio from 2 1layers (if the pref is set)
            /// backing audio tracks are always full visible
            /// the array is used to set the values of the "is_volume_master" parameter of the effect (see the Weed Audio spec.)
            vis = get_track_visibility_at_tc(mainw->multitrack->event_list, nfiles,
                                             mainw->multitrack->opts.back_audio_tracks, tc, &shortcut,
                                             mainw->multitrack->opts.audio_bleedthru);

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
            weed_set_boolean_value(layers[x], WEED_LEAF_HOST_KEEP_ADATA, WEED_TRUE);
          }

          /// apply the audo effects
          weed_apply_audio_effects(mainw->afilter_map, layers, nbtracks, out_achans, blocksize, out_arate, tc, vis);
          lives_freep((void **)&vis);

          if (layers) {
            /// after processing we get the audio data back from the layers
            for (x = 0; x < nfiles; x++) {
              float **adata = (weed_layer_get_audio_data(layers[x], NULL));
              for (y = 0; y < out_achans; y++) {
                if (chunk_float_buffer[x * out_achans + y] != adata[y]) {
                  /// non-inplace, audio was replaced so we need to copy to float_buffer and free
                  lives_memcpy(chunk_float_buffer[x * out_achans + y], adata[y], weed_layer_get_audio_length(layers[x])
                               * sizeof(float));
                  lives_free(adata[y]);
                }
              }
              lives_free(adata);
              weed_layer_set_audio_data(layers[x], NULL, 0, 0, 0);
              weed_layer_unref(layers[x]);
            }
            lives_freep((void **)&layers);
          }
        }
      }

      if (!mainw->multitrack && opvol_end != opvol_start) {
        time += (double)samples_out / (double)out_arate / out_achans;
        opvol = opvol_start + (opvol_end - opvol_start) * (time / (double)((tc_end - tc_start) / TICKS_PER_SECOND_DBL));
        opvol = lives_vol_from_linear(opvol);
      }

      if (is_fade) {
        // output to file
        // convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
        samples_out = sample_move_float_int((void *)finish_buff, chunk_float_buffer, blocksize, 1., out_achans,
                                            out_asamps * 8, out_unsigned, out_reverse_endian, FALSE, opvol);
        lives_write_buffered(out_fd, finish_buff, samples_out * out_asamps, TRUE);
        threaded_dialog_spin(0.);
        tot_samples += samples_out;
#ifdef DEBUG_ARENDER
        g_print(".");
#endif
      }

      tc += (double)blocksize / (double)out_arate * TICKS_PER_SECOND_DBL;
    }

    if (!is_fade) {
      if (to_file > -1) {
        /// output to file:
        if (out_asamps == 4)
          samples_out = float_interleave((float *)finish_buff, float_buffer, xsamples, 1., out_achans, opvol);
        else
          /// convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
          samples_out = sample_move_float_int((void *)finish_buff, float_buffer, xsamples, 1., out_achans,
                                              out_asamps * 8, out_unsigned, out_reverse_endian, FALSE, opvol);

        lives_write_buffered(out_fd, finish_buff, samples_out * out_asamps, TRUE);
#ifdef DEBUG_ARENDER
        g_print(".");
#endif
        tot_samples += samples_out / out_achans;
      } else {
        /// output to memory buffer; for jack we retain the float audio, for pulse we use int16_t
        if (prefs->audio_player == AUD_PLAYER_JACK) {
          samples_out = chunk_to_float_abuf(obuf, float_buffer, xsamples);
        } else {
          samples_out = chunk_to_int16_abuf(obuf, float_buffer, xsamples);
        }
        obuf->samples_filled += samples_out;
        tot_samples += samples_out;
      }
    }
    xsamples = zsamples;
  }

  if (xsamples > 0) {
    for (i = 0; i < out_achans * nfiles; i++) {
      if (float_buffer[i]) lives_free(float_buffer[i]);
    }
  }

  if (finish_buff) lives_free(finish_buff);
  if (holding_buff) lives_free(holding_buff);

  // close files
  for (track = 0; track < nfiles; track++) {
    if (!is_silent[track]) {
      if (track >= NSTOREDFDS && in_fd[track] > -1) lives_close_buffered(in_fd[track]);
    }
  }

  if (to_file > -1) {
#ifdef DEBUG_ARENDER
    g_prit("fs is %ld %s\n", get_file_size(out_fd, FALSE), cfile->handle);
#endif
    lives_close_buffered(out_fd);
  }

  return tot_samples;
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


/* void preview_aud_vol(frames_t aframeno) { */
/*   float ovol = cfile->vol; */
/*   cfile->vol = (float)mainw->fx1_val; */
/*   preview_audio(aframeno); */
/*   cfile->vol = ovol; */
/*   mainw->cancelled = CANCEL_NONE; */
/*   mainw->error = FALSE; */
/* } */


LIVES_GLOBAL_INLINE boolean adjust_clip_volume(int fileno, float newvol, boolean make_backup) {
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


// open a new audio file for writing to
#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(int fileno, int old_file, lives_rec_audio_type_t rec_type) {
  lives_clip_t *outfile;
  LiVESResponseType retval;

  if (fileno == -1) {
    // respond to external audio, but do not record it (yet)
    if (!mainw->jackd_read) {
      mainw->jackd_read = jack_get_driver(0, FALSE);
      mainw->jackd_read->playing_file = fileno;
      mainw->jackd_read->reverse_endian = FALSE;
      mainw->jackd_read->frames_written = 0;

      // connect the client and activate it
      jack_create_client_reader(mainw->jackd_read);
      if (!(future_prefs->jack_opts & JACK_INFO_TEST_SETUP))
        jack_read_client_activate(mainw->jackd_read, FALSE);
    }
    return;
  }

  outfile = mainw->files[fileno];

  if (mainw->aud_rec_fd == -1) {
    char *outfilename = lives_get_audio_file_name(fileno);
    do {
      retval = 0;
      mainw->aud_rec_fd = lives_create_buffered_nosync(outfilename, DEF_FILE_PERMS);
      if (mainw->aud_rec_fd < 0) {
        retval = do_write_failed_error_s_with_retry(outfilename, lives_strerror(errno));
        if (retval == LIVES_RESPONSE_CANCEL) {
          lives_free(outfilename);
          return;
        }
      }
    } while (retval == LIVES_RESPONSE_RETRY);
    lives_free(outfilename);
    if (fileno == mainw->ascrap_file) mainw->files[mainw->ascrap_file]->cb_src = mainw->aud_rec_fd;
    lives_write_buffered_set_custom_size(mainw->aud_rec_fd, AREC_BUF_SIZE);
    lives_buffered_set_ringmode(mainw->aud_rec_fd, 2);
  }

  if (rec_type == RECA_GENERATED) {
    mainw->jackd->playing_file = fileno;
  } else {
    if (rec_type == RECA_EXTERNAL) {
      mainw->jackd_read = jack_get_driver(0, FALSE);
      mainw->jackd_read->playing_file = fileno;
      mainw->jackd_read->frames_written = 0;
    }
  }

  if (rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED || rec_type == RECA_MIXED
      || rec_type == RECA_DESKTOP_GRAB_INT || rec_type == RECA_DESKTOP_GRAB_EXT) {
    off_t fsize = lives_buffered_offset(mainw->aud_rec_fd);

    if (rec_type == RECA_EXTERNAL) {
      mainw->jackd_read->reverse_endian = FALSE;

      outfile->arate = outfile->arps = mainw->jackd_read->sample_in_rate;
      outfile->achans = mainw->jackd_read->num_input_channels;
      outfile->asampsize = 16;
      outfile->signed_endian = get_signed_endian(TRUE, TRUE);

      mainw->jackd_read->frames_written = fsize / (outfile->achans * (outfile->asampsize >> 3));
    } else {
      mainw->jackd->reverse_endian = FALSE;
      outfile->arate = outfile->arps = mainw->jackd->sample_out_rate;
      outfile->achans = mainw->jackd->num_output_channels;

      outfile->asampsize = 16;
      outfile->signed_endian = get_signed_endian(TRUE, TRUE);
    }
    save_clip_audio_values(fileno);
  } else {
    int out_bendian = outfile->signed_endian & AFORM_BIG_ENDIAN;

    if ((!out_bendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
      mainw->jackd_read->reverse_endian = TRUE;
    else mainw->jackd_read->reverse_endian = FALSE;

    // start jack recording; window grab
    jack_read_client_activate(mainw->jackd_read, TRUE);
  }

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type == RECA_WINDOW_GRAB || rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED) return;

  mainw->cancelled = CANCEL_NONE;
  mainw->cancel_type = CANCEL_TYPE_SOFT;
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
    mainw->jackd_read->is_paused = TRUE;
    mainw->jackd_read->in_use = TRUE;
    on_playsel_activate(NULL, NULL);
    mainw->current_file = current_file;
  }
  mainw->cancel_type = CANCEL_TYPE_KILL;
  jack_rec_audio_end(TRUE);
}


void jack_rec_audio_end(boolean close_fd) {
  // recording ended
  if (mainw->aud_rec_rcpt) {
    // TODO !!!! need to ensure this finishes triggering ///////////////
    lives_hook_cb_remove(mainw->aud_rec_rcpt);
    mainw->aud_rec_rcpt = NULL;
    if (rec_ext_dets->bad_aud_file) lives_free(rec_ext_dets->bad_aud_file);
    lives_free(rec_ext_dets);
    rec_ext_dets = NULL;
  }

  if (mainw->jackd_read) {
    pthread_mutex_lock(&mainw->audio_filewriteend_mutex);
    mainw->jackd_read->in_use = FALSE;
    mainw->jackd_read->playing_file = -1;
    pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
  }
  if (close_fd && mainw->aud_rec_fd != -1) {
    // close file
    lives_close_buffered(mainw->aud_rec_fd);
    mainw->aud_rec_fd = -1;
  }
}
#endif


// open a new audio file for writing to
#ifdef HAVE_PULSE_AUDIO
void pulse_rec_audio_to_clip(int clipno, int old_file, lives_rec_audio_type_t rec_type) {
  // open audio file for writing
  lives_clip_t *outfile;
  int retval;

  if (clipno == -1) {
    // just activate the reader
    if (!mainw->pulsed_read) {
      mainw->pulsed_read = pulse_get_driver(FALSE);
      mainw->pulsed_read->reverse_endian = FALSE;
      mainw->aud_rec_fd = -1;
      pulse_driver_activate(mainw->pulsed_read);
      mainw->pulsed_read->in_use = TRUE;
    }
    return;
  }

  outfile = mainw->files[clipno];

  if (mainw->aud_rec_fd == -1) {
    char *outfilename = lives_get_audio_file_name(clipno);
    do {
      retval = 0;
      mainw->aud_rec_fd = lives_create_buffered_nosync(outfilename, DEF_FILE_PERMS);
      if (mainw->aud_rec_fd < 0) {
        retval = do_write_failed_error_s_with_retry(outfilename, lives_strerror(errno));
        if (retval == LIVES_RESPONSE_CANCEL) {
          lives_free(outfilename);
          return;
        }
      }
    } while (retval == LIVES_RESPONSE_RETRY);
    lives_free(outfilename);
    if (clipno == mainw->ascrap_file) mainw->files[mainw->ascrap_file]->cb_src = mainw->aud_rec_fd;
    //lives_write_buffered_set_custom_size(mainw->aud_rec_fd, AREC_BUF_SIZE);
    lives_buffered_set_ringmode(mainw->aud_rec_fd, 2);
  }

  if (rec_type == RECA_GENERATED) {
    lives_aplayer_set_clip(mainw->aplayer, clipno);
  } else {
    if (rec_type == RECA_EXTERNAL) {
      mainw->pulsed_read = pulse_get_driver(FALSE);
      //mainw->pulsed_read->playing_file = clipno;
      //mainw->pulsed_read->samples_written = 0;
    }
  }

  if (rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED || rec_type == RECA_MIXED
      || rec_type == RECA_DESKTOP_GRAB_INT || rec_type == RECA_DESKTOP_GRAB_EXT) {
    off_t fsize = lives_buffered_offset(mainw->aud_rec_fd);

    if (rec_type == RECA_EXTERNAL || rec_type == RECA_DESKTOP_GRAB_EXT) {
      mainw->pulsed_read->reverse_endian = FALSE;

      pulse_driver_activate(mainw->pulsed_read);

      outfile->arate = outfile->arps = mainw->pulsed_read->in_arate;
      outfile->achans = mainw->pulsed_read->in_achans;
      outfile->asampsize = mainw->pulsed_read->in_asamps;
      outfile->signed_endian = get_signed_endian(mainw->pulsed_read->in_signed != AFORM_UNSIGNED,
                               mainw->pulsed_read->in_endian != AFORM_BIG_ENDIAN);

      mainw->pulsed_read->samples_written = fsize / (outfile->achans * (outfile->asampsize >> 3));
    } else {
      mainw->pulsed->reverse_endian = FALSE;
      outfile->arate = outfile->arps = mainw->pulsed->out_arate;
      outfile->achans = mainw->pulsed->out_achans;
      outfile->asampsize = mainw->pulsed->out_asamps;
      outfile->signed_endian = get_signed_endian(mainw->pulsed->out_signed != AFORM_UNSIGNED,
                               mainw->pulsed->out_endian != AFORM_BIG_ENDIAN);
    }
    save_clip_audio_values(clipno);
  } else {
    int out_bendian = outfile->signed_endian & AFORM_BIG_ENDIAN;

    if ((!out_bendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN)) ||
        (out_bendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
      mainw->pulsed_read->reverse_endian = TRUE;
    else mainw->pulsed_read->reverse_endian = FALSE;

    // start pulse recording
    pulse_driver_activate(mainw->pulsed_read);
  }

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type == RECA_WINDOW_GRAB || rec_type == RECA_EXTERNAL || rec_type == RECA_GENERATED
      || rec_type == RECA_MIXED || rec_type == RECA_DESKTOP_GRAB_INT || rec_type == RECA_DESKTOP_GRAB_EXT) return;

  mainw->cancelled = CANCEL_NONE;
  mainw->cancel_type = CANCEL_TYPE_SOFT;
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
  mainw->cancel_type = CANCEL_TYPE_KILL;
  pulse_rec_audio_end(TRUE);
}


void pulse_rec_audio_end(boolean close_fd) {
  // recording ended
  if (mainw->aud_rec_rcpt) {
    // TODO !!!! need to ensure this finishes triggering ///////////////
    lives_hook_cb_remove(mainw->aud_rec_rcpt);
    mainw->aud_rec_rcpt = NULL;
    if (rec_ext_dets->bad_aud_file) lives_free(rec_ext_dets->bad_aud_file);
    lives_free(rec_ext_dets);
    rec_ext_dets = NULL;
  }

  if (mainw->pulsed_read) {
    pthread_mutex_lock(&mainw->audio_filewriteend_mutex);
    mainw->pulsed_read->in_use = FALSE;
    //mainw->pulsed_read->playing_file = -1;
    pthread_mutex_unlock(&mainw->audio_filewriteend_mutex);
  }

  if (close_fd && mainw->aud_rec_fd != -1) {
    // close file
    lives_close_buffered(mainw->aud_rec_fd);
    mainw->aud_rec_fd = -1;
  }
}

#endif

lives_proc_thread_t start_audio_rec(lives_obj_instance_t *aplayer) {
  // if the user activates recording during playback, prepare to start recording audio
  //  in this case we record only if the audio source is external, or an audio generator is running
  //
  // this is also used when recording overlay audio to a clip
  // in the current iteration, we simply add the real recording function to the DATA_READY hook
  // for the audio reader or writer
  // as a result we can be writing the data at the same time as it is being passed to analysers and video effects
  // with audio channels
  // - in theory we could record to any file, but in all cases we will write to the ascrap_file
  // which is intended solely for this purpose
  //
  // if the global audio source is internal, we record from the audio writer, if the source is external
  // we record from the reader
  arec_details *dets;
  char *lives_header, *audio_file;
  int aud_src;

  if (!aplayer) return NULL;
  aud_src = lives_aplayer_get_source(aplayer);

  if (mainw->ascrap_file == -1) open_ascrap_file(-1);

  if (mainw->ascrap_file != -1) {
    // set values so this event can be recorded in the event_list
    mainw->rec_samples = -1; // record unlimited
    mainw->rec_aclip = mainw->ascrap_file;
    mainw->rec_avel = 1.;
    mainw->rec_aseek = (double)mainw->files[mainw->ascrap_file]->aseek_pos /
                       (double)(mainw->files[mainw->ascrap_file]->arps * mainw->files[mainw->ascrap_file]->achans *
                                mainw->files[mainw->ascrap_file]->asampsize >> 3);
  }

  lives_header = lives_build_filename(prefs->workdir, mainw->files[mainw->ascrap_file]->handle,
                                      LIVES_ACLIP_HEADER, NULL);
  mainw->clip_header = fopen(lives_header, "w"); // speed up clip header writes
  lives_free(lives_header);

  IF_APLAYER_JACK
  (if (!mainw->agen_key && !mainw->agen_needs_reinit) {
  if (aud_src == AUD_SRC_EXTERNAL) {
      jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
      mainw->jackd_read->is_paused = FALSE;
      mainw->jackd_read->in_use = TRUE;
    } else jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_MIXED);
  } else {
    if (mainw->jackd) {
      jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
    }
  })

  IF_APLAYER_PULSE
  (if (mainw->agen_key && !mainw->agen_needs_reinit) {
  if (aud_src == AUD_SRC_EXTERNAL) {
      pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
      mainw->pulsed_read->is_paused = FALSE;
      mainw->pulsed_read->in_use = TRUE;
    } else pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_MIXED);
  } else {
    if (mainw->pulsed) {
      pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
    }
  })

  if (mainw->clip_header) fclose(mainw->clip_header);
  mainw->clip_header = NULL;

  if (aud_src == AUD_SRC_INTERNAL) {
    if (prefs->rec_opts & REC_AUDIO) {
      // recording INTERNAL audio
      IF_APLAYER_JACK(jack_get_rec_avals(mainw->jackd);)
      IF_APLAYER_PULSE(pulse_get_rec_avals(mainw->pulsed);)
    }
  }

  dets = (arec_details *)lives_calloc(1, sizeof(arec_details));
  rec_ext_dets = dets;

  audio_file = lives_get_audio_file_name(mainw->ascrap_file);
  dets->fd = lives_open_buffered_writer(audio_file, DEF_FILE_PERMS, TRUE);
  if (dets->fd == -1) return NULL;

  return lives_proc_thread_add_hook_cb(aplayer, DATA_READY_HOOK, 0, write_aud_data_cb);
}


void send_audio_to_fx(lives_obj_t *aplayer, lives_af_t af_type, weed_layer_t *layer) {
  if (has_audio_filters(af_type)) {
    ticks_t tc = mainw->currticks;
    // to reduce latency we run analysers i the readonly cycle phase and non analysers in
    // rw phase
    weed_apply_audio_effects_rt(layer, tc, af_type == AF_TYPE_A, TRUE);
  }
}


///////////////// data hook callbacks /////

void send_audio_to_vpp(lives_obj_t *aplayer, weed_layer_t *layer) {
  // streaming - we can push float audio to the playback plugin
  if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
    float **fltbuf = NULL;
    int nchans, nsamples;
    lives_databook_t *lbook = lives_local_databook();
    GET_BOOK_ARRAY(fltbuf, lbook, ATTR_AUDIO_DATA, &nchans);
    GET_BOOK_VALUE(nsamples, lbook, ATTR_AUDIO_DATA_LENGTH);
    pthread_mutex_lock(&mainw->vpp_stream_mutex);
    (*mainw->vpp->render_audio_frame_float)(fltbuf, nsamples);
    pthread_mutex_unlock(&mainw->vpp_stream_mutex);
  }
}


void send_audio_to_afbuffer(lives_obj_t *aplayer, weed_layer_t *layer) {
  int nsamples = weed_layer_get_audio_length(layer), nchans;
  int arate = weed_layer_get_audio_rate(layer);
  float **fltbuf = weed_layer_get_audio_data(layer, &nchans);

  //yg_print("copying %d samples to %d chans\n", nsamples, nchans);

  for (int i = nchans; i--;)
    append_to_audio_bufferf(fltbuf[i], nsamples, mainw->afbuffer, arate, i);
}


void send_audio_to_aux_afbuffer(lives_obj_t *aplayer, weed_layer_t *layer) {
  int nsamples = weed_layer_get_audio_length(layer), nchans;
  int arate = weed_layer_get_audio_rate(layer);
  float **fltbuf = weed_layer_get_audio_data(layer, &nchans);

  //g_print("copying %d samples to %d chans\n", nsamples, nchans);

  for (int i = nchans; i--;)
    append_to_audio_bufferf(fltbuf[i], nsamples, mainw->aux_afbuffer, arate, i);
}


void send_audio_to_fifo(lives_obj_t *aplayer, weed_layer_t *layer) {
  //
}


float **rt_mix_audio(lives_obj_t *aplayer, float **fltbuf) {
  // this is a callback for audio player data_preview hook
  // we mix in registered sources with pre or post effected audio
  // we will pull audio from each source, resample etc
  // each source will providde vol. levels for its channels
  // we apply auto gain to each audio source then mix them all
  //
  //

  boolean alock_mixer = FALSE;
  if (alock_mixer) {
    int nsamples = lives_aplayer_get_data_len(aplayer);
    if (nsamples && !pthread_mutex_trylock(&mainw->alock_mutex)) {
      float **xfltbuf;
      float xshrink_factor = 1.;
      int64_t xin_samplesd;
      size_t xxin_bytes;
      int arate = lives_aplayer_get_arate(aplayer);
      int nchans = lives_aplayer_get_achans(aplayer);
      off_t offs = mainw->alock_abuf->seek / (mainw->alock_abuf->in_achans
                                              * (mainw->alock_abuf->in_asamps >> 3));
      xshrink_factor = (float)mainw->alock_abuf->arate / (float)arate / mainw->audio_stretch;
      xfltbuf = lives_calloc(nchans, sizeof(float *));
      if (offs + nsamples > mainw->alock_abuf->samp_space) {
        offs = mainw->alock_abuf->seek = 0;
      }
      for (int i = 0; i < nchans; i++) {
        if (i > mainw->alock_abuf->in_achans) break;
        xfltbuf[i] = &mainw->alock_abuf->bufferf[i][offs];
      }

      xin_samplesd = fabs((double)xshrink_factor * (double)nsamples);
      xxin_bytes = (size_t)(xin_samplesd * mainw->alock_abuf->in_achans * (mainw->alock_abuf->in_asamps >> 3));

      mainw->alock_abuf->seek += xxin_bytes;
      pthread_mutex_unlock(&mainw->alock_mutex);
    }
  }
  return fltbuf;
}


boolean write_aud_data_cb(lives_obj_instance_t *aplayer, void *xdets) {
  // this function is similar to push_audio_to_channel, except that it pushes to a file,
  // and it runs during the audio cycle
  // this can be added as a callback for a player's DATA_READY_HOOK, so it can be run in parallel
  // with data analysis, and will not hold up the audio player, since these callbacks are run async / parallel
  // dets->fd is a FILE * to the file being written to
  // generally we would write to the ascrap_file

  arec_details *dets = (arec_details *)xdets;
  GET_PROC_THREAD_SELF(self);
  lives_clip_t *ofile;
  lives_databook_t *lbook;
  void *holding_buff = NULL, *out_buff;
  int nsamples, samples_out;
  size_t target_bytes, rbytes;
  ssize_t actual_bytes;
  float out_scale;
  int in_achans, out_achans;
  int in_arate, out_arate;
  int in_sampsize, out_sampsize;
  int swap_sign = 0;
  boolean in_float, out_float = FALSE;
  boolean in_interleaved = TRUE;
  boolean out_unsigned, in_unsigned;
  boolean no_free_hb = FALSE;
  boolean rev_endian = FALSE;

  if (lives_proc_thread_get_cancel_requested(self)) lives_proc_thread_cancel();

  if (dets->bad_aud_file) return FALSE;
  if (dets->rec_samples == 0) return FALSE;
  if (!IS_VALID_CLIP(dets->clipno)) return FALSE;

  if (mainw->record_paused) return TRUE;

  lbook = lives_local_databook();

  GET_BOOK_VALUE(nsamples, lbook, ATTR_AUDIO_DATA_LENGTH);
  if (!nsamples) return FALSE;

  GET_BOOK_VALUE(in_arate, lbook, ATTR_AUDIO_RATE);
  GET_BOOK_VALUE(in_float, lbook, ATTR_AUDIO_FLOAT);
  GET_BOOK_VALUE(in_arate, lbook, ATTR_AUDIO_RATE);
  GET_BOOK_VALUE(in_achans, lbook, ATTR_AUDIO_CHANNELS);
  GET_BOOK_VALUE(in_sampsize, lbook, ATTR_AUDIO_SAMPSIZE);

  GET_BOOK_VALUE(in_interleaved, lbook, ATTR_AUDIO_INTERLEAVED);

  GET_BOOK_VALUE(in_unsigned, lbook, ATTR_AUDIO_SIGNED);
  in_unsigned = !in_unsigned;

  ofile = mainw->files[dets->clipno];

  out_sampsize = ofile->asampsize >> 3;
  out_achans = ofile->achans;
  out_arate = ofile->arate;
  out_unsigned = ofile->signed_endian & AFORM_UNSIGNED;

  if (prefs->audio_opts & AUDIO_OPTS_AUX_RECORD) in_achans <<= 1;

  out_scale = out_arate / in_arate;
  samples_out = (int)((double)nsamples / out_scale + .49999);

  if (out_sampsize == 2) {
    int aendian;
    GET_BOOK_VALUE(aendian, lbook, ATTR_AUDIO_ENDIAN);
    if ((aendian == LIVES_LITTLE_ENDIAN && capable->hw.byte_order == LIVES_BIG_ENDIAN)
        || (aendian == LIVES_BIG_ENDIAN && capable->hw.byte_order == LIVES_LITTLE_ENDIAN))
      rev_endian = TRUE;
  }

  if (in_float) {
    holding_buff = lives_calloc(samples_out, out_achans * out_sampsize);
    if (!holding_buff) return FALSE;
    if (!in_interleaved) {
      float **in_buffer = NULL;
      int *pnchans = &in_achans;
      GET_BOOK_ARRAY(in_buffer, lbook, ATTR_AUDIO_DATA, pnchans);
      if (!out_float) {
        samples_out = sample_move_float_int(holding_buff, in_buffer, samples_out, out_scale, in_achans,
                                            out_sampsize * 8, out_unsigned, rev_endian, FALSE, 1.);
        rev_endian = FALSE;
        in_unsigned = FALSE;
        in_sampsize = 2;
      } else samples_out = float_interleave(holding_buff, in_buffer, samples_out, out_scale, in_achans, 1.);
      out_scale = 1.;
    } else {
      /// TODO
    }
    samples_out /= in_achans;
  }

  if (dets->rec_samples > 0) {
    if (samples_out > dets->rec_samples) samples_out = mainw->rec_samples;
    dets->rec_samples -= samples_out;
  }

  rbytes = samples_out * in_achans * in_sampsize;
  samples_out = (size_t)((double)(rbytes / out_sampsize / out_achans) / (double)out_scale);
  target_bytes = samples_out * out_achans * out_sampsize;
  g_print("REC2: %ld %ld %d\n", rbytes, target_bytes, samples_out);
  out_buff = lives_calloc(target_bytes, 4);

  if (!out_buff) {
    if (holding_buff) lives_free(holding_buff);
    return FALSE;
  }
  if (!holding_buff) {
    GET_BOOK_VALUE(holding_buff, lbook, ATTR_AUDIO_DATA);
    no_free_hb = TRUE;
  }

  if (!in_unsigned && out_unsigned) swap_sign = SWAP_S_TO_U;
  else if (in_unsigned && !out_unsigned) swap_sign = SWAP_U_TO_S;

  if (out_sampsize == 2) {
    sample_move_d16_d16((int16_t *)out_buff, holding_buff, samples_out, target_bytes, out_scale,
                        out_achans, in_achans, rev_endian ? SWAP_L_TO_X : 0, swap_sign);
  } else {
    sample_move_d16_d8((uint8_t *)out_buff, holding_buff, samples_out, target_bytes, out_scale,
                       out_achans, in_achans, swap_sign);
  }

  actual_bytes = lives_write_buffered(dets->fd, out_buff, target_bytes, TRUE);

  if (actual_bytes > 0) {
    //uint64_t chk = (mainw->aud_data_written & AUD_WRITE_CHECK);

    mainw->aud_data_written += actual_bytes;

    if (dets->clipno == mainw->ascrap_file) add_to_ascrap_mb(actual_bytes);
    //check_for_disk_space((mainw->aud_data_written & AUD_WRITE_CHECK) != chk);
    ofile->aseek_pos += actual_bytes;
  }
  if (actual_bytes < target_bytes) dets->bad_aud_file = filename_from_fd(NULL, mainw->aud_rec_fd);

  if (!no_free_hb) lives_free(holding_buff);
  lives_free(out_buff);

  return TRUE;
}


static void *ana_fx_rcpt = NULL;
static void *apply_fx_rcpt = NULL;
static void *afbuffer_rcpt = NULL;
static void *afbuffer_aux_rcpt = NULL;

/* NB: prefs->audio_opts & AUDIO_OPTS_AUX_PLAY */

void update_audio_cbs(lives_obj_instance_t *aplayer, boolean is_aux) {
  // there are several patterns we can use here:
  // for normal intern / extern playback - main audio goes to afbuffer / layer[0]
  // for AUX input, eg. voiceovers, internal audio goes to afbuffer as usual, and we create
  // aux_afbuffer. ext audio goes to aux, and is then mixed in post fx (pre or post data_ready)
  // --
  // we can apply fx to aux audio by adding to its data preview hook.
  //
  //
  if (!is_aux) {
    // analyser fx
    if (LIVES_IS_PLAYING && has_audio_filters(AF_TYPE_A)) {
      if (!ana_fx_rcpt)
        ana_fx_rcpt = lives_obj_instance_add_hook_cb_full
                      (aplayer, DATA_READY_HOOK, 0, send_audio_to_fx, WEED_SEED_VOID,
                       "i", AF_TYPE_A);
    } else {
      if (ana_fx_rcpt) {
        lives_hook_cb_remove(ana_fx_rcpt);
        ana_fx_rcpt = NULL;
      }
    }

    // non-analyser fx
    if (LIVES_IS_PLAYING && ((prefs->audio_src != AUDIO_SRC_EXT || (prefs->audio_opts & AUDIO_OPTS_EXT_FX))
                             && has_audio_filters(AF_TYPE_NONA))) {
      if (!apply_fx_rcpt)
        apply_fx_rcpt =
          lives_obj_instance_add_hook_cb_full(aplayer, DATA_PREVIEW_HOOK,
                                              0, send_audio_to_fx, WEED_SEED_VOID,
                                              "i", AF_TYPE_NONA);
    }



    /// to generators
    if (LIVES_IS_PLAYING && mainw->afbuffer && (!mainw->event_list || mainw->record || mainw->record_paused))  {
      if (!afbuffer_rcpt)
        afbuffer_rcpt =
          lives_obj_instance_add_hook_cb_full(aplayer, DATA_READY_HOOK, 0,
                                              send_audio_to_afbuffer, WEED_SEED_VOID, "");
      LIVES_ASSERT(lives_obj_instance_has_hook_cbs(aplayer, DATA_READY_HOOK));
    } else {
      if (afbuffer_rcpt) {
        lives_hook_cb_remove(afbuffer_rcpt);
        afbuffer_rcpt = NULL;
      }
    }




  } else {
    if (LIVES_IS_PLAYING && is_aux && mainw->aux_afbuffer) {
      if (!afbuffer_aux_rcpt)
        afbuffer_aux_rcpt =
          lives_obj_instance_add_hook_cb_full(aplayer, DATA_READY_HOOK, 0,
                                              send_audio_to_aux_afbuffer, WEED_SEED_VOID, "");

    } else {
      if (afbuffer_aux_rcpt) {
        lives_hook_cb_remove(afbuffer_aux_rcpt);
        afbuffer_aux_rcpt = NULL;
      }
    }
  }
}


LIVES_GLOBAL_INLINE void block_unblock_aux_cbs(boolean block) {
  if (afbuffer_aux_rcpt) {
    if (block) lives_cb_receipt_block_cb((afbuffer_aux_rcpt));
    else lives_cb_receipt_block_cb((afbuffer_aux_rcpt));
  }
}


/////////////////////////////////////////////////////////////////

// playback via memory buffers (e.g. in multitrack)

////////////////////////////////////////////////////////////////
/// TODO - move these to events.c

static lives_audio_track_state_t *resize_audstate(lives_audio_track_state_t *ostate, int nostate, int nstate) {
  // increase the element size of the audstate array (ostate)
  // from nostate elements to nstate elements
  lives_audio_track_state_t *audstate =
    (lives_audio_track_state_t *)lives_recalloc((void *)ostate, nstate, nostate, sizeof(lives_audio_track_state_t));
  return audstate;
}


static lives_audio_track_state_t *aframe_to_atstate_inner(weed_plant_t *event, int *ntracks) {
  // parse an audio frame, and set the track file, seek and velocity values
  int num_aclips = 0, atrack;
  int *aclips = NULL;
  double *aseeks = NULL;
  int naudstate = 0;
  lives_audio_track_state_t *atstate = NULL;
  int btoffs = mainw->multitrack ? mainw->multitrack->opts.back_audio_tracks : 1;

  num_aclips = weed_frame_event_get_audio_tracks(event, &aclips, &aseeks);
  for (int i = 0; i < num_aclips; i += 2) {
    if (aclips[i + 1] > 0) { // else ignore
      atrack = aclips[i];
      if (atrack + btoffs + 1 > naudstate) {
        atstate = resize_audstate(atstate, naudstate, atrack + btoffs + 1);
        for (int j = naudstate; j <= atrack + btoffs; j++) atstate[j].afile = -1;
        naudstate = atrack + btoffs + 1;
      }
      atstate[atrack + btoffs].afile = aclips[i + 1];
      atstate[atrack + btoffs].seek = aseeks[i];
      atstate[atrack + btoffs].vel = aseeks[i + 1];
    }
  }

  lives_freep((void **)&aclips);
  lives_freep((void **)&aseeks);

  if (ntracks) *ntracks = naudstate;

  return atstate;
}


LIVES_GLOBAL_INLINE lives_audio_track_state_t *audio_frame_to_atstate(weed_event_t *event, int *ntracks) {
  return aframe_to_atstate_inner(event, ntracks);
}


/**
   @brief get audio (and optionally video) state at timecode tc OR before event st_event

   if st_event is not NULL, we get the state just prior to it
   (state being effects and filter maps, audio tracks / positions)

   if st_event is NULL, this is a continuation, and we get the audio state only at timecode tc
   similar to quantise_events(), except we don't produce output samples
*/
lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event,
    weed_timecode_t fill_tc, int what_to_get, boolean exact, int *xntracks) {
  // if exact is set, we must rewind back to first active stateful effect,
  // and play forwards from there (not yet implemented - TODO)
  lives_audio_track_state_t *atstate = NULL, *audstate = NULL;
  weed_timecode_t last_tc = 0;
  weed_event_t *event, *nevent;
  weed_event_t *deinit_event;
  int ntracks = 0, etype;

  // gets effects state immediately prior to start_event. (initing any effects which should be active, and applying param changes
  // if not in multrack)

  // optionally: gets audio state, sets atstate[0].tc
  // and initialises audio buffers

  if (fill_tc == 0 && event_list) {
    if (what_to_get != LIVES_PREVIEW_TYPE_AUDIO_ONLY)
      mainw->filter_map = NULL;
    if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY)
      mainw->afilter_map = NULL;
    event = get_first_event(event_list);
  } else {
    event = st_event;
    st_event = NULL;
  }

  if (!event) return audstate;

  while ((st_event && event != st_event) || (!st_event && get_event_timecode(event) < fill_tc)) {
    etype = weed_event_get_type(event);
    if (what_to_get == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || (etype != WEED_EVENT_TYPE_FRAME
        && (!event_list || etype != WEED_EVENT_TYPE_PARAM_CHANGE))) {
      switch (etype) {
      case WEED_EVENT_TYPE_FILTER_MAP:
        if (what_to_get != LIVES_PREVIEW_TYPE_AUDIO_ONLY)
          mainw->filter_map = event;
        if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY) {
          mainw->afilter_map = event;
        }
        break;
      case WEED_EVENT_TYPE_FILTER_INIT:
        deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, NULL);
        if (!deinit_event || get_event_timecode(deinit_event) >= fill_tc) {
          // this effect should be activated
          if (what_to_get != LIVES_PREVIEW_TYPE_AUDIO_ONLY)
            process_events(event, FALSE, get_event_timecode(event));
          if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY)
            process_events(event, TRUE, get_event_timecode(event));
          /// TODO: if exact && non-stateless, silently process audio / video until st_event
        }
        break;
      case WEED_EVENT_TYPE_FILTER_DEINIT:
        if (what_to_get == LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
          weed_event_t *init_event = weed_get_voidptr_value((weed_plant_t *)event, WEED_LEAF_INIT_EVENT, NULL);
          if (get_event_timecode(init_event) >= last_tc) break;
          process_events(event, TRUE, get_event_timecode(event));
        }
        break;
      case WEED_EVENT_TYPE_PARAM_CHANGE:
        if (!mainw->multitrack) {
          weed_event_t *init_event = weed_get_voidptr_value((weed_plant_t *)event, WEED_LEAF_INIT_EVENT, NULL);
          deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, NULL);
          if (deinit_event && get_event_timecode(deinit_event) < fill_tc) break;

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
              lives_leaf_dup(param, event, WEED_LEAF_VALUE);
              weed_instance_unref(inst);
            }
          }
        }
        break;
      case WEED_EVENT_TYPE_FRAME:
        if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_AUDIO) break;

        if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
          /// update audio state
          int nntracks;
          atstate = audio_frame_to_atstate(event, &nntracks);
          if (!audstate) {
            audstate = atstate;
            last_tc = get_event_timecode(event);
            ntracks = nntracks;
          } else {
            // have an existing audio state, update with current
            weed_timecode_t tc = get_event_timecode(event);
            weed_timecode_t delta = tc - last_tc;
            if (nntracks > ntracks) {
              audstate = resize_audstate(audstate, ntracks, nntracks);
              ntracks = nntracks;
            }
            for (int i = 0; i < ntracks; i++) {
              if (delta > 0) {
                // increase seek values up to current frame
                audstate[i].seek += audstate[i].vel * delta / TICKS_PER_SECOND_DBL;
              }
            }
            last_tc = tc;

            for (int i = 0; i < nntracks; i++) {
              if (atstate[i].afile != -1) {
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
    if (!nevent) break;
    event = nevent;
    if (what_to_get == LIVES_PREVIEW_TYPE_AUDIO_ONLY && WEED_EVENT_IS_AUDIO_FRAME(event)) break;
  }
  if (what_to_get == LIVES_PREVIEW_TYPE_VIDEO_AUDIO) {
    if (audstate) {
      weed_timecode_t delta = get_event_timecode(event) - last_tc;
      if (delta > 0) {
        for (int i = 0; i < ntracks; i++) {
          if (audstate[i].afile != -1) {
            // increase seek values up to current frame
            audstate[i].seek += audstate[i].vel * delta / TICKS_PER_SECOND_DBL;
          }
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (what_to_get != LIVES_PREVIEW_TYPE_VIDEO_ONLY)
    mainw->audio_event = event;
  if (xntracks) *xntracks = ntracks;
  return audstate;
}


void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, boolean exact) {
  // fill audio buffer with audio samples, using event_list as a guide
  // if st_event!=NULL, that is our start event, and we will calculate the audio state at that
  // point

  // otherwise, we continue from where we left off the last time

  // all we really do here is set from_files, aseeks and avels arrays and call render_audio_segment
  // effects are ignored here; they are applied in smaller chunks in render_audio_segment, so that parameter interpolation can be done

  // this is called repeatedly from cache_my_audio(), as well as once from player-control.c to preload the buffers for playback

  lives_audio_track_state_t *atstate = NULL;
  double chvols[MAX_AUDIO_TRACKS]; // TODO - use list

  static weed_timecode_t last_tc, tc;
  static weed_timecode_t fill_tc;
  static weed_plant_t *event;
  static int ntracks;
  int nntracks = 0;

  static int *from_files = NULL;
  static double *aseeks = NULL, *avels = NULL;

  int rr;

  if (!abuf) return;

  abuf->samples_filled = 0; // write fill level of buffer
  abuf->start_sample = 0; // read level

  if (st_event) {
    // this is only called for the first buffered read
    //
    event = st_event;
    tc = last_tc = get_event_timecode(event);

    lives_freep((void **)&from_files);
    lives_freep((void **)&avels);
    lives_freep((void **)&aseeks);

    if (mainw->multitrack && mainw->multitrack->avol_init_event)
      ntracks = weed_leaf_num_elements(mainw->multitrack->avol_init_event, WEED_LEAF_IN_TRACKS);
    else ntracks = 1;

    from_files = (int *)lives_calloc(ntracks, sizint);
    avels = (double *)lives_calloc(ntracks, sizdbl);
    aseeks = (double *)lives_calloc(ntracks, sizdbl);

    for (rr = 0; rr < ntracks; rr++) {
      from_files[rr] = -1;
      avels[rr] = aseeks[rr] = 0.;
    }

    // get audio and fx state at pt immediately before st_event
    atstate = get_audio_and_effects_state_at(event_list, event, 0, LIVES_PREVIEW_TYPE_VIDEO_AUDIO, exact, &nntracks);

    if (nntracks > ntracks) {
      from_files = (int *)lives_recalloc(from_files, nntracks, ntracks, sizint);
      avels = (double *)lives_recalloc(avels, nntracks, ntracks, sizdbl);
      aseeks = (double *)lives_recalloc(aseeks, nntracks, ntracks, sizdbl);
    }

    if (atstate) {
      for (rr = 0; rr < nntracks; rr++) {
        if (rr >= ntracks) from_files[rr] = -1;
        else {
          if (atstate[rr].afile > 0) {
            from_files[rr] = atstate[rr].afile;
            avels[rr] = atstate[rr].vel;
            aseeks[rr] = atstate[rr].seek;
          }
        }
      }
      lives_free(atstate);
    }
  }

  if (nntracks > ntracks) ntracks = nntracks;


  if (mainw->multitrack) {
    // get channel volumes from the mixer
    int ch = 0;
    for (LiVESList *list = mainw->multitrack->audio_vols; list; list = list->next) {
      chvols[ch++] = (double)LIVES_POINTER_TO_INT(list->data) / ONE_MILLION_DBL;
    }
  } else chvols[0] = 1.;

  fill_tc = last_tc + fabs((double)(abuf->samp_space) / (double)abuf->arate * TICKS_PER_SECOND_DBL);

  // continue until we have a full buffer
  // if we get an audio frame we render up to that point
  // then we render what is left to fill abuf
  while (event && (tc = get_event_timecode(event)) < fill_tc) {
    if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
      // got next audio frame
      if (tc > last_tc)
        render_audio_segment(ntracks, from_files, -1, avels, aseeks, last_tc, tc, chvols, 1., 1., abuf);
      last_tc = tc;
      // process audio updates at this frame
      atstate = audio_frame_to_atstate(event, &nntracks);

      if (atstate) {
        for (rr = 0; rr < nntracks; rr++) {
          if (atstate[rr].afile > 0) {
            from_files[rr] = atstate[rr].afile;
            avels[rr] = atstate[rr].vel;
            aseeks[rr] = atstate[rr].seek;
          }
        }
        lives_free(atstate);
      }
      if (nntracks > ntracks) ntracks = nntracks;
    }
    event = get_next_audio_frame_event(event);
  }

  if (last_tc < fill_tc) {
    // fill the rest of the buffer
    render_audio_segment(ntracks, from_files, -1, avels, aseeks, last_tc, fill_tc, chvols, 1., 1., abuf);
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
  mainw->jackd->abufs = (lives_audio_buf_t **)lives_calloc(prefs->num_rtaudiobufs, sizeof(lives_audio_buf_t *));

  for (int i = 0; i < prefs->num_rtaudiobufs; i++) {
    mainw->jackd->abufs[i] = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));
    mainw->jackd->abufs[i]->out_achans = achans;
    mainw->jackd->abufs[i]->arate = arate;
    mainw->jackd->abufs[i]->samp_space = XSAMPLES / prefs->num_rtaudiobufs;
    mainw->jackd->abufs[i]->bufferf = (float **)lives_calloc(achans, sizeof(float *));
    for (int chan = 0; chan < achans; chan++) {
      mainw->jackd->abufs[i]->bufferf[chan] = (float *)lives_calloc_safety(XSAMPLES / prefs->num_rtaudiobufs, sizeof(float));
    }
  }
#endif
}


/* void init_pulse_audio_buffers(int achans, int arate, boolean exact) { */
/* #ifdef HAVE_PULSE_AUDIO */
/*   mainw->pulsed->abufs = (lives_audio_buf_t **)lives_calloc(prefs->num_rtaudiobufs, sizeof(lives_audio_buf_t *)); */

/*   for (int i = 0; i < prefs->num_rtaudiobufs; i++) { */
/*     mainw->pulsed->abufs[i] = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t)); */

/*     mainw->pulsed->abufs[i]->out_achans = achans; */
/*     mainw->pulsed->abufs[i]->arate = arate; */
/*     mainw->pulsed->abufs[i]->start_sample = 0; */
/*     mainw->pulsed->abufs[i]->samp_space = XSAMPLES / prefs->num_rtaudiobufs; // samp_space here is in stereo samples */
/*     mainw->pulsed->abufs[i]->buffer16 = (int16_t **)lives_calloc(1, sizeof(int16_t *)); */
/*     mainw->pulsed->abufs[i]->buffer16[0] = (int16_t *)lives_calloc_safety(XSAMPLES / prefs->num_rtaudiobufs, */
/*                                            achans * sizeof(int16_t)); */
/*   } */
/* #endif */
/* } */


void free_jack_audio_buffers(void) {
#ifdef ENABLE_JACK
  int chan;

  if (!mainw->jackd || !mainw->jackd->abufs) return;

  for (int i = 0; i < prefs->num_rtaudiobufs; i++) {
    if (mainw->jackd->abufs[i]) {
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


/* void free_pulse_audio_buffers(void) { */
/* #ifdef HAVE_PULSE_AUDIO */

/*   if (!mainw->pulsed || !mainw->pulsed->abufs) return; */

/*   for (int i = 0; i < prefs->num_rtaudiobufs; i++) { */
/*     if (mainw->pulsed->abufs[i]) { */
/*       lives_free(mainw->pulsed->abufs[i]->buffer16[0]); */
/*       lives_free(mainw->pulsed->abufs[i]->buffer16); */
/*       lives_free(mainw->pulsed->abufs[i]); */
/*     } */
/*   } */
/*   lives_free(mainw->pulsed->abufs); */
/* #endif */
/* } */


void freeze_unfreeze_audio(boolean is_frozen) {
  if (prefs->audio_src == AUDIO_SRC_INT) {
    // TODO - what we need to do here is force the audio position back (or forwards)
    // to the previous frame position, so when
    int afile = get_aplay_clipno();
#ifdef ENABLE_JACK
    if (mainw->jackd && prefs->audio_player == AUD_PLAYER_JACK
        && mainw->jackd->is_paused != is_frozen
        && ((afile == mainw->playing_file
             && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS))
            || ((prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
                && (!is_frozen || (prefs->audio_opts & AUDIO_OPTS_LOCKED_FREEZE))))) {
      if (IS_PHYSICAL_CLIP(afile)) {
        lives_clip_t *xafile = mainw->files[afile];
        if (!is_frozen) {
          /* align_async_pos(afile); */
          mainw->startticks = mainw->currticks - xafile->async_delta;
        } else {
          xafile->async_delta = mainw->currticks - mainw->startticks;
        }
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO) && mainw->agen_key == 0 &&
            !mainw->agen_needs_reinit) {
          if (is_frozen) {
            weed_plant_t *event = get_last_frame_event(mainw->event_list);
            insert_audio_event_at(event, -1, afile, 0., 0.); // audio switch off
          } else {
            // seek to restart frame
            jack_get_rec_avals(mainw->jackd);
          }
        }
      } else mainw->rec_aclip = -1;
      mainw->jackd->is_paused = is_frozen;
      if (mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
          && (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER)) {
        if (is_frozen) jack_pb_stop(mainw->jackd_trans);
        else jack_pb_start(mainw->jackd_trans, -1.);
      }
    }
#endif
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed && prefs->audio_player == AUD_PLAYER_PULSE
        && mainw->pulsed->is_paused != is_frozen
        && ((lives_aplayer_get_clip(mainw->aplayer)
             == mainw->playing_file
             && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS))
            || ((prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
                && (!is_frozen || (prefs->audio_opts & AUDIO_OPTS_LOCKED_FREEZE))))) {
      if (IS_PHYSICAL_CLIP(afile)) {
        lives_clip_t *xafile = mainw->files[afile];
        if (!is_frozen) {
          mainw->startticks = mainw->currticks - xafile->async_delta;
        } else {
          xafile->async_delta = mainw->currticks - mainw->startticks;
        }
        if (mainw->record && !mainw->record_paused && (prefs->rec_opts & REC_AUDIO) && mainw->agen_key == 0 &&
            !mainw->agen_needs_reinit) {
          if (is_frozen) {
            if (!mainw->mute) {
              weed_plant_t *event = get_last_frame_event(mainw->event_list);
              insert_audio_event_at(event, -1, lives_aplayer_get_clip(mainw->aplayer), 0., 0.); // audio switch off
            }
          } else {
            pulse_get_rec_avals(mainw->pulsed);
          }
        }
      } else mainw->rec_aclip = -1;
      mainw->pulsed->is_paused = is_frozen;
    }
#endif
  }
}


LIVES_GLOBAL_INLINE boolean avsync_force(lives_obj_instance_t *aplayer) {
  // this is called to force realignment of audio with video,
  /// either from lives_aplayer_seek_to, or directly to resync at next video frame

  // not only do we resync the position but we also reset the audio play direction to equal video direction
  // and we reset the velocity to equal abs(pb_fps / fps)

  // it should be called in the following circumstances:
  // fps reset is actioned, unless
  //  - AUDIO_OPTS_NO_RESYNC_FPS is set, or
  //	- AUDIO_OPTS_IS_LOCKED is set, and AUDIO_OPTS_LOCKED_RESET is not set

  // vpos jumps, unless OPTS_NO_RESYNC_VPOS is set or  AUDIO_OPTS_IS_LOCKED is set
  // audio direction changes and AUDIO_OPTS_RESYNC_ADIR and  AUDIO_OPTS_IS_LOCKED not set
  // audio clip changes and AUDIO_OPTS_RESYNC_ACLIP is set
  // audio is unlocked and AUDIO_OPTS_UNLOCK_RESYNC is set

  //= get_aplayer_instance(prefs->audio_src);

  uint64_t astat;
  if (!LIVES_CE_PLAYBACK || AUD_SRC_EXTERNAL || mainw->foreign) return FALSE;
  lives_clip_t *sfile = RETURN_NORMAL_CLIP(mainw->playing_file);
  if (!sfile) return FALSE;

  g_print("avsync\n");

  if (lives_aplayer_get_seek_state(aplayer) == not_seeking) {
    if (sfile->pb_fps > 0.) sfile->adirection = LIVES_DIRECTION_FORWARD;
    else sfile->adirection = LIVES_DIRECTION_REVERSE;
    sfile->avelocity = abs(sfile->pb_fps / sfile->fps);

    lives_aplayer_set_seek_vals(aplayer, mainw->playing_file, -1.,
                                sfile->adirection, sfile->avelocity);

    astat = lives_aplayer_get_status(aplayer) & ~3;

    lives_aplayer_set_seek_state(aplayer, seek_needstarget);

    lives_aplayer_set_status(aplayer, astat | APLAYER_STATUS_RESYNC);
    mainw->avsync_time = lives_get_session_time();
  }
  return TRUE;
}


lives_result_t audio_sync_ready(lives_obj_instance_t *aplayer) {
  while (LIVES_IS_PLAYING && !pthread_mutex_trylock(&mainw->avseek_mutex))
    pthread_mutex_unlock(&mainw->avseek_mutex);

  if (LIVES_IS_PLAYING) {
    uint64_t astat = lives_aplayer_get_status(aplayer) & ~3;
    astat |= APLAYER_STATUS_RUNNING;
    lives_aplayer_set_status(aplayer, astat);
    if (lives_aplayer_get_seek_state(aplayer) == seek_ready) {
      IF_APLAYER_JACK
      (if ((prefs->rec_opts & REC_AUDIO) && AUD_SRC_INTERNAL
           && mainw->rec_aclip != mainw->ascrap_file)
       jack_get_rec_avals(mainw->jackd);)

        IF_APLAYER_PULSE
        (if ((prefs->rec_opts & REC_AUDIO) && AUD_SRC_INTERNAL
             && mainw->rec_aclip != mainw->ascrap_file)
         pulse_get_rec_avals(mainw->pulsed);
         mainw->pulsed->in_use = TRUE;
        )
        }
    lives_aplayer_set_seek_state(aplayer, not_seeking);
    lives_obj_instance_trigger_hook(aplayer, SEEK_READY_HOOK);
    return LIVES_RESULT_SUCCESS;
  }
  return LIVES_RESULT_FAILED;
}


LIVES_GLOBAL_INLINE int get_aplay_clipno(void) {
  return lives_aplayer_get_clip(mainw->aplayer);
}


LIVES_GLOBAL_INLINE off_t get_aplay_offset(void) {
  return lives_aplayer_get_pos(mainw->aplayer);
}


LIVES_GLOBAL_INLINE int get_aplay_rate(void) {
  return lives_aplayer_get_arate(mainw->aplayer);
}


lives_result_t lives_aplayer_do_seek(lives_obj_instance_t *obj, boolean block) {
  // seek to values set with lives_aplayer_set_seek_values
  lives_result_t res = LIVES_RESULT_SUCCESS;

  /* IF_APLAYER_JACK(//jack_audio_seek_frame(mainw->jackd, clip, target); */
  /* 		  if (mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) */
  /* 		      && (prefs->jack_opts & JACK_OPTS_TIMEBASE_LSTART)) */
  /* 		    jack_transport_update(mainw->jackd_trans, xtime);) */

  IF_APLAYER_PULSE(res = lives_pulse_seek(mainw->pulsed, block);)

  return res;
}


/**
   @brief resync audio playback to a target clip / frame

   this is called internally - for example when the play position jumps, either due
   to external transport changes, (e.g. jack transport, osc retrigger / goto)
   or if we are looping a video selection, or it may be triggered from the keyboard

   if the mode is not free playback in clip editor this will be ignored
   if player mode is external this will be ignored

   the target clip, frame, velocity and direction may be specified
   if clip is -1 or oher invalid clip, resync will be with the current video clip
   if the frame target is <= 0. if the clip is not specified, the seek will synchronis with the video player
   if the clip is to a different clip, audio will firstr seek to the aseek_pos

   in all cases an approximate seek is followed by an exact seek for fine adjustment
   - the video stream is only paused briefly while the exact seekk is done, and during this momentary period
   the playback clock juat marks time.

   to resync to the current video frame, it may be preferrable to call avsync_force()
   directly instead.

   Note if LiVES is controlling exteral audio transprt (eg. jack transport) that will also
   be algned alongside the audio.
*/

lives_result_t lives_aplayer_seek_to(int clip, double xtime,
                                     lives_direction_t dir, double vel, boolean block) {
  lives_result_t res = LIVES_RESULT_FAILED;

  if (!LIVES_CE_PLAYBACK || AUD_SRC_EXTERNAL) return LIVES_RESULT_INVALID;
  // if we are playing an event_list, then resync is meaningless
  if (mainw->event_list && !mainw->record && !mainw->record_paused) return LIVES_RESULT_ERROR;

  lives_aplayer_set_seek_vals(mainw->aplayer, clip, xtime, dir, vel);

  res = lives_aplayer_do_seek(mainw->aplayer, block);
  return res;
}


//////////////////////////////////////////////////////////////////////////
static lives_audio_buf_t *cache_buffer = NULL;
static lives_audio_buf_t *cache_buffera = NULL;
static lives_audio_buf_t *cache_bufferb = NULL;
static lives_proc_thread_t athread;

static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;

static void fill_layer(weed_layer_t *layer) {

#define maxtime 60.
#define mintime 20.

  // aim is to back and front fill from the pb offset
  // by default we shift the seek offset to 1 / 4  full buff size
  // so we have some leeway for bacwards adjustments and quick dirchanges / loops

  // we try to at all times keep between mintime and maxtime audio buffered

  // the audio player then takes small scoops out of this

  ____FUNC_ENTRY____(fill_layer, "", "P");

  int clip = lives_layer_get_clip(layer);
  lives_clip_t *sfile = RETURN_VALID_CLIP(clip);
  if (!sfile)
    ____FUNC_EXIT____;
  GET_PROC_THREAD_SELF(self);

  int in_arate = sfile->arate;
  int in_inter = TRUE;
  int in_achans = sfile->achans;
  int in_asamps = sfile->asampsize;
  int in_float = FALSE;

  int out_nchans = weed_layer_get_naudchans(layer);
  int out_arate = weed_layer_get_audio_rate(layer);

  int out_asamps = weed_layer_get_audio_asamps(layer);
  int out_inter = weed_layer_get_audio_interleaved(layer);

  static size_t bufsz = 0, minbufsz;

  int fd = weed_get_int_value(layer, "abuff_id", NULL);

  int64_t bb_size = weed_get_int64_value(layer, "bb_size", NULL);

  int new_clip;

  GET_SELF_VALUE(new_clip, "new_clip");

  if (new_clip != clip) {
    if (fd > 0) {
      lives_close_buffered(fd);
      fd = -1;
      weed_set_int_value(layer, "abuff_id", fd);
      bb_size = 0;
    }
    clip = new_clip;
  }

  if (fd <= 0) {
    char *filename = lives_get_audio_file_name(clip);
    fd = lives_open_buffered_rdonly(filename);
    lives_free(filename);
    // tell the file reader to fill to MIN(remainder in  buffer,  65536)
    lives_buffered_rdonly_set_quanta(fd, 65536);
    weed_set_int_value(layer, "abuff_id", fd);
  }

  if (!bufsz) {
    bufsz = (size_t)(maxtime * out_arate) * (out_asamps >> 3);
    minbufsz = (size_t)(mintime * out_arate) * (out_asamps >> 3);
    if (minbufsz < bufsz >> 1) minbufsz = bufsz >> 1;
  }

  // we have 3 buffsizes -
  // - bufsz - configured target size
  // - bb_size - filled size
  // - new_size - target - small size for prebuff

  //

  // a) calc bytes needed to fill to bufsz - if > minbufsz, exit
  // b) calc in bytes needed
  // c) round in bytes to quanta
  // d) recalc out bytes, this becomes new_bb_size
  // e) alloc inbuff(s) size == in_bytes
  // f) read from src -> inbuffs
  // g) alloc tmpbuff - size == new_bb_size
  // h) copy/shift from bbuff to tmpbuff - size real_bb_size - bb_offs + rev
  // i) resample from inbuffs to tmpbuf, append after, up to new_bb_size
  // j) lock mutex, swap in tmpbuf for bb_buff
  // k) unlock mutex, free old bb_buffs
  // l) real_bb_size = new_bb_size

  void **bb_buff = weed_get_voidptr_array(layer, "bb_buff", NULL);
  int64_t needed = 0, used = 0, unused, mv = 0;
  int64_t in_bytes, max_in_bytes, target;;
  int64_t bb_offs = weed_get_int64_value(layer, "bb_offs", NULL);
  int64_t bb_in_offs;
  int64_t bb_in_pos = weed_get_int64_value(layer, "bb_in_pos", NULL);
  int64_t bb_in_size = weed_get_int64_value(layer, "bb_in_size", NULL);
  int64_t new_bb_size;

  boolean is_preload = FALSE;

  int64_t xseek = bb_in_pos + bb_in_size;

  if (!bb_size) {
    needed = target = bufsz >> 2;
    is_preload = TRUE;
  } else {
    target = bufsz;
    used = bb_offs; // eg. 0.7
    unused = bb_size - used; // eg. 0.3
    if (unused > minbufsz) return;
    if (used > (bufsz >> 2)) {
      mv = used - (bufsz >> 2);  // .45
      used = (bufsz >> 2); // 0.25
    }
    needed = target - used - unused;
    // 0.7 - 0.25 = 0.45
  }

  void **inbuffs = NULL; // buffers to read into
  void **xbufs; // pointers to fill region for resampling
  void **tmpbufs; // buffers to replace bb_buff

  int out_smps = needed / (out_asamps >> 3);
  int64_t out_bytes = out_smps * (out_asamps >> 3);

#if HAVE_SWRESAMPLE
  struct SwrContext *swr_ctx = NULL;
  enum AVSampleFormat outfmt, infmt;
  int xin_asamps = in_asamps;
  if (in_float) xin_asamps = -in_asamps;

  int max_out_smps = bufsz / (out_asamps >> 3);
  int max_in_smps = ((double)max_out_smps * (double)in_arate / (double)out_arate + .999999);

  // xout_samps is a least out_smps, in_smps is exact
  int xout_smps = out_smps;
  int in_smps = get_swr_fmts(&swr_ctx, -out_asamps, out_arate, out_nchans, FALSE, &xout_smps,
                             xin_asamps, in_arate, in_achans, in_inter, &outfmt, &infmt, FALSE);
#else
  int in_smps = ((double)out_smps * (double)in_arate / (double)out_arate + .999999);
#endif

  in_bytes = in_smps * (in_asamps >> 3);
  if (in_inter) in_bytes *= in_achans;

  // round in_bytes up to 65536
  in_bytes = ((in_bytes + 65535) >> 16) << 16;
  in_smps = in_bytes / (in_asamps >> 3);
  if (in_inter) in_smps /= in_achans;

  //
#if HAVE_SWRESAMPLE
  // in size fixed, out is max
  out_smps = get_swr_fmts(&swr_ctx, xin_asamps, in_arate, in_achans, in_inter, &in_smps,
                          -out_asamps, out_arate, out_nchans, FALSE, &infmt, &outfmt, TRUE);
  out_bytes = xout_smps * (out_asamps >> 3);
#endif

  double reticence = THREADVAR(loveliness) / AVG_LOVELINESS;
  if (is_preload) reticence = 0.;

  //////
  if (in_inter) {
    ssize_t read_bytes;
    ssize_t rem_bytes = in_bytes, in_offs = 0;



    inbuffs = LIVES_CALLOC_SIZEOF(void *, in_achans);
    inbuffs[0] = lives_malloc(in_bytes);
    if (weed_layer_get_audio_vel(layer) < 1.0)
      lives_buffered_rdonly_set_reversed(fd, TRUE);
    else lives_buffered_rdonly_set_reversed(fd, FALSE);
    lives_lseek_buffered_rdonly_absolute(fd, xseek);

    do {
      if (lives_proc_thread_get_pause_requested(self)) return;

      read_bytes = lives_read_buffered(fd, inbuffs[0] + in_offs, rem_bytes, TRUE);
      if (read_bytes < 0) {
        // throw err

      }
      rem_bytes -= read_bytes;
      in_offs += read_bytes;
      // check for EOF and set direction or reseek
      if (lives_read_buffered_eof(fd)) {

      }
    } while (rem_bytes >= 65536);

    in_bytes -= rem_bytes;
    in_smps = in_bytes / (in_asamps >> 3) / in_achans;

#if HAVE_SWRESAMPLE
    // in_smps fixed, get max out
    out_smps = get_swr_fmts(&swr_ctx, xin_asamps, in_arate, in_achans, in_inter, &in_smps,
                            -out_asamps, out_arate, out_nchans, FALSE, &infmt, &outfmt, TRUE);
#endif
    // max
    out_bytes = out_smps * (out_asamps >> 3);
    new_bb_size = bb_size - mv + out_bytes;
    //////////////////////////////////////////////////

    // pointer refs
    xbufs = LIVES_CALLOC_SIZEOF(void *, out_nchans);
    // construction buffers
    tmpbufs = LIVES_CALLOC_SIZEOF(void *, out_nchans);
    // out buffers
    if (!bb_buff) bb_buff = LIVES_CALLOC_SIZEOF(void *, out_nchans);
    for (int i = 0; i < out_nchans; i++) {
      ///
      tmpbufs[i] = lives_malloc(new_bb_size);
      ///
      if (!bb_buff[i]) xbufs[i] = tmpbufs[i];
      else {
        lives_memcpy(tmpbufs[i], bb_buff[i] + mv, bb_size - mv);
        xbufs[i] = tmpbufs[i] + bb_size - mv;
      }
    }

    /////////////////////////////////////////////////

    //g_print("reading in %ld %ld %d %ld (%ld) bytes\n", needed, out_bytes, in_smps, in_bytes, read_bytes);

    // lock buffer
    if (in_asamps != out_asamps || in_arate != out_arate || out_inter || in_achans != out_nchans) {
#if HAVE_SWRESAMPLE
      out_smps = sw_resample(xbufs, out_smps, inbuffs, in_smps, swr_ctx);
      /* g_print("RES %p, %d, %d, %d, %d ==  %d, %p %d %d %d %d ==  %d\n", */
      /*         inbuffs, in_smps, in_achans, in_arate, infmt, AV_SAMPLE_FMT_S16, */
      /*         xbufs, out_smps, out_nchans, out_arate, outfmt, AV_SAMPLE_FMT_FLTP); */
#else

#endif
    }
  }

  if (lives_proc_thread_get_pause_requested(self)) return;

  // recalculate
  out_bytes = out_smps * (out_asamps >> 3);
  new_bb_size = bb_size - mv + out_bytes;

  pthread_mutex_t *bufmutex =
    (pthread_mutex_t *)weed_get_voidptr_value(layer, "bufmutex", NULL);
  if (!bufmutex) {
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    bufmutex = LIVES_CALLOC_SIZEOF(pthread_mutex_t, 1);
    pthread_mutex_init(bufmutex, &mattr);
    weed_set_voidptr_value(layer, "bufmutex", bufmutex);
  }

  /////////////////////
  pthread_mutex_lock(bufmutex);
  ///////////////////////////////

  // position in input file
  weed_set_int64_value(layer, "bb_in_pos", xseek);
  weed_set_int64_value(layer, "bb_in_size", in_bytes);
  weed_set_int64_value(layer, "bb_max_in_samps", max_in_smps);

  bb_in_offs = weed_get_int64_value(layer, "bb_in_offs", NULL);
  weed_set_int64_value(layer, "bb_in_offs", bb_in_offs - bb_in_size);

  weed_set_voidptr_array(layer, "bb_buff", out_nchans, tmpbufs);

  bb_offs = weed_get_int64_value(layer, "bb_offs", NULL);
  weed_set_int64_value(layer, "bb_offs", bb_offs - mv);

  weed_set_int64_value(layer, "bb_size", new_bb_size);
  weed_set_voidptr_array(layer, "bb_buff", out_nchans, tmpbufs);

  pthread_mutex_unlock(bufmutex);

  //g_print("filled layer, size = %ld, buff == %p\n", new_bb_size, tmpbufs);

  for (int i = 0; i < out_nchans; i++)
    if (bb_buff[i]) lives_free(bb_buff[i]);

  for (int i = 0; i < in_achans; i++)
    if (inbuffs[i]) lives_free(inbuffs[i]);

  if (xbufs) lives_free(xbufs);
  if (tmpbufs) lives_free(tmpbufs);
  if (bb_buff) lives_free(bb_buff);

  ____FUNC_EXIT____;
}


static boolean fill_cbuffer(weed_layer_t *layer, lives_obj_instance_t *aplayer,
                            double vel, double clip_vol) {
  if (!layer || !aplayer) return TRUE;
  pthread_mutex_t *bufmutex = (pthread_mutex_t *)
                              weed_get_voidptr_value(layer, "bufmutex", NULL);
  if (!bufmutex) return FALSE;

  void **bb_buff = weed_get_voidptr_array(layer, "bb_buff", NULL);
  if (!bb_buff) return FALSE;

  int in_achans = weed_layer_get_naudchans(layer);
  int in_asamps = weed_layer_get_audio_asamps(layer);

  int out_nchans = in_achans;
  int out_asamps = in_asamps;

  int in_arate = weed_layer_get_audio_rate(layer);
  int out_arate = lives_aplayer_get_arate(aplayer);
  int nsamples = lives_aplayer_get_data_len(aplayer);

  int64_t sbf_size = weed_get_int64_value(layer, "sbf_size", NULL);

  int64_t bb_offs, bb_in_offs;

  void **sbf_buff = weed_get_voidptr_array(layer, "sbf_buff", NULL);
  void **in_datap = NULL, **out_datap = NULL;
  int insamples, xinsamples;
  size_t in_bytes, src_bytes;
  size_t out_bytes = nsamples * 4;
  int out_smps = nsamples;

  if (!sbf_buff) {
    sbf_buff = LIVES_CALLOC_SIZEOF(void *, out_nchans);
    sbf_size = 0;
  }

  vel = fabs(weed_layer_get_audio_vel(layer));

  vel /= mainw->audio_stretch;
  in_arate = (int)((double)in_arate * vel + .5);

  insamples = (int)((double)nsamples * vel * (double)in_arate / (double)out_arate + .999999);
  xinsamples = (int)((double)nsamples * vel * (double)in_arate / (double)out_arate  + .499999 + fastrand_dbl(1.));

#if HAVE_SWRESAMPLE
  ///struct SwrContext *swr_ctx = NULL;
  enum AVSampleFormat infmt, outfmt;
  // xnsamples is at least nsamples, insamples is exact
  int xnsamples = nsamples;
  int zinsamples = get_swr_fmts(&smbuf_ctx, -out_asamps, out_arate, out_nchans, FALSE, &xnsamples,
                                -in_asamps, in_arate, in_achans, FALSE, &outfmt, &infmt, FALSE);
  //g_print("XNSMPS is %d\n", xnsamples);
  int Xnsamples = xnsamples;
  zinsamples = get_swr_fmts(&smbuf_ctx, -out_asamps, out_arate, out_nchans, FALSE, &Xnsamples,
                            -in_asamps, in_arate, in_achans, FALSE, &outfmt, &infmt, FALSE);
  // g_print("XNSMPS2 is %d\n", Xnsamples);
  //g_print("zin is %d\n", zinsamples);

#endif

  //LIVES_ASSERT(insamples == zinsamples);

  in_bytes = insamples * (in_asamps >> 3);
  out_bytes = Xnsamples * (out_asamps >> 3);

  for (int i = 0; i < out_nchans; i++) {
    //g_print("rra %d. %ld + %ld\n", i, sbf_size, out_bytes);
    sbf_buff[i] = lives_realloc(sbf_buff[i], sbf_size + out_bytes);
  }

  weed_set_voidptr_array(layer, "sbf_buff", out_nchans, sbf_buff);
  out_bytes = nsamples * (out_asamps >> 3);

  int clip = lives_layer_get_clip(layer);
  lives_clip_t *sfile = RETURN_VALID_CLIP(clip);

  src_bytes = (int64_t)((double)(xinsamples * (sfile->asampsize >> 3)
                                 * (double)sfile->achans));
  if (in_arate == out_arate) {
    pthread_mutex_lock(bufmutex);
    bb_buff = weed_get_voidptr_array(layer, "bb_buff", NULL);
    bb_offs = weed_get_int64_value(layer, "bb_offs", NULL);
    out_bytes = xnsamples * (out_asamps >> 3);
    for (int i = 0; i < out_nchans; i++)
      lives_memcpy(sbf_buff[i] + sbf_size, bb_buff[i] + bb_offs, out_bytes);
    goto done;
  }

  in_datap = LIVES_CALLOC_SIZEOF(void *, in_achans);
  out_datap = LIVES_CALLOC_SIZEOF(void *, out_nchans);

  for (int i = 0; i < out_nchans; i++) out_datap[i] = sbf_buff[i] + sbf_size;

  pthread_mutex_lock(bufmutex);

  bb_buff = weed_get_voidptr_array(layer, "bb_buff", NULL);
  bb_offs = weed_get_int64_value(layer, "bb_offs", NULL);

  for (int i = 0; i < in_achans; i++)
    if (bb_buff[i]) in_datap[i] = bb_buff[i] + bb_offs;

#if HAVE_SWRESAMPLE

  //g_print("doing %d and %d\n", zinsamples, nsamples);
  out_smps = sw_resample(out_datap, xnsamples, in_datap, zinsamples + 1, smbuf_ctx);

  out_bytes = out_smps * 4;
#else

#endif

done:

  bb_offs = weed_get_int64_value(layer, "bb_offs", NULL);
  weed_set_int64_value(layer, "bb_offs", bb_offs + in_bytes);

  bb_in_offs = weed_get_int64_value(layer, "bb_in_offs", NULL);
  weed_set_int64_value(layer, "bb_in_offs", bb_in_offs + src_bytes);

  pthread_mutex_unlock(bufmutex);

  // apply clip volume
  if (clip_vol != 1.) {
    for (int i = 0; i < out_nchans; i++) {
      if (sbf_buff[i]) {
        void *start = sbf_buff[i] + sbf_size;
        void *end = start + out_bytes;
        for (float *smp = (float *)start; smp < (float *)end; smp++) *smp *= clip_vol;
      }
    }
  }

  sbf_size += out_bytes;

  weed_set_int64_value(layer, "sbf_size", sbf_size);
  weed_set_int64_value(layer, "sbf_newsize", out_bytes);

  if (out_datap) lives_free(out_datap);
  if (in_datap) lives_free(in_datap);
  if (sbf_buff) lives_free(sbf_buff);
  if (bb_buff) lives_free(bb_buff);
  return TRUE;
}



static void cache_audio(void) {
  // this is the thread function responsible for filling large audio buffers during playback
  // normally it is paused but can be wokne by sending a resume request
  // on resuming it check the value of local book variable (int) "this_op"
  // 1 indicates a normal buffer fill - if the remaining level is below a threshold we top it up
  // 2 indicates a seek and fill. Seek is done in two steps, first we seek to an approximate frame,
  //  which allows the buffer to be prefilled to a reduced level to reduce latency
  // this is then followed by a fine seek, which ideally can be handled by a fine adjutment to the
  // read point in the current buffer

  // this is all handled asynchrounously - the order is:
  // seek is initiated by a call to avsync_force or to lives_aplayer_seek_to
  // for seek_to, the aplayer specific seek_to is called
  // this in turn calls lives_aplayer_set_seek_vals which updates values in aplayer
  // within a mutex lock,

  weed_layer_t **layers = NULL, **xlayers;
  int nlayers = 0, xnlayers, i, op;

  GET_PROC_THREAD_SELF(self);

  while (1) {
    lives_proc_thread_pause();
    if (lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_cancel();
    }

    // check for new layers
    GET_SELF_ARRAY(xlayers, "layers", &xnlayers);
    DEL_SELF_VALUE("layers");

    //g_print("GOT %d layers %p\n", xnlayers, xlayers);
    if (!xlayers) xnlayers = 0;

    if (xlayers) {
      nlayers = xnlayers;
      layers = xlayers;
      xlayers = NULL;
      xnlayers = 0;
    }

    if (nlayers) {
      for (i = 0; i < nlayers; i++) {
        //g_print("will fill layer %d\n", i);
        fill_layer(layers[i]);
        if (lives_proc_thread_get_pause_requested(self)) break;
      }
    }

    /* for (i = 0; i < xnlayers; i++) { */
    /*   if (i >= nlayers || xlayers[i] != layers[i]) { */
    /* 	// layer changed or added */
    /* 	fill_layer(xlayers[i]); */
    /* 	if (i == nlayers) nlayers++; */
    /*   } */
    /* } */


  }
}

// value of op can be: -1 cancel, 0 get buffs param0 nsamps param1 vel, 1 update alayers,
// 2 reseek param0 new time, 3 dirchange, param0 direction. 4 pingpong param 0 set or not

lives_result_t audio_cache(int op, ...) {
  static weed_layer_t **layers = NULL;
  static int nlayers = 0;
  static lives_obj_instance_t *aplayer = NULL;
  static lives_proc_thread_t caud_lpt = NULL;

  lives_result_t res = LIVES_RESULT_SUCCESS;

  if (!caud_lpt)
    caud_lpt = lives_proc_thread_create(LIVES_THRDATTR_NO_GUI,
                                        cache_audio, WEED_SEED_VOID, "");

  if (op == -1) {
    // cancel
    lives_proc_thread_request_cancel(caud_lpt, FALSE);
    lives_proc_thread_join_void(caud_lpt);
    caud_lpt = NULL;
    return LIVES_RESULT_CANCELLED;
  }

  if (op == 0) {
    // small buffer fill
    if (!aplayer || !layers) return LIVES_RESULT_ERROR;

    int nsamples = lives_aplayer_get_data_len(aplayer);

    if (!nsamples) return res;
    T_array params;
    VA_TO_T_ARRAY(params, op, "ddb");
    double vel = params[0]->values.d[0];
    double clip_vol = params[1]->values.d[0];
    T_ARRAY_FREE(params);

    for (int i = 0; i < nlayers; i++) {
      if (!fill_cbuffer(layers[i], aplayer, vel, clip_vol))
        res = LIVES_RESULT_BUSY_RETRY;
    }


    if (lives_proc_thread_is_paused(caud_lpt)) {
      SET_LPT_ARRAY(caud_lpt, WEED_SEED_VOIDPTR, "layers", nlayers, layers);
      lives_proc_thread_request_resume(caud_lpt);
    }

    return res;
  } else {
    // big buffer fill
    T_array params;
    VA_TO_T_ARRAY(params, op, "iVP");
    nlayers = params[0]->values.i[0];
    //T_ARRAY_VAL(params, 0, i, 0);
    layers = (weed_layer_t **)params[1]->values.V[0];
    aplayer = (lives_obj_instance_t *)params[2]->values.P[0];
    T_ARRAY_FREE(params);
    // do we need to seek ?

    pthread_mutex_t *aplayer_seek_mutex =
      (pthread_mutex_t *)weed_get_voidptr_value(aplayer, "seekmutex", NULL);

    pthread_mutex_lock(aplayer_seek_mutex);

    SET_LPT_ARRAY(caud_lpt, WEED_SEED_VOIDPTR, "layers", nlayers, layers);

    seek_phase skstate = lives_aplayer_get_seek_state(aplayer);

    if (skstate == seek_active || skstate == seek_ready) {
      int new_clip = lives_aplayer_get_seek_clip(aplayer);
      double new_time  = lives_aplayer_get_seek_time(aplayer);
      lives_direction_t new_dir = lives_aplayer_get_seek_direction(aplayer);
      // get vals from aplayer

      pthread_mutex_unlock(aplayer_seek_mutex);
      // get clip number if invalid we maintain old value
      // if new clip does not have audio, we clean up
      //
      // get_seek time and convert to position
      // check if we can seek by adjusting bb_offs
      // be aware me may have a reduced buffer size after seek,
      // we can just load more
      ///
      // if not, we set bb_in_pos, set bb_in_size to 0, bb_offs
      //  set bb_buff / bb_size

      lives_clip_t *sfile = RETURN_VALID_CLIP(new_clip);
      if (sfile && CLIP_HAS_AUDIO(new_clip)) {
        int clip = lives_layer_get_clip(layers[0]);
        int in_arate = sfile->arate;
        int in_achans = sfile->achans;
        int in_asamps = sfile->asampsize;
        int new_smp_pos = new_time * in_arate;
        int64_t new_in_pos = new_smp_pos * (in_asamps >> 3) * in_achans;
        pthread_mutex_t *bufmutex =
          (pthread_mutex_t *)weed_get_voidptr_value(layers[0], "bufmutex", NULL);
        lives_proc_thread_request_pause(caud_lpt);
        lives_microsleep_while_false(lives_proc_thread_is_paused(caud_lpt));

        if (new_clip == clip) {
          int64_t bb_in_pos = weed_get_int64_value(layers[0], "bb_in_pos", NULL);
          int64_t bb_in_size = weed_get_int64_value(layers[0], "bb_in_size", NULL);
          int64_t bb_max_in_samps = weed_get_int64_value(layers[0], "bb_max_in_samps", NULL);
          int in_smp_pos = bb_in_pos / (in_asamps >> 3) / in_achans;

          if (new_smp_pos > in_smp_pos + (bb_max_in_samps >> 3)
              && new_smp_pos < in_smp_pos + bb_max_in_samps - (bb_max_in_samps >> 3)) {
            int out_arate = weed_layer_get_audio_rate(layers[0]);
            int out_asamps = weed_layer_get_audio_asamps(layers[0]);
            int64_t bb_in_offs = new_in_pos - bb_in_pos;
            int64_t bb_offs = (double)bb_in_offs * (double)out_arate / (double)in_arate / in_achans;
            bb_offs /= (out_asamps >> 3);
            bb_offs *= (out_asamps >> 3);
            if (bufmutex) pthread_mutex_lock(bufmutex);
            weed_set_int64_value(layers[0], "bb_in_offs", bb_in_offs);
            weed_set_int64_value(layers[0], "bb_offs", bb_offs);
            if (bufmutex) pthread_mutex_unlock(bufmutex);
            lives_proc_thread_ensure_resume(caud_lpt);
            return LIVES_RESULT_SUCCESS;
          }
        } else {
          int nchans = weed_layer_get_naudchans(layers[0]);
          if (bufmutex) pthread_mutex_lock(bufmutex);
          void **bb_buff = weed_get_voidptr_value(layers[0], "bb_buff", NULL);
          if (bb_buff) {
            for (int i = 0; i < nchans; i++) {
              if (bb_buff[i]) lives_free(bb_buff[i]);
              if (bb_buff[i]) lives_free(bb_buff[i]);
            }
          }

          weed_set_voidptr_value(layers[0], "bb_buff", NULL);
          weed_set_int64_value(layers[0], "bb_size", 0);

          weed_set_int64_value(layers[0], "bb_in_pos", new_in_pos);
          weed_set_int64_value(layers[0], "bb_in_size", 0);
          if (bufmutex) pthread_mutex_unlock(bufmutex);
        }

        SET_LPT_VALUE(caud_lpt, WEED_SEED_INT, "new_clip", new_clip);
        lives_proc_thread_ensure_resume(caud_lpt);
        return LIVES_RESULT_SUCCESS;
      } else {
        // switch to clip with no audio -- close buffers and free
        int fd = weed_get_int_value(layers[0], "abuff_id", NULL);
        if (fd > 0) {
          lives_close_buffered(fd);
          fd = -1;
          weed_set_int_value(layers[0], "abuff_id", fd);
        }
        // free layers + buffers
        pthread_mutex_t *bufmutex = (pthread_mutex_t *)
                                    weed_get_voidptr_value(layers[0], "bufmutex", NULL);
        if (bufmutex) pthread_mutex_lock(bufmutex);
        void **bb_buff = weed_get_voidptr_array(layers[0], "bb_buff", NULL);
        int nchans = weed_layer_get_naudchans(layers[0]);
        for (int i = 0; i < nchans; i++)
          if (bb_buff[i]) lives_free(bb_buff[i]);
        weed_set_voidptr_value(layers[0], "bb_buff", NULL);
        weed_set_int64_value(layers[0], "bb_size", 0);
        if (bufmutex) pthread_mutex_unlock(bufmutex);
      }
    }
    pthread_mutex_unlock(aplayer_seek_mutex);
    lives_proc_thread_ensure_resume(caud_lpt);
    return LIVES_RESULT_SUCCESS;
  }

  return LIVES_RESULT_INVALID;
}


void wake_audio_thread(void) {
  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  if (cache_buffer == cache_buffera) cache_buffer = cache_bufferb;
  else cache_buffer = cache_buffera;
  if (cache_buffer) cache_buffer->is_ready = FALSE;
  pthread_mutex_unlock(&mainw->cache_buffer_mutex);

  pthread_mutex_lock(&cond_mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&cond_mutex);
}


lives_audio_buf_t *audio_cache_init(void) {
  for (int i = 0; i < 2; i++) {
    cache_buffer = (lives_audio_buf_t *)lives_calloc(1, sizeof(lives_audio_buf_t));
    if (i == 0) cache_bufferb = cache_buffer;
    else cache_buffera = cache_buffer;
    cache_buffer->is_ready = FALSE;
    cache_buffer->die = FALSE;

    if (!mainw->multitrack) {
      cache_buffer->in_achans = 0;

      // NULLify all pointers of cache_buffer

      /* cache_buffer->buffer8 = NULL; */
      /* cache_buffer->buffer16 = NULL; */
      /* cache_buffer->buffer24 = NULL; */
      /* cache_buffer->buffer32 = NULL; */
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

      pthread_mutex_init(&cache_buffer->atomic_mutex, NULL);
    }
  }

  // init the audio caching thread for rt playback
  /* lives_proc_thread_create(LIVES_THRDATTR_NO_GUI | LIVES_THRDATTR_DONTCARE, */
  /*                          cache_my_audio, WEED_SEED_VOID, "v", &cache_buffer); */
  return cache_buffer;
}


void audio_cache_finish(void) {
  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  if (!cache_buffer) {
    pthread_mutex_unlock(&mainw->cache_buffer_mutex);
    return;
  }
  cache_buffera->die = cache_bufferb->die = TRUE; ///< tell cache thread to exit when possible
  pthread_mutex_unlock(&mainw->cache_buffer_mutex);
  wake_audio_thread();
}


void audio_cache_end(void) {
  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  lives_proc_thread_join_void(athread);
  lives_proc_thread_unref(athread);
  pthread_mutex_unlock(&mainw->cache_buffer_mutex);

  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  if (!mainw->event_list) {
    // free all buffers
    for (int c = 0; c < 2; c++) {
      if (!c) cache_buffer = cache_buffera;
      else cache_buffer = cache_bufferb;
      if (cache_buffer) {
        for (int i = 0; i < (cache_buffer->_cin_interleaf ? 1 : cache_buffer->_cachans); i++) {
          /* if (cache_buffer->buffer8 && cache_buffer->buffer8[i]) lives_free(cache_buffer->buffer8[i]); */
          if (cache_buffer->buffer16 && cache_buffer->buffer16[i]) lives_free(cache_buffer->buffer16[i]);
          /* if (cache_buffer->buffer24 && cache_buffer->buffer24[i]) lives_free(cache_buffer->buffer24[i]); */
          /* if (cache_buffer->buffer32 && cache_buffer->buffer32[i]) lives_free(cache_buffer->buffer32[i]); */
          if (cache_buffer->bufferf && cache_buffer->bufferf[i]) lives_free(cache_buffer->bufferf[i]);
        }

        /* if (cache_buffer->buffer8) lives_free(cache_buffer->buffer8); */
        if (cache_buffer->buffer16) lives_free(cache_buffer->buffer16);
        /* if (cache_buffer->buffer24) lives_free(cache_buffer->buffer24); */
        /* if (cache_buffer->buffer32) lives_free(cache_buffer->buffer32); */
        if (cache_buffer->bufferf) lives_free(cache_buffer->bufferf);

        if (cache_buffer->_filebuffer) lives_free(cache_buffer->_filebuffer);
      }
    }

    if (cache_buffera) {
      cache_buffer = cache_buffera;
      cache_buffera = NULL;
      if (cache_buffer->_fd != -1) lives_close_buffered(cache_buffer->_fd);
      pthread_mutex_destroy(&cache_buffer->atomic_mutex);
      lives_free(cache_buffer);
    }

    if (cache_bufferb) {
      cache_buffer = cache_bufferb;
      cache_bufferb = NULL;
      if (cache_buffer->_fd != -1 && (!cache_buffera || cache_buffer->_fd != cache_buffera->_fd))
        lives_close_buffered(cache_buffer->_fd);
      pthread_mutex_destroy(&cache_buffer->atomic_mutex);
      lives_free(cache_buffer);
    }
  }
  pthread_mutex_unlock(&mainw->cache_buffer_mutex);

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    jack_pb_end();
  }
#endif
}

LIVES_GLOBAL_INLINE lives_audio_buf_t *audio_cache_get_buffer(void) {
  lives_audio_buf_t *cb;
  pthread_mutex_lock(&mainw->cache_buffer_mutex);
  if (cache_buffer == cache_buffera)
    cb = cache_bufferb;
  else
    cb = cache_buffera;
  pthread_mutex_unlock(&mainw->cache_buffer_mutex);
  return cb;
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
  if (channel) {
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
      // allow ain thread to complete the reinit so we do not delay; just return silence
      weed_instance_unref(orig_inst);
      return FALSE;
    }

    weed_set_int64_value(channel, WEED_LEAF_TIMECODE, tc);

    weed_channel_set_audio_data(channel, fbuffer, arate, xnchans, nsamps);
    weed_set_double_value(inst, WEED_LEAF_FPS, cfile->pb_fps);
  }

  if (mainw->pconx && !(mainw->preview || mainw->is_rendering)) {
    // chain any data pipelines
    if (!filter_mutex_trylock(mainw->agen_key - 1)) {
      mainw->agen_needs_reinit =
        pconx_chain_data(mainw->agen_key - 1, rte_key_getmode(mainw->agen_key), is_audio_thread);
      filter_mutex_unlock(mainw->agen_key - 1);
      if (mainw->agen_needs_reinit) {
        // allow main thread to complete the reinit so we do not delay; just return silence
        weed_instance_unref(orig_inst);
        return FALSE;
      }
    }
  }

  retval = run_process_func(inst, tc);

  if (retval != WEED_SUCCESS) {
    if (retval == WEED_ERROR_REINIT_NEEDED) mainw->agen_needs_reinit = TRUE;
    weed_instance_unref(orig_inst);
    return FALSE;
  }

  if (channel && xnchans == 1 && nchans == 2) {
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
    append_silence(audio_fd, NULL, audio_pos, audio_end_pos, cfile->asampsize, cfile->signed_endian & AFORM_UNSIGNED,
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


boolean apply_rte_audio(int nsamples) {
  // CALLED When we are rendering audio to a file

  // - read nsamples from clip or generator
  // - convert to float if necessary
  // - send to rte audio effects
  // - convert back to s16 or s8
  // - save to audio_fd
  weed_layer_t *layer;
  size_t tbytes;
  uint8_t *in_buff;
  float **fltbuf, *fltbufni = NULL;
  int16_t *shortbuf = NULL;
  boolean rev_endian = FALSE;

  int i;

  int abigendian = cfile->signed_endian & AFORM_BIG_ENDIAN;
  int onsamples;

  // read nsamples of audio from clip or generator

  if ((abigendian && capable->hw.byte_order == LIVES_LITTLE_ENDIAN) || (!abigendian &&
      capable->hw.byte_order == LIVES_BIG_ENDIAN)) rev_endian = TRUE;

  tbytes = nsamples * cfile->achans * cfile->asampsize / 8;

  if (mainw->agen_key == 0) {
    if (tbytes + audio_pos > cfile->afilesize) tbytes = cfile->afilesize - audio_pos;
    if (tbytes <= 0) return TRUE;
    nsamples = tbytes / cfile->achans / (cfile->asampsize / 8);
  }

  onsamples = nsamples;

  in_buff = (uint8_t *)lives_calloc_safety(tbytes, 1);
  if (!in_buff) return FALSE;

  if (cfile->asampsize == 8) {
    shortbuf = (int16_t *)lives_calloc_safety(tbytes / sizeof(int16_t), sizeof(int16_t));
    if (!shortbuf) {
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
      sample_move_d8_d16(shortbuf, in_buff, nsamples, tbytes,
                         1.0, cfile->achans, cfile->achans, 0);
    } else shortbuf = (int16_t *)in_buff;

    nsamples = tbytes / cfile->achans / (cfile->asampsize / 8);

    // convert to float

    for (i = 0; i < cfile->achans; i++) {
      // convert s16 to non-interleaved float
      fltbuf[i] = (float *)lives_calloc(nsamples, sizeof(float));
      if (!fltbuf[i]) {
        while (i--) lives_free(fltbuf[i]);
        lives_free(fltbuf);
        if (shortbuf != (int16_t *)in_buff) lives_free(shortbuf);
        lives_free(in_buff);
        return FALSE;
      }
      lives_memset(fltbuf[i], 0, nsamples * sizeof(float));
      if (nsamples > 0) sample_move_d16_float(fltbuf[i], shortbuf + i, nsamples, cfile->achans, \
                                                (cfile->signed_endian & AFORM_UNSIGNED), rev_endian,
                                                lives_vol_from_linear(cfile->vol));
    }
  } else {
    // read from plugin. This should already be float.
    get_audio_from_plugin(fltbuf, cfile->achans, cfile->arate, nsamples, FALSE);
  }

  // apply any audio effects

  aud_tc += (double)onsamples / (double)cfile->arate * TICKS_PER_SECOND_DBL;
  // apply any audio effects with in_channels

  layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
  weed_layer_set_audio_data(layer, fltbuf, cfile->arate, cfile->achans, onsamples);
  weed_apply_audio_effects_rt(layer, aud_tc, FALSE, FALSE);
  lives_free(fltbuf);
  fltbuf = weed_layer_get_audio_data(layer, NULL);
  weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
  weed_layer_unref(layer);

  if (!(has_audio_filters(AF_TYPE_NONA) || mainw->agen_key != 0)) {
    // analysers only - no need to save (just render as normal)
    // or, audio is being generated (we rendered it to ascrap file)

    audio_pos += tbytes;

    if (!fltbufni) {
      for (i = 0; i < cfile->achans; i++) {
        lives_free(fltbuf[i]);
      }
    } else lives_free(fltbufni);

    lives_free(fltbuf);

    if (shortbuf != (int16_t *)in_buff) lives_free(shortbuf);
    lives_free(in_buff);

    return TRUE;
  }

  // convert float audio back to int
  sample_move_float_int(in_buff, fltbuf, onsamples, 1.0, cfile->achans, cfile->asampsize, (cfile->signed_endian & AFORM_UNSIGNED),
                        !(cfile->signed_endian & AFORM_BIG_ENDIAN), FALSE, 1.0);

  if (!fltbufni) {
    for (i = 0; i < cfile->achans; i++) {
      lives_free(fltbuf[i]);
    }
  } else lives_free(fltbufni);

  lives_free(fltbuf);

  if (audio_fd >= 0) {
    // save to file
    lives_lseek_buffered_writer(audio_fd, audio_pos);
    tbytes = onsamples * cfile->achans * cfile->asampsize / 8;
    lives_write_buffered(audio_fd, (const char *)in_buff, tbytes, FALSE);
    audio_pos += tbytes;
  }

  if (shortbuf != (int16_t *)in_buff) lives_free(shortbuf);
  lives_free(in_buff);

  if (THREADVAR(write_failed) == audio_fd + 1) {
    THREADVAR(write_failed) = 0;
    do_write_failed_error_s(audio_file, NULL);
    return FALSE;
  }

  return TRUE;
}


// read from arena, and possibly resample rate / chans / sampsize / inter
// we utilise the fact that layers and channels are interchangeable
int push_adata(weed_layer_t *alayer, int offset, int nsamples, int sampsize, int trate,
               int tsamps, int tchans, int tinter) {
  int olen = nsamples, xoffset = offset, nchans;
  boolean need_resamp = FALSE;
  void **tmp, **dst, **src;

#if HAVE_SWRESAMPLE
  struct SwrContext *swr_ctx = NULL;
  enum AVSampleFormat outfmt = 0, infmt = 0;
#endif
  lives_audio_buf_t *abuf = mainw->afbuffer;
  if (!abuf) return 0;

  if (abuf->arate != trate || sampsize != tsamps || abuf->in_achans != tchans || tinter) {
    need_resamp = TRUE;
#if HAVE_SWRESAMPL

    // nsamples exact, olen is max
    olen = get_swr_fmts(&swr_ctx, sampsize, abuf->arate, 2, FALSE, &nsamples,
                        tsamps, trate, tchans, FALSE, &outfmt, &infmt, TRUE);
#else
    double scale = (double)trate / (double)abuf->arate;
    olen = (size_t)(fabs(((double)nsamples * scale)) + .49999);
#endif
  }

  sampsize = abs(sampsize);
  tsamps = abs(tsamps);

  src = LIVES_CALLOC_SIZEOF(void *, abuf->in_achans);

  for (int i = 0; i < tchans; i++) {
    int j = i % abuf->in_achans;
    src[i] = abuf->bufferf[j];
  }

  mainw->debug_ptr = NULL;
  dst = (void **)weed_layer_get_audio_data(alayer, &nchans);
  if (!dst) dst = LIVES_CALLOC_SIZEOF(void *, tchans);
  else {
    for (int i = 0; i < nchans; i++) {
      if (dst[i] && !((char *)dst[i] - (char *)src[i] <= ABUF_ARENA_SIZE)) {
        lives_free(dst[i]);
        dst[i] = NULL;
      }
    }
  }

  if (!need_resamp) {
    if (abuf->write_pos >= offset + nsamples) {
      for (int i = 0; i < tchans; i++) dst[i] = src[i] + offset * (sampsize >> 3);
      weed_layer_set_audio_data(alayer, (float **)dst, trate, tchans, nsamples);
      return offset + nsamples;
    }
  }

  tmp = LIVES_CALLOC_SIZEOF(void *, tchans);

  for (int i = 0; i < nchans; i++) {
    dst[i] = lives_calloc(olen, (tsamps >> 3));
  }

  // need to reample or flatten
  for (int i = 0; i < tchans; i++) {
    if (need_resamp)
      tmp[i] = lives_calloc(nsamples, (sampsize >> 3));
    else tmp[i] = dst[i];
    xoffset = arena_read(tmp[i], (void *)src[i], mainw->afbuffer, offset, nsamples, (sampsize >> 3));
  }

  if (need_resamp) {
#if HAVE_SWRESAMPLE
    sw_resample(dst, olen, tmp, nsamples, swr_ctx);
#else
    // todo
#endif
    for (int i = 0; i < tchans; i++) lives_free(tmp[i]);
  }

  // set channel values
  //  g_print("SET alayer %p from %p %d %d %lu\n", alayer, dst, trate, tchans, olen);
  weed_layer_set_audio_data(alayer, (float **)dst, trate, tchans, olen);

  lives_free(dst);
  lives_free(tmp);
  lives_free(src);
  return xoffset;
}


/**
   @brief fill the audio channel(s) for effects with mixed audio / video

   There are three methods by which a plugin can access the audio data:
   - quick read / write
   - client pull
   - server push

   This function is for client pull
   This is called from fill_audio_channel(filter, achan)
   The client (filter) must first register with register_audio_client(is_vid) to ensure that
   the arena buffer gets filled

   The arena is filled by the output from the audio player afer passing through premix, fx, post mix stages
   each client will keep track of its own in and out points and will pull audio from its offset up to MIN(offset + maxsamples, write_pos)
   (allowing for the end wrap around). maxsamples is either unlimited, or defined for the filter or channel template
   Audio will be resampled to channel rate, nchans, as defined in the filter or channel template.
   There are no time constraints on how long the client an take to process data, except that if the offset falls too far behind
   it can be lapped (overtaken) by the write position
   If the client pulls data too rapidly there may be no new samples available.
   If it pulls too slowly, the lenght will increase and could cause buffer overflows.
   Thus - each client can set a min fill size and a max fill size
   If nsamps available < min fill size, the old data will be retained and LIVES_RESULT_BUSY_RETRY retunred
   If nsamps > max fill size. the buffer will be filled with the most recent samples and LIVES_RESULT_FAILED returned

   In addition, the time to process n samples will be measured, and from this we can calulate the max sample rate for the
   client. The client can reduce the sample rate to avoid buffer overruns.

   Server push
   - add a callback to the player's data_ready hook. All callbacks here are run asynch, the player will trigger them, then join
   them as they complete, then retrigger.
   Each client gets a
   offset + length will become offset for the next trigger

   The arena buffer is filled as a calback here. These clients can run slower, but must not write to the arena.
*/
lives_result_t pull_audio_for_channel(weed_plant_t *filter, weed_channel_t *achan, lives_audio_buf_t *abuf) {
  weed_plant_t *ctmpl;
  boolean rvary = FALSE, lvary = FALSE;
  int trate, tchans, xnchans, flags;
  int offset, samps, wrtpos;
  int minsamps, maxsamps;
  boolean overflow = FALSE;

  if (!weed_plant_has_leaf(achan, LIVES_LEAF_OFFSET)) {
    weed_set_int_value(achan, LIVES_LEAF_OFFSET, abuf->write_pos);
    return LIVES_RESULT_BUSY_RETRY;
  }

  offset = weed_get_int_value(achan, LIVES_LEAF_OFFSET, NULL);
  wrtpos = abuf->write_pos;

  //g_print("offset == %d, wrpos == %d\n", offset, wrtpos);
  if (wrtpos >= offset)
    samps = abuf->write_pos - offset;
  else samps = (ABUF_ARENA_SIZE >> 2) - offset + wrtpos;

  if (!samps || !abuf->arate) {
    weed_channel_set_audio_data(achan, NULL, 0, 0, 0);
    return LIVES_RESULT_ERROR;
  }
  minsamps = weed_get_int_value(achan, "min_samps", NULL);
  if (samps < minsamps) return LIVES_RESULT_BUSY_RETRY;

  maxsamps = weed_get_int_value(achan, "max_samps", NULL);
  if (maxsamps && samps > maxsamps) {
    offset += samps - maxsamps;
    if (offset > (ABUF_ARENA_SIZE >> 2)) offset -= (ABUF_ARENA_SIZE >> 2);
    samps = maxsamps;
    overflow = TRUE;
  }


  if (filter) {
    ctmpl = weed_channel_get_template(achan);
    flags = weed_filter_get_flags(filter);
    if (flags & WEED_FILTER_AUDIO_RATES_MAY_VARY) rvary = TRUE;
    if (flags & WEED_FILTER_CHANNEL_LAYOUTS_MAY_VARY) lvary = TRUE;

    if (!has_audio_chans_out(filter, FALSE)) {
      int maxlen = weed_chantmpl_get_max_audio_length(ctmpl);
      if (maxlen > 0) {
        if (abuf->in_interleaf) maxlen *= abuf->in_achans;
        if (maxlen < samps) {
          offset += samps - maxlen;
          if (offset >= (ABUF_ARENA_SIZE >> 2)) offset -= (ABUF_ARENA_SIZE >> 2);
          samps = maxlen;
        }
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
  } else {
    trate = weed_channel_get_audio_rate(achan);
    if (!trate) trate = abuf->arate;
    tchans = weed_channel_get_naudchans(achan);
    if (!tchans) tchans = abuf->in_achans;
  }

#ifdef DEBUG_AFB
  g_print("push from afb %d %p len %d\n", offset, abuf->bufferf, samps);
#endif

  offset = push_adata(achan, offset, samps, -32, trate, -32, tchans, FALSE);
  weed_set_int_value(achan, LIVES_LEAF_OFFSET, offset);

  // push to alayer "audio_data", taking into account "audio_data_length" and "audio_channels"
  if (overflow) return LIVES_RESULT_FAILED;
  return LIVES_RESULT_SUCCESS;

}


////////////////////////////////////////
// audio streaming, older API

lives_pid_t astream_pid = 0;

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

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    arate = (int)mainw->jackd->sample_out_rate;
    // TODO - chans, samps, signed, endian

  }
#endif

  astreamer = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_AUDIO_STREAM, playername, NULL);
  com = lives_strdup_printf("%s play %lu \"%s\" \"%s\" %d", astreamer, mainw->vpp->audio_codec, astream_name, astream_name_out,
                            arate);
  lives_free(astreamer);

  astream_pid = lives_fork(com);

  alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

  do {
    // wait for other thread to create stream (or timeout)
    afd = lives_open2(astream_name, O_WRONLY | O_SYNC);
    if (afd != -1) break;
    lives_usleep(prefs->sleep_time);
  } while ((timeout = lives_alarm_check(alarm_handle)) > 0);
  lives_alarm_clear(alarm_handle);

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    mainw->pulsed->astream_fd = afd;
  }
#endif

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    mainw->jackd->astream_fd = afd;
  }
#endif

  lives_free(astream_name);
  lives_free(astream_name_out);

  return TRUE;
}


void lives_aplayer_prepare(int clip) {
  // start up our audio player
  IF_APLAYER_JACK(jack_aud_pb_ready(mainw->jackd, clip););
  IF_APLAYER_PULSE(pulse_aud_pb_ready(mainw->pulsed, clip););
}


void stop_audio_stream(void) {
  if (astream_pid > 0) {
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

#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player == AUD_PLAYER_PULSE) {
      if (mainw->pulsed->astream_fd > -1) close(mainw->pulsed->astream_fd);
      mainw->pulsed->astream_fd = -1;
    }
#endif
#ifdef ENABLE_JACK
    if (prefs->audio_player == AUD_PLAYER_JACK) {
      if (mainw->jackd->astream_fd > -1) close(mainw->jackd->astream_fd);
      mainw->jackd->astream_fd = -1;
    }
#endif

    lives_killpg(astream_pid, LIVES_SIGKILL);
    lives_rm(astream_name);
    lives_free(astream_name);

    // astreamer should remove cooked stream
    com = lives_strdup_printf("\"%s\" cleanup %lu \"%s\"", astreamer, mainw->vpp->audio_codec, astream_name_out);
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
  if (fd > -1) lives_write(fd, buff, nbytes, TRUE);
}


LIVES_GLOBAL_INLINE lives_cancel_t handle_audio_timeout(void) {
  char *msg2 = (prefs->audio_player == AUD_PLAYER_PULSE) ? lives_strdup(
                 _("\nClick Retry to attempt to restart the audio server.\n")) :
               lives_strdup("");

  char *msg = lives_strdup_printf(
                _("LiVES was unable to connect to %s.\nPlease check your audio settings and restart %s\n"
                  "and LiVES if necessary.\n%s"),
                audio_player_get_display_name(prefs->aplayer),
                audio_player_get_display_name(prefs->aplayer), msg2);
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    int retval = do_abort_retry_cancel_dialog(msg);
    if (retval == LIVES_RESPONSE_RETRY) pulse_try_reconnect();
    else {
      mainw->aplayer_broken = TRUE;
      switch_aud_to_none(FALSE);
    }
#endif
  } else {
    do_error_dialog(msg);
    mainw->aplayer_broken = TRUE;
    switch_aud_to_none(FALSE);
  }
  lives_free(msg); lives_free(msg2);
  return CANCEL_ERROR;
}


//////////// objects / intents //////

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_seek_time(lives_obj_t *aplayer, double xtime) {
  return weed_set_double_value(aplayer, LIVES_LEAF_SEEK_TIME, xtime);
}

LIVES_GLOBAL_INLINE double lives_aplayer_get_seek_time(lives_obj_t *aplayer) {
  return weed_get_double_value(aplayer, LIVES_LEAF_SEEK_TIME, NULL);
}

LIVES_GLOBAL_INLINE  weed_error_t lives_aplayer_set_seek_clip(lives_obj_t *aplayer, int clip) {
  return weed_set_int_value(aplayer, LIVES_LEAF_SEEK_CLIP, clip);
}

LIVES_GLOBAL_INLINE int lives_aplayer_get_seek_clip(lives_obj_t *aplayer) {
  return weed_get_int_value(aplayer, LIVES_LEAF_SEEK_CLIP, NULL);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_seek_direction(lives_obj_t *aplayer, lives_direction_t dir) {
  return weed_set_int_value(aplayer, LIVES_LEAF_SEEK_DIR, (int)dir);
}

LIVES_GLOBAL_INLINE lives_direction_t lives_aplayer_get_seek_direction(lives_obj_t *aplayer) {
  return weed_get_int_value(aplayer, LIVES_LEAF_SEEK_DIR, NULL);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_seek_velocity(lives_obj_t *aplayer, double vel) {
  return weed_set_double_value(aplayer, LIVES_LEAF_SEEK_VEL, vel);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_get_seek_velocity(lives_obj_t *aplayer) {
  return weed_get_double_value(aplayer, LIVES_LEAF_SEEK_VEL, NULL);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_limit_behavior(lives_obj_t *aplayer,
    limit_behaviour_t low,
    limit_behaviour_t high) {
  int vals[2];
  vals[0] = low;
  vals[1] = high;
  return lives_obj_instance_set_attr_array(aplayer, ATTR_AUDIO_LIMIT_ACT, 2, &vals);
}


void lives_aplayer_update_loop_mode(lives_obj_t *aplayer) {
  if (mainw->whentostop != STOP_ON_AUD_END && !mainw->preview) {
    if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && !mainw->multitrack
        && (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
            || ((prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG))))
      lives_aplayer_set_limit_behavior(aplayer, limit_bounce, limit_bounce);
    else lives_aplayer_set_limit_behavior(aplayer, limit_rollover, limit_rollover);
  } else lives_aplayer_set_limit_behavior(aplayer, limit_stop, limit_stop);
}


LIVES_GLOBAL_INLINE int *lives_aplayer_get_limit_behaviour(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_LIMIT_ACT);
  return lives_attribute_get_array_int(param);
}


void lives_aplayer_set_seek_vals(lives_obj_t *aplayer, int clip, double xtime,
                                 lives_direction_t dir, double vel) {
  pthread_mutex_t *aplayer_seek_mutex =
    (pthread_mutex_t *)weed_get_voidptr_value(aplayer, "seekmutex", NULL);
  pthread_mutex_lock(aplayer_seek_mutex);
  lives_aplayer_set_seek_clip(aplayer, clip);
  lives_aplayer_set_seek_time(aplayer, xtime);
  lives_aplayer_set_seek_direction(aplayer, dir);
  lives_aplayer_set_seek_velocity(aplayer, vel);
  pthread_mutex_unlock(aplayer_seek_mutex);
}


LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_seek_state(lives_obj_t *aplayer, seek_phase state) {
  seek_phase old = lives_aplayer_get_seek_state(aplayer);
  if (state == seek_active && old !=  not_seeking) return WEED_SUCCESS;
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_SEEK_STATE, (int)state);
}

LIVES_GLOBAL_INLINE seek_phase lives_aplayer_get_seek_state(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_SEEK_STATE);
  return lives_attribute_get_value_int(param);
}


LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_seek_dir(lives_obj_t *aplayer, lives_direction_t dir) {
  return weed_set_int_value(aplayer, LIVES_LEAF_SEEK_DIR, dir);
}


LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_seek_vel(lives_obj_t *aplayer, double vel) {
  return weed_set_double_value(aplayer, LIVES_LEAF_SEEK_VEL, vel);
}


LIVES_GLOBAL_INLINE int lives_aplayer_get_source(lives_obj_t *aplayer) {
  return GET_ATTR_VALUE(aplayer, int, ATTR_AUDIO_SOURCE);

  //weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_SOURCE);
  // return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_source(lives_obj_t *aplayer, int source) {
  //mainw->debug = TRUE;
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_SOURCE, source);
}

LIVES_GLOBAL_INLINE int lives_aplayer_get_arate(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_RATE);
  return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_arate(lives_obj_t *aplayer, int arate) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_RATE, arate);
}

LIVES_GLOBAL_INLINE double lives_aplayer_get_velocity(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_VELOCITY);
  return lives_attribute_get_value_double(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_velocity(lives_obj_t *aplayer, double velocity) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_VELOCITY, velocity);
}


// clip is not an official attribute

LIVES_GLOBAL_INLINE int lives_aplayer_get_clip(lives_obj_t *aplayer) {
  return aplayer ? weed_get_int_value(aplayer, WEED_LEAF_CLIP, NULL) : -1;
}


LIVES_GLOBAL_INLINE void lives_aplayer_set_clip(lives_obj_t *aplayer, int clipno) {
  if (aplayer) weed_set_int_value(aplayer, WEED_LEAF_CLIP, clipno);
}

////////// position in input stream in bytes

LIVES_GLOBAL_INLINE int64_t lives_aplayer_get_pos(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_POSITION);
  return lives_attribute_get_value_int64(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_pos(lives_obj_t *aplayer, int64_t pos) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_POSITION, pos);
}


int64_t lives_aplayer_get_tot_samps(lives_obj_t *aplayer) {
  lives_attribute_t *attr = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_SMPS_PROCESSED);
  return lives_attribute_get_value_int64(attr);
}

weed_error_t lives_aplayer_add_nsamps(lives_obj_t *aplayer, int64_t nsamps) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_SMPS_PROCESSED,
                                         lives_aplayer_get_tot_samps(aplayer) + nsamps);
}

weed_error_t lives_aplayer_reset_nsamps(lives_obj_t *aplayer) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_SMPS_PROCESSED, 0);
}

/////

LIVES_GLOBAL_INLINE lives_direction_t lives_aplayer_get_direction(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_DIRECTION);
  return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_direction(lives_obj_t *aplayer, lives_direction_t dir) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_DIRECTION, dir);
}

LIVES_GLOBAL_INLINE int lives_aplayer_get_achans(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_CHANNELS);
  return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_achans(lives_obj_t *aplayer, int achans) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_CHANNELS, achans);
}

LIVES_GLOBAL_INLINE int lives_aplayer_get_sampsize(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_SAMPSIZE);
  return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_sampsize(lives_obj_t *aplayer, int asamps) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_SAMPSIZE, asamps);
}

LIVES_GLOBAL_INLINE uint64_t lives_aplayer_get_status(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_STATUS);
  return lives_attribute_get_value_uint64(param);
}

LIVES_GLOBAL_INLINE uint64_t lives_aplayer_get_active_status(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_STATUS);
  return lives_attribute_get_value_uint64(param) & 3;
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_status(lives_obj_t *aplayer, uint64_t status) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_STATUS, status);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_active_status(lives_obj_t *aplayer, uint64_t astatus) {
  uint64_t status = lives_aplayer_get_status(aplayer);
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_STATUS,
                                         (status & ~APLAYER_ASTATUS_MASK)
                                         | (astatus & APLAYER_ASTATUS_MASK));
}

LIVES_GLOBAL_INLINE boolean lives_aplayer_get_signed(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_SIGNED);
  return lives_attribute_get_value_boolean(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_signed(lives_obj_t *aplayer, boolean asigned) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_SIGNED, asigned);
}

LIVES_GLOBAL_INLINE int lives_aplayer_get_endian(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_ENDIAN);
  return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_endian(lives_obj_t *aplayer, int aendian) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_ENDIAN, aendian);
}

LIVES_GLOBAL_INLINE boolean lives_aplayer_get_float(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_FLOAT);
  return lives_attribute_get_value_boolean(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_float(lives_obj_t *aplayer, boolean is_float) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_FLOAT, is_float);
}

LIVES_GLOBAL_INLINE int lives_aplayer_get_data_len(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_DATA_LENGTH);
  return lives_attribute_get_value_int(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_data_len(lives_obj_t *aplayer, int alength) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_DATA_LENGTH, alength);
}

LIVES_GLOBAL_INLINE boolean lives_aplayer_get_interleaved(lives_obj_t *aplayer) {
  weed_param_t *param = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_INTERLEAVED);
  return lives_attribute_get_value_boolean(param);
}

LIVES_GLOBAL_INLINE weed_error_t lives_aplayer_set_interleaved(lives_obj_t *aplayer, boolean ainter) {
  return lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_INTERLEAVED, ainter);
}

LIVES_GLOBAL_INLINE void **lives_aplayer_get_data(lives_obj_t *aplayer) {
  weed_param_t *paramd = lives_obj_instance_get_attribute(aplayer, ATTR_AUDIO_DATA);
  return weed_get_voidptr_array(paramd, WEED_LEAF_VALUE, NULL);
}

LIVES_GLOBAL_INLINE lives_result_t lives_aplayer_set_data(lives_obj_t *aplayer, void **data) {
  if (aplayer) {
    if (!data) {
      lives_obj_instance_set_attr_val(aplayer, ATTR_AUDIO_DATA, NULL);
      return LIVES_RESULT_SUCCESS;
    } else {
      int nchans = lives_aplayer_get_achans(aplayer);
      if (nchans > 0) {
        if (lives_obj_instance_set_attr_array(aplayer, ATTR_AUDIO_DATA,
                                              nchans, data) == WEED_SUCCESS)
          return LIVES_RESULT_SUCCESS;
        return LIVES_RESULT_ERROR;
      }
      return LIVES_RESULT_FAILED;
    }
  }
  return LIVES_RESULT_INVALID;
}


void show_aplayer_attribs(LiVESWidget * w, void **player) {
  lives_rfx_t *rfx = NULL;
  char *title = NULL;
#ifdef HAVE_PULSE_AUDIO
  if (*player == (void *)mainw->pulsed) {
    rfx = mainw->pulsed->interface;
    title = _("Audio Player Details");
  } else if (*player == (void *)mainw->pulsed_read) {
    rfx = mainw->pulsed_read->interface;
    title = _("Audio Reader Details");
  }
#endif
  if (rfx) {
    LiVESWidget *dialog = rfx_make_param_dialog(rfx, title, FALSE);
    lives_dialog_run(LIVES_DIALOG(dialog));
    lives_widget_destroy(dialog);
  }
  if (title) lives_free(title);
}


lives_result_t await_audio_queue(uint64_t nsec) {
  lives_sys_alarm_set_timeout(audio_msgq_timeout, nsec);

  IF_APLAYER_JACK
  (lives_microsleep_while_true(jack_get_msgq(mainw->jackd) && !lives_sys_alarm_triggered(audio_msgq_timeout));
   if (lives_sys_alarm_disarm(audio_msgq_timeout, TRUE) && jack_get_msgq(mainw->jackd)) return LIVES_RESULT_FAIL;)

    IF_APLAYER_PULSE
    (lives_microsleep_while_true(pulse_get_msgq(mainw->pulsed) && !lives_sys_alarm_triggered(audio_msgq_timeout));
     if (lives_sys_alarm_disarm(audio_msgq_timeout, TRUE) && pulse_get_msgq(mainw->pulsed)) return LIVES_RESULT_FAIL;)

      return LIVES_RESULT_SUCCESS;
}



// fill_layer()
// layer should have an audio_src
// if it is a file, we calculate nsamples from arate
// read in as much as we can, then resample from in vals to out vals

// if we already have audio, if amy remaining < threshold, shift down and top up
//
//
// we use a ring buffer here
// if we are reading forward, we have the following
//
// <rev space > offs <fwd space>
//     0.25            0.75
// initially
//
// <rev space> offs <fwd space>
// 0.5< x <0.75    0.5> x > 0.25
//  top up !
// this is buffer 0
//
// if vel < 0., then fwd space, rev space are swapped.
//
// this is handled by the file buffering system, as well as reversing the buffer
// if playback direction changes, we do nothing - the inversion will happen when read.
// however we reverse for buffer 1, first we ensure fwd remaining >= 0.25,
// then we copy fwd remaining ->rev space, limited to .25, then copy rev space reversed, then top up

// in free playback, we read and store at the internal arate, achans, asamps. float
// since velocity is instantaneous, we resample at output time
//
// We read in based on time values = nseconds * arate * achans * (asamps >> 3)
// supposing max time was 10 sec, arate 96000, achans = 2, asamps = 32, size of buffer is 96000 * 10 * 2 * 4 == 7.68 MB
// we double this for 2 ring buffers - 15.35MB,
// if we wanted to limit audio cache space to 100MB, we could cache up to 6 layers. For 192K, 3 layers.
//
// Thus we limit arate to 2 X player rate.
//
// layer will have value to determine what happens when we reach eof fwd or 0 backward
// for ping pong, either we seek to other end and continue or we invert velocity
// when this happens we set a value - the offset where this first happens.
// If the user enables or disables ping pong, we set buffer 0 invalid from this point, and in buffer 1, re read the part from
// there onwards.
//
// copy offs.
// copy rev space -> 0.25 in buffer1, offs, copy remainder of fwd space, top up fwd space
//
// thus - each buffer 0, 1 has reversed, first eof, invalid from, ping pong,
// the associated layer has -  arate, asamps, etc
// the layer has a buffered reader in ringbuff mode
// we have a mutex which the player locks, and a call to swap buffers.
// we actually have 3 buffers 0 - in use by player; 1 - buffer to be swapped in; 2 - new data to be swapped into 1
// the swap call locks 1, and swaps it into 0, unlocks 1, update waits for 1 to be unlocked, locks 1, swaps in 2.
// buffers have reversed flag, dir change is applied to buffer 2
// altering ping pong sets invalid from in buffer 0 and buffer 1
//
//
// there aew 3 situations to handle separateley - reseek / clip change / direction change
// ping-pong change
