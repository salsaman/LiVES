/// LiVES - avformat plugin
// (c) G. Finch 2010 - 2016 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details
//
// some code adapted from vlc (GPL v2 or higher)

#include "decplugin.h"

#define HAVE_AVCODEC
#define HAVE_AVUTIL

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-compat.h>
#else
#include "../../../libweed/weed-compat.h"
#endif

///////////////////////////////////////////////////////
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

/* ffmpeg header */
//#if defined(HAVE_LIBAVFORMAT_AVFORMAT_H)
//#   include <libavformat/avformat.h>
//#elif defined(HAVE_FFMPEG_AVFORMAT_H)
//#   include <ffmpeg/avformat.h>
//#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>

#include "libav_helper.h"

#include "avformat_decoder.h"

const char *plugin_version = "LiVES avformat decoder version 1.0";

static pthread_mutex_t avcodec_mutex = PTHREAD_MUTEX_INITIALIZER;

#define FAST_SEEK_LIMIT 50000 // microseconds (default 0.1 sec)
#define NO_SEEK_LIMIT 2000000 // microseconds
#define LNO_SEEK_LIMIT 10000000 // microseconds

#define SEEK_SUCCESS_MIN_RATIO 0.5 // how many frames we can seek to vs. suggested duration

static void lives_avcodec_lock(void) {
  pthread_mutex_lock(&avcodec_mutex);
}


static void lives_avcodec_unlock(void) {
  pthread_mutex_unlock(&avcodec_mutex);
}


/* static int stream_peek(int fd, unsigned char *str, size_t len) { */
/*   off_t cpos = lseek(fd, 0, SEEK_CUR); // get current posn */
/*   int rv = pread(fd, str, len, cpos); // read len bytes without changing cpos */

/*   if (rv == -1) { */
/*     fprintf(stderr, "err is %d\n", errno); */
/*   } */
/*   return rv; */
/* } */


void get_samps_and_signed(enum AVSampleFormat sfmt, int *asamps, boolean *asigned) {
  *asamps = av_get_bits_per_sample(sfmt);

  switch (sfmt) {
  case AV_SAMPLE_FMT_U8:
    *asigned = FALSE;
    break;
  default:
    *asigned = TRUE;
  }
}


static int64_t get_current_ticks(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}


#define FRAMES_GUESS 32768
#define HALF_ROUND_UP(x) (x > 0 ? (int)(x / 2. + .5) : (int)(x / 2. - .5))

static int64_t get_real_last_frame(lives_clip_data_t *cdata, boolean allow_longer_seek) {
  int64_t diff = 0;

  // lframe is the frame we will check, olframe is the current seek base frame, maxframes is the largest found
  int64_t olframe = cdata->nframes - 1, lframe = olframe;
  int64_t timex, tottime = 0;
  long no_seek_limit = NO_SEEK_LIMIT;

  int nseeks = 0;
  boolean have_upper_bound = FALSE;
  boolean have_lower_bound = FALSE;

  if (allow_longer_seek) no_seek_limit = LNO_SEEK_LIMIT;
  cdata->seek_flag = LIVES_SEEK_FAST;

  // check we can get at least one frame
  if (!get_frame(cdata, 0, NULL, 0, NULL)) {
    return -1;
  }

  if (cdata->nframes == 0) {
    // if we got a broken file with 0 last frame, search for it
    olframe = lframe = FRAMES_GUESS - 1;
    diff = -FRAMES_GUESS;
    have_lower_bound = TRUE;
  }

  while (1) {
    nseeks++;
    //#define DEBUG_RLF
#ifdef DEBUG_RLF
    fprintf(stderr, "will check frame %ld of (allegedly) %ld...", lframe + 1, cdata->nframes);
#endif

    timex = get_current_ticks();

    // see if we can find lframe
    if (!get_frame(cdata, lframe, NULL, 0, NULL)) {
      // lframe not found

      have_upper_bound = TRUE; // we got an upper bound

      timex = get_current_ticks() - timex;

      if (timex > no_seek_limit) {
        // seek took too long, give up
#ifdef DEBUG_RLF
        fprintf(stderr, "avcodec_decoder: seek was %ld, longer than limit of %ld; giving up.\n", timex, no_seek_limit);
#endif
        return -1;
      }

#ifdef DEBUG_RLF
      fprintf(stderr, "no (%ld)\n", timex);
#endif
      if (diff == 1) {
        // this is the ideal situation, we got lframe - 1, but not lframe
        tottime /= nseeks;
#ifdef DEBUG_RLF
        fprintf(stderr, "av seek was %ld\n", tottime);
#endif
        return lframe - 1;
      }
      if (diff == 0) {
        diff = -1; // initial frame not found, start a backwards scan
        have_lower_bound = FALSE;
      } else {
        if (!have_lower_bound) diff *= 2; // found no frame yet, search backwards more
        else diff = HALF_ROUND_UP(diff); // found a frame, or started with a guess, search backwards less
      }

      if (diff > 0) diff = -diff;

      if (diff < -olframe) diff = -olframe; // can't jump to a negative frame !
    } else {
      // we did find a frame
      have_lower_bound = TRUE;

      timex = get_current_ticks() - timex;

      if (timex > no_seek_limit) {
#ifdef DEBUG_RLF
        fprintf(stderr, "avcodec_decoder: seek was %ld, longer than limit of %ld; giving up.\n", timex, no_seek_limit);
#endif
        return -1;
      }

      if (timex > FAST_SEEK_LIMIT)
        cdata->seek_flag = LIVES_SEEK_NEEDS_CALCULATION;

      tottime += timex;
#ifdef DEBUG_RLF
      fprintf(stderr, "yes ! (%ld)\n", timex);
#endif
      if (diff == 0) diff = 1; // initial frame found, start a forward scan
      else {
        if (diff < 2 && diff > -2) {
          // this is the ideal situation, we got lframe, but not lframe + 1
          tottime /= nseeks;
#ifdef DEBUG_RLF
          fprintf(stderr, "av seek was %ld\n", tottime);
#endif
          if (tottime > FAST_SEEK_LIMIT)
            cdata->seek_flag |= LIVES_SEEK_NEEDS_CALCULATION;
          return lframe;
        }
        if (have_upper_bound) diff = HALF_ROUND_UP(diff); // we found a null frame
        else diff *= 2;
        if (diff < 0) diff = -diff; // we were searching backwards, now we want to go forwards
      }

    }
    olframe = lframe;
    lframe += diff;
  }
}


static boolean attach_stream(lives_clip_data_t *cdata, boolean isclone) {
  // open the file and get a handle
  lives_av_priv_t *priv = cdata->priv;
  AVInputFormat *fmt;
  AVCodec *vdecoder;
  AVStream *s;
  AVCodecContext *cc;
  AVFormatContext *fmt_ctx;

  int64_t i_start_time = 0;

  boolean is_partial_clone = FALSE;

  int err;

  register int i;

  if (isclone && !priv->inited) {
    isclone = FALSE;
    if (cdata->fps > 0. && cdata->nframes > 0)
      is_partial_clone = TRUE;
    else priv->longer_seek = TRUE;
  }

  if (isclone) goto skip_probe;

  fmt_ctx = avformat_alloc_context();
  if (!fmt_ctx) {
    fprintf(stderr, "No fmt_ctx\n");
    return FALSE;
  }

  if ((err = avformat_open_input(&fmt_ctx, cdata->URI,
                                 NULL, NULL)) < 0) {
    fprintf(stderr, "avf_open failed\n");
    return FALSE;
  }

  fmt = fmt_ctx->iformat;

  if (!strcmp(fmt->name, "redir") ||
      !strcmp(fmt->name, "sdp")) {
    return FALSE;
  }

  /* Don't trigger false alarms on bin files */
  if (!strcmp(fmt->name, "psxstr")) {
    int i_len;
    i_len = strlen(cdata->URI);
    if (i_len < 4) return FALSE;

    if (strcasecmp(&cdata->URI[i_len - 4], ".str") &&
        strcasecmp(&cdata->URI[i_len - 4], ".xai") &&
        strcasecmp(&cdata->URI[i_len - 3], ".xa")) {
      return FALSE;
    }
  }

  fprintf(stderr, "avformat detected format: %s\n", fmt->name);

  priv->fmt = fmt;
  priv->inited = TRUE;

  avformat_close_input(&fmt_ctx);

skip_probe:

  priv->ic = NULL;

  priv->found_pts = -1;

  priv->last_frame = -1;
  priv->black_fill = FALSE;

  av_init_packet(&priv->packet);
  priv->needs_packet = FALSE;

  /* Open it */
  if (avformat_open_input(&priv->ic, cdata->URI, priv->fmt, NULL)) {
    fprintf(stderr, "avformat_open_input failed\n");
    return FALSE;
  }

  if (priv->ic->pb == NULL) {
    fprintf(stderr, "avformat stream not video\n");
    return FALSE;
  }

  if (!priv->ic->pb->seekable) {
    fprintf(stderr, "avformat stream non-seekable\n");
    return FALSE;
  }

  lives_avcodec_lock(); /* avformat calls avcodec behind our back!!! */
  if (avformat_find_stream_info(priv->ic, NULL) < 0) {
    fprintf(stderr, "avformat_find_stream_info failed\n");
  }
  lives_avcodec_unlock();

  if (isclone) {
    i = priv->vstream;
    goto skip_init;
  }

  // fill cdata

  cdata->nclips = 1;

  cdata->interlace = LIVES_INTERLACE_NONE; // TODO - this is set per frame

  cdata->par = 1.;
  cdata->offs_x = 0;
  cdata->offs_y = 0;
  cdata->frame_width = cdata->width = 0;
  cdata->frame_height = cdata->height = 0;

  if (!is_partial_clone) {
    cdata->nframes = 0;
    cdata->fps = 0.;
  }

  sprintf(cdata->container_name, "%s", priv->ic->iformat->name);

  memset(cdata->video_name, 0, 1);
  memset(cdata->audio_name, 0, 1);

  cdata->achans = cdata->asamps = cdata->arate = 0;

  cdata->asigned = FALSE;
  cdata->ainterleaf = TRUE;

  for (i = 0; i < priv->ic->nb_streams; i++) {

skip_init:
    s = priv->ic->streams[i];

    cc = s->codec;

#ifdef API_3_1
    ret = avcodec_parameters_to_context(cc, s->codecpar);
    if (ret < 0) {
      fprintf(stderr, "avcodec_decoder: avparms to context failed\n");
      return FALSE;
    }
#endif

    // vlc_fourcc_t fcc;
    //const char *psz_type = "unknown";

    /*      if( !GetVlcFourcc( cc->codec_id, NULL, &fcc, NULL ) )
      fcc = VLC_FOURCC( 'u', 'n', 'd', 'f' );*/

    switch (cc->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      if (priv->astream != -1) {
        fprintf(stderr, "Warning - got multiple audio streams\n");
        break;
      }

      cdata->achans = cc->channels;
      cdata->arate = cc->sample_rate;

#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
      cdata->asamps = cc->bits_per_sample;
#else
      cdata->asamps = cc->bits_per_coded_sample;
#endif

      get_samps_and_signed(cc->sample_fmt, &cdata->asamps, &cdata->asigned);

#ifdef HAVE_AVCODEC_GET_NAME
      sprintf(cdata->audio_name, "%s", avcodec_get_name(cc->codec_id));
#else
      sprintf(cdata->audio_name, "%s", cc->codec->name);
#endif

      priv->astream = i;
      break;

    case AVMEDIA_TYPE_VIDEO:

      if (!isclone && priv->vstream != -1) {
        fprintf(stderr, "Warning - got multiple video streams\n");
        break;
      }

      vdecoder = avcodec_find_decoder(cc->codec_id);
      avcodec_open2(cc, vdecoder, NULL);

      if (isclone) return TRUE;

      cdata->frame_width = cdata->width = cc->width;
      cdata->frame_height = cdata->height = cc->height;

      /*if( cc->palctrl )
      {
      fmt.video.p_palette = malloc( sizeof(video_palette_t) );
      *fmt.video.p_palette = *(video_palette_t *)cc->palctrl;
      }*/

      cdata->YUV_subspace = WEED_YUV_SUBSPACE_YCBCR;
      cdata->YUV_sampling = WEED_YUV_SAMPLING_DEFAULT;
      cdata->YUV_clamping = WEED_YUV_CLAMPING_CLAMPED;

      cdata->palettes[0] = avi_pix_fmt_to_weed_palette(cc->pix_fmt, &cdata->YUV_clamping);
      cdata->palettes[1] = WEED_PALETTE_END;

      if (cdata->palettes[0] == WEED_PALETTE_END) {
        fprintf(stderr, "avcodec_decoder: no usable palette found (%d)\n", cc->pix_fmt);
        return FALSE;
      }

#ifdef HAVE_AVCODEC_GET_NAME
      sprintf(cdata->video_name, "%s", avcodec_get_name(cc->codec_id));
#else
      sprintf(cdata->video_name, "%s", cc->codec->name);
#endif

      cdata->par = cc->sample_aspect_ratio.num / cc->sample_aspect_ratio.den;
      if (cdata->par == 0) cdata->par = 1;

      priv->fps_avg = FALSE;

      if (cdata->fps == 0.) {
        cdata->fps = s->time_base.den / s->time_base.num;
        if (cdata->fps >= 1000. || cdata->fps == 0.) {
          cdata->fps = (float)s->avg_frame_rate.num / (float)s->avg_frame_rate.den;
          priv->fps_avg = TRUE;
          if (isnan(cdata->fps)) {
            cdata->fps = s->time_base.den / s->time_base.num;
            priv->fps_avg = FALSE;
          }
        }
      }

      priv->ctx = cc;

#ifdef DEBUG
      fprintf(stderr, "fps is %.4f %d %d %d %d %d\n", cdata->fps, s->time_base.den, s->time_base.num, cc->time_base.den, cc->time_base.num,
              priv->ctx->ticks_per_frame);
#endif

      if (cdata->nframes == 0) {
        if (priv->ic->duration != (int64_t)AV_NOPTS_VALUE)
          cdata->nframes = ((double)priv->ic->duration / (double)AV_TIME_BASE * cdata->fps -  .5);
        if ((cdata->nframes == 0 || cdata->fps == 1000.) && s->nb_frames > 1) cdata->nframes = s->nb_frames;
      }

      priv->vstream = i;

      break;

    /*        case CODEC_TYPE_SUBTITLE:

    if( strncmp( p_sys->ic->iformat->name, "matroska", 8 ) == 0 &&
              cc->codec_id == AV_CODEC_ID_DVD_SUBTITLE &&
              cc->extradata != NULL &&
              cc->extradata_size > 0 )
    {
              char *psz_start;
              char *psz_buf = malloc( cc->extradata_size + 1);
              if( psz_buf != NULL )
              {
    memcpy( psz_buf, cc->extradata , cc->extradata_size );
    psz_buf[cc->extradata_size] = '\0';

    psz_start = strstr( psz_buf, "size:" );
    if( psz_start &&
    vobsub_size_parse( psz_start,
    &fmt.subs.spu.i_original_frame_width,
    &fmt.subs.spu.i_original_frame_height ) == VLC_SUCCESS )
    {
    msg_Dbg( p_demux, "original frame size: %dx%d",
    fmt.subs.spu.i_original_frame_width,
    fmt.subs.spu.i_original_frame_height );
    }
    else
    {
    msg_Warn( p_demux, "reading original frame size failed" );
    }

    psz_start = strstr( psz_buf, "palette:" );
    if( psz_start &&
    vobsub_palette_parse( psz_start, &fmt.subs.spu.palette[1] ) == VLC_SUCCESS )
    {
    fmt.subs.spu.palette[0] =  0xBeef;
    msg_Dbg( p_demux, "vobsub palette read" );
    }
    else
    {
    msg_Warn( p_demux, "reading original palette failed" );
    }
    free( psz_buf );
              }
    }

    psz_type = "subtitle";
    break;
    */

    //        default:

    /*
    #ifdef HAVE_FFMPEG_CODEC_ATTACHMENT
    if( cc->codec_type == CODEC_TYPE_ATTACHMENT )
    {
    input_attachment_t *p_attachment;
    psz_type = "attachment";
    if( cc->codec_id == AV_CODEC_ID_TTF )
    {
    p_attachment = vlc_input_attachment_New( s->filename, "application/x-truetype-font", NULL,
    cc->extradata, (int)cc->extradata_size );
    TAB_APPEND( p_sys->i_attachments, p_sys->attachments, p_attachment );
    }
    else msg_Warn( p_demux, "unsupported attachment type in ffmpeg demux" );
    }
    break;
    #endif

    if( cc->codec_type == CODEC_TYPE_DATA )
    psz_type = "data";

    msg_Warn( p_demux, "unsupported track type in ffmpeg demux" );
    break;
      }
      fmt.psz_language = strdup( s->language );
      if( s->disposition & AV_DISPOSITION_DEFAULT )
    fmt.i_priority = 1000;

    #ifdef HAVE_FFMPEG_CODEC_ATTACHMENT
      if( cc->codec_type != CODEC_TYPE_ATTACHMENT )
    #endif
      {
    const bool    b_ogg = !strcmp( p_sys->fmt->name, "ogg" );
    const uint8_t *p_extra = cc->extradata;
    unsigned      i_extra  = cc->extradata_size;

    if( cc->codec_id == AV_CODEC_ID_THEORA && b_ogg )
    {
    unsigned pi_size[3];
    void     *pp_data[3];
    unsigned i_count;
    for( i_count = 0; i_count < 3; i_count++ )
    {
    if( i_extra < 2 )
    break;
    pi_size[i_count] = GetWBE( p_extra );
    pp_data[i_count] = (uint8_t*)&p_extra[2];
    if( i_extra < pi_size[i_count] + 2 )
    break;

    p_extra += 2 + pi_size[i_count];
    i_extra -= 2 + pi_size[i_count];
    }
    if( i_count > 0 && xiph_PackHeaders( &fmt.i_extra, &fmt.p_extra,
    pi_size, pp_data, i_count ) )
    {
    fmt.i_extra = 0;
    fmt.p_extra = NULL;
    }
    }
    else if( cc->codec_id == AV_CODEC_ID_SPEEX && b_ogg )
    {
    uint8_t p_dummy_comment[] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    };
    unsigned pi_size[2];
    void     *pp_data[2];

    pi_size[0] = i_extra;
    pp_data[0] = (uint8_t*)p_extra;

    pi_size[1] = sizeof(p_dummy_comment);
    pp_data[1] = p_dummy_comment;

    if( pi_size[0] > 0 && xiph_PackHeaders( &fmt.i_extra, &fmt.p_extra,
    pi_size, pp_data, 2 ) )
    {
    fmt.i_extra = 0;
    fmt.p_extra = NULL;
    }
    }
    else if( cc->extradata_size > 0 )
    {
    fmt.p_extra = malloc( i_extra );
    if( fmt.p_extra )
    {
    fmt.i_extra = i_extra;
    memcpy( fmt.p_extra, p_extra, i_extra );
    }
    }
      }
      es = es_out_Add( p_demux->out, &fmt );
      if( s->disposition & AV_DISPOSITION_DEFAULT )
    es_out_Control( p_demux->out, ES_OUT_SET_ES_DEFAULT, es );
      es_format_Clean( &fmt );

      msg_Dbg( p_demux, "adding es: %s codec = %4.4s",
    psz_type, (char*)&fcc );
      TAB_APPEND( p_sys->i_tk, p_sys->tk, es );
    */
    default:
      break;

    }
  }

  if (priv->vstream == -1) {
    fprintf(stderr, "avcodec_decoder: no video stream found\n");
    return FALSE;
  }

  if (priv->ic->start_time != (int64_t)AV_NOPTS_VALUE)
    i_start_time = priv->ic->start_time * 1000000 / AV_TIME_BASE;

  fprintf(stderr, "AVFormat supported stream\n");
  fprintf(stderr, "    - format = %s (%s)\n",
          priv->fmt->name, priv->fmt->long_name);
  fprintf(stderr, "    - start time = %"PRId64"\n", i_start_time);
  fprintf(stderr, "    - duration = %"PRId64"\n",
          (priv->ic->duration != (int64_t)AV_NOPTS_VALUE) ?
          priv->ic->duration * 1000000 / AV_TIME_BASE : -1);

#ifdef HAVE_FFMPEG_CHAPTERS
  /*    if( p_sys->ic->nb_chapters > 0 )
        p_sys->p_title = vlc_input_title_New();
  for( i = 0; i < p_sys->ic->nb_chapters; i++ )
  {
        seekpoint_t *s = vlc_seekpoint_New();

        if( p_sys->ic->chapters[i]->title )
        {
  s->psz_name = strdup( p_sys->ic->chapters[i]->title );
  EnsureUTF8( s->psz_name );
  msg_Dbg( p_demux, "    - chapter %d: %s", i, s->psz_name );
        }
        s->i_time_offset = p_sys->ic->chapters[i]->start * 1000000 *
  p_sys->ic->chapters[i]->time_base.num /
  p_sys->ic->chapters[i]->time_base.den -
  (i_start_time != -1 ? i_start_time : 0 );
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
  }*/
#endif

  return TRUE;
}


static void detach_stream(lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_av_priv_t *priv = cdata->priv;

  // will close and free the context
  if (priv->ic != NULL) {
    avformat_close_input(&priv->ic);
  }

  if (cdata->palettes != NULL) free(cdata->palettes);
  cdata->palettes = NULL;

  priv->ctx = NULL;

  if (!priv->needs_packet)
    av_packet_unref(&priv->packet);

  if (priv->pFrame != NULL) {
    av_frame_unref(priv->pFrame);
    priv->pFrame = NULL;
  }

  priv->ic = NULL;

  priv->astream = -1;
  priv->vstream = -1;
}


//////////////////////////////////////////
// std functions

const char *module_check_init(void) {
  lives_avcodec_lock();
  av_register_all(); /* Can be called several times */
  lives_avcodec_unlock();
  return NULL;
}


const char *version(void) {
  return plugin_version;
}


static lives_clip_data_t *init_cdata(void) {
  lives_av_priv_t *priv;
  lives_clip_data_t *cdata = (lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->palettes = malloc(2 * sizeof(int));

  cdata->palettes[1] = WEED_PALETTE_END;

  cdata->URI = NULL;

  cdata->priv = priv = malloc(sizeof(lives_av_priv_t));

  priv->ic = NULL;

  priv->astream = -1;
  priv->vstream = -1;

  priv->inited = FALSE;
  priv->longer_seek = FALSE;

  cdata->seek_flag = 0;

  priv->ctx = NULL;
  priv->pFrame = NULL;

  cdata->sync_hint = SYNC_HINT_AUDIO_PAD_START;

  cdata->video_start_time = 0.;

  memset(cdata->author, 0, 1);
  memset(cdata->title, 0, 1);
  memset(cdata->comment, 0, 1);
#ifdef TEST_CACHING
  priv->cachemax = 128;
  priv->cache = NULL;
#endif
  return cdata;
}


static lives_clip_data_t *avf_clone(lives_clip_data_t *cdata) {
  lives_clip_data_t *clone = init_cdata();
  lives_av_priv_t *dpriv, *spriv;

  // copy from cdata to clone, with a new context for clone
  clone->URI = strdup(cdata->URI);  // VALID for pclone
  clone->nclips = cdata->nclips; // ???
  snprintf(clone->container_name, 512, "%s", cdata->container_name);
  clone->current_clip = cdata->current_clip;
  clone->width = cdata->width;
  clone->height = cdata->height;
  clone->nframes = cdata->nframes;   // valid for pclone
  clone->interlace = cdata->interlace;
  clone->offs_x = cdata->offs_x;
  clone->offs_y = cdata->offs_y;
  clone->frame_width = cdata->frame_width;
  clone->frame_height = cdata->frame_height;
  clone->par = cdata->par;
  clone->fps = cdata->fps;   // valid ? for pclone
  if (cdata->palettes != NULL) clone->palettes[0] = cdata->palettes[0];
  clone->current_palette = cdata->current_palette;
  clone->YUV_sampling = cdata->YUV_sampling;
  clone->YUV_clamping = cdata->YUV_clamping;
  snprintf(clone->video_name, 512, "%s", cdata->video_name);
  clone->arate = cdata->arate;
  clone->achans = cdata->achans;
  clone->asamps = cdata->asamps;
  clone->asigned = cdata->asigned;
  clone->ainterleaf = cdata->ainterleaf;
  snprintf(clone->audio_name, 512, "%s", cdata->audio_name);
  clone->seek_flag = cdata->seek_flag;
  clone->sync_hint = cdata->sync_hint;

  snprintf(clone->author, 256, "%s", cdata->author);
  snprintf(clone->title, 256, "%s", cdata->title);
  snprintf(clone->comment, 256, "%s", cdata->comment);

  // create "priv" elements
  dpriv = clone->priv;
  spriv = cdata->priv;

  if (spriv != NULL) {
    dpriv->vstream = spriv->vstream;
    dpriv->astream = spriv->astream;

    dpriv->fps_avg = spriv->fps_avg;

    dpriv->fmt = spriv->fmt;
    dpriv->inited = TRUE;
  }

  if (!attach_stream(clone, TRUE)) {
    free(clone->URI);
    clone->URI = NULL;
    clip_data_free(clone);
    return NULL;
  }

  if (spriv == NULL) {
    clone->current_palette = clone->palettes[0];
    clone->current_clip = 0;
    dpriv->last_frame = 1000000000;
  }

  dpriv->pFrame = NULL;

  return clone;
}


lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different URI or a different current_clip, this must be called again with the same
  // cdata as the second parameter
  int64_t real_frames;

  lives_av_priv_t *priv;

  if (URI == NULL && cdata != NULL) {
    // create a clone of cdata - we also need to be able to handle a "fake" clone with only URI, nframes and fps set (priv == NULL)
    cdata = avf_clone(cdata);
    if (cdata == NULL) return NULL;
    priv = cdata->priv;
    if (priv->longer_seek) goto rescan;
    return cdata;
  }

  if (cdata != NULL && cdata->current_clip > 0) {
    // currently we only support one clip per container

    clip_data_free(cdata);
    return NULL;
  }

  if (cdata == NULL) {
    cdata = init_cdata();
  }

  if (cdata->URI == NULL || strcmp(URI, cdata->URI)) {
    if (cdata->URI != NULL) {
      detach_stream(cdata);
      free(cdata->URI);
    }
    cdata->URI = strdup(URI);
    if (!attach_stream(cdata, FALSE)) {
      detach_stream(cdata);
      free(cdata->URI);
      cdata->URI = NULL;
      clip_data_free(cdata);
      return NULL;
    }
    cdata->current_palette = cdata->palettes[0];
    cdata->current_clip = 0;
    priv = cdata->priv;
    priv->last_frame = 1000000000;
    priv->pFrame = NULL;
  }

rescan:
  priv = cdata->priv;

  real_frames = get_real_last_frame(cdata, priv->longer_seek) + 1;

  if (real_frames <= 0) {
    fprintf(stderr,
            "avformat_decoder: ERROR - could not find the last frame\navformat_decoder: I will pass on this file as it may be broken.\n");
    detach_stream(cdata);
    free(cdata->URI);
    cdata->URI = NULL;
    clip_data_free(cdata);
    return NULL;
  }

  if (cdata->nframes > 100 && real_frames < cdata->nframes * SEEK_SUCCESS_MIN_RATIO) {
    fprintf(stderr,
            "avformat_decoder: ERROR - could only seek to %ld frames out of %ld\navformat_decoder: I will pass on this file as it may be broken.\n",
            real_frames, cdata->nframes);
    detach_stream(cdata);
    free(cdata->URI);
    cdata->URI = NULL;
    clip_data_free(cdata);
    return NULL;
  }

  priv = cdata->priv;

  if (priv->fps_avg && cdata->nframes > 1) {
    //cdata->fps=cdata->fps*(cdata->nframes-1.)/(float)real_frames;
  }

  cdata->nframes = real_frames;

  if (priv->pFrame != NULL) av_frame_unref(priv->pFrame);
  priv->pFrame = NULL;

  return cdata;
}


static size_t write_black_pixel(unsigned char *idst, int pal, int npixels, int y_black) {
  unsigned char *dst = idst;
  register int i;

  for (i = 0; i < npixels; i++) {
    switch (pal) {
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
      dst[0] = dst[1] = dst[2] = 0;
      dst[3] = 255;
      dst += 4;
      break;
    case WEED_PALETTE_ARGB32:
      dst[1] = dst[2] = dst[3] = 0;
      dst[0] = 255;
      dst += 4;
      break;
    case WEED_PALETTE_UYVY8888:
      dst[1] = dst[3] = y_black;
      dst[0] = dst[2] = 128;
      dst += 4;
      break;
    case WEED_PALETTE_YUYV8888:
      dst[0] = dst[2] = y_black;
      dst[1] = dst[3] = 128;
      dst += 4;
      break;
    case WEED_PALETTE_YUV888:
      dst[0] = y_black;
      dst[1] = dst[2] = 128;
      dst += 3;
      break;
    case WEED_PALETTE_YUVA8888:
      dst[0] = y_black;
      dst[1] = dst[2] = 128;
      dst[3] = 255;
      dst += 4;
      break;
    case WEED_PALETTE_YUV411:
      dst[0] = dst[3] = 128;
      dst[1] = dst[2] = dst[4] = dst[5] = y_black;
      dst += 6;
    default:
      break;
    }
  }
  return idst - dst;
}

#ifdef TEST_CACHING
#define DEF_CACHEFRAMES_MAX 16

#ifndef ABS(a)
#define ABS(a) (a >= 0ll ? a : -a)
#endif

static AVFrame *get_from_cache(lives_av_priv_t *priv, int64_t pts) {
  priv_cache_t *cache = priv->cache;
  while (cache != NULL) {
    //fprintf(stderr, "CF %ld %ld %ld\n", cache->pts, pts, ABS(cache->pts - pts));
    if (ABS(cache->pts - pts) <= 100) {
      return cache->frame;
    }
    cache = cache->next;
  }
  return NULL;
}


static void remove_cache_above(priv_cache_t *cache, int maxe) {
  priv_cache_t *lastcache = cache,  *nextcache;
  int count = 0;

  while (cache != NULL) {
    nextcache = cache->next;
    if (count++ >= maxe) {
      if (lastcache != NULL) lastcache->next = NULL;
      av_frame_unref(cache->frame);
      free(cache);
      lastcache = NULL;
    } else lastcache = cache;
    cache = nextcache;
  }
}


static void add_to_cache(lives_av_priv_t *priv, int64_t pts) {
  // if we have a frame with this pts in the list, ignore
  // otherwise free the tail of the list and add this at the head

  // NOTE: void av_frame_move_ref(AVFrame *dst, AVFrame *src); int av_frame_copy(AVFrame *dst, const AVFrame *src);
  // int av_frame_ref(AVFrame *dst, const AVFrame *src); AVFrame *av_frame_clone(const AVFrame *src);
  if (priv->cachemax < 1) return;
  if (get_from_cache(priv, pts) != NULL) {
    fprintf(stderr, "Dupe\n");
    return;
  } else {
    priv_cache_t *cache = (priv_cache_t *)malloc(sizeof(priv_cache_t));
    if (cache == NULL) return;
    cache->frame = priv->pFrame;
    cache->pts = pts;
    cache->next = priv->cache;
    priv->cache = cache;
    remove_cache_above(cache, priv->cachemax);
  }
}


static void free_cache(lives_av_priv_t *priv) {
  // walk the list and free each element
  remove_cache_above(priv->cache, 0);
  priv->cache = NULL;
}


int begin_caching(const lives_clip_data_t *cdata, int maxframes) {
  lives_av_priv_t *priv = cdata->priv;
  if (maxframes == -1) maxframes = DEF_CACHEFRAMES_MAX;
  priv->cachemax = maxframes;
  if (maxframes == 0) free_cache(priv);
  return maxframes;
}
#endif

// tune this so small jumps forward are efficient
#define JUMP_FRAMES_SLOW 64
#define JUMP_FRAMES_FAST 1

boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, int *rowstrides, int height, void **pixel_data) {
  // seek to frame, and return pixel_data

  // tframe starts at 0

  lives_av_priv_t *priv = cdata->priv;
  double time;

  AVStream *s = priv->ic->streams[priv->vstream];
  AVCodecContext *cc;

  int64_t target_pts, MyPts, seek_target = 10000000000;
  int64_t timex;

  unsigned char *dst, *src;
  unsigned char black[4] = {0, 0, 0, 255};

  boolean hit_target = FALSE;
  //boolean did_seek = FALSE;

  int gotFrame;

  int xheight = cdata->frame_height, pal = cdata->current_palette, nplanes = 1, dstwidth = cdata->width, psize = 1;
  int btop = cdata->offs_y, bbot = xheight - 1 - btop;
  int bleft = cdata->offs_x, bright = cdata->frame_width - cdata->width - bleft;
  int y_black = (cdata->YUV_clamping == WEED_YUV_CLAMPING_CLAMPED) ? 16 : 0;
  int ret;

  int jump_frames;

  register int p, i;

  // if pixel_data is NULL, just check if the frame exists
  if (tframe < 0 || ((tframe >= cdata->nframes || cdata->fps == 0.) && pixel_data != NULL)) return FALSE;

  cc = s->codec;

#ifdef API_3_1
  ret = avcodec_parameters_to_context(cc, s->codecpar);
  if (ret < 0) {
    fprintf(stderr, "avcodec_decoder: avparms to context failed\n");
    return FALSE;
  }
#endif

  //cc->get_buffer = our_get_buffer;
  //cc->release_buffer = our_release_buffer;

  timex = get_current_ticks();

  if (pixel_data != NULL) {

    // calc frame width and height, including any border

    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P) {
      nplanes = 3;
      black[0] = y_black;
      black[1] = black[2] = 128;
    } else if (pal == WEED_PALETTE_YUVA4444P) {
      nplanes = 4;
      black[0] = y_black;
      black[1] = black[2] = 128;
      black[3] = 255;
    }

    if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) psize = 3;

    if (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32 || pal == WEED_PALETTE_UYVY8888 ||
        pal == WEED_PALETTE_YUYV8888 || pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888) psize = 4;

    if (pal == WEED_PALETTE_YUV411) psize = 6;

    if (pal == WEED_PALETTE_A1) dstwidth >>= 3;

    dstwidth *= psize;

    if (cdata->frame_height > cdata->height && height == cdata->height) {
      // host ignores vertical border
      btop = 0;
      xheight = cdata->height;
      bbot = xheight - 1;
    }

    if (cdata->frame_width > cdata->width && rowstrides[0] < cdata->frame_width * psize) {
      // host ignores horizontal border
      bleft = bright = 0;
    }
  }
  //#define DEBUG

  time = (double)tframe / cdata->fps;
  target_pts = time * (double)AV_TIME_BASE;
#ifdef TEST_CACHING
  priv->pFrame = get_from_cache(priv, target_pts);
#ifdef DEBUG
  if (priv->pFrame != NULL) fprintf(stderr, "got frame from cache for target %ld %p %d\n", target_pts, priv->pFrame, pal);
  else fprintf(stderr, "got frame from cache FAIL for target %ld %p %d\n", target_pts, priv->pFrame, pal);
#endif
  if (priv->pFrame != NULL) goto framedone2;
#endif

  if (priv->pFrame == NULL || tframe != priv->last_frame) {
    // same frame -> we reuse priv-pFrame if we have it; otherwise we do this

#ifdef DEBUG
    fprintf(stderr, "pt a1 %d %ld\n", priv->last_frame, tframe);
#endif

#ifndef TEST_CACHING
    if (priv->pFrame != NULL) av_frame_unref(priv->pFrame);
#endif
    priv->pFrame = NULL;

    if (cdata->seek_flag & LIVES_SEEK_FAST) jump_frames = JUMP_FRAMES_FAST;
    else jump_frames = JUMP_FRAMES_SLOW;

    if (tframe < priv->last_frame || tframe - priv->last_frame > jump_frames) {
      int64_t xtarget_pts;
      // seek to new frame

      // try to seek straight to keyframe
      if (!(cdata->seek_flag & LIVES_SEEK_FAST) && tframe < priv->last_frame && priv->found_pts != -1 && target_pts > priv->found_pts)
        xtarget_pts = priv->found_pts;
      else xtarget_pts = target_pts;

      xtarget_pts += priv->ic->start_time;

      seek_target = av_rescale_q(xtarget_pts, AV_TIME_BASE_Q, s->time_base);
      av_seek_frame(priv->ic, priv->vstream, seek_target, AVSEEK_FLAG_BACKWARD);
#ifdef DEBUG
      fprintf(stderr, "new seek: %d %ld %ld\n", priv->last_frame, seek_target, priv->ic->start_time);
#endif
      avcodec_flush_buffers(cc);
      priv->black_fill = FALSE;
      MyPts = -1;
      priv->needs_packet = TRUE;
      //did_seek = TRUE;
    } else {
      MyPts = (priv->last_frame + 1.) / cdata->fps * (double)AV_TIME_BASE;
    }

    //

    do {
      if (priv->needs_packet) {
        do {
          av_packet_unref(&priv->packet);
          av_init_packet(&priv->packet);
          ret = av_read_frame(priv->ic, &priv->packet);

#ifdef DEBUG
          fprintf(stderr, "ret was %d for tframe %ld\n", ret, tframe);
#endif
          if (ret < 0) {
            priv->needs_packet = TRUE;
            priv->last_frame = tframe;
            if (pixel_data == NULL) return FALSE;
            priv->black_fill = TRUE;
            goto framedone;
          }
        } while (priv->packet.stream_index != priv->vstream);
      }

      if (MyPts == -1) {
        MyPts = priv->packet.pts;
        MyPts = av_rescale_q(MyPts, s->time_base, AV_TIME_BASE_Q) - priv->ic->start_time;
        priv->found_pts = MyPts; // PTS of start of frame, set so start_time is zero
      }

#ifdef DEBUG
      fprintf(stderr, "pt b1 %ld %ld %ld\n", MyPts, target_pts, seek_target);
#endif
      // decode any frames from this packet
      if (priv->pFrame == NULL) priv->pFrame = av_frame_alloc();

#if LIBAVCODEC_VERSION_MAJOR >= 52
      ret = avcodec_decode_video2(cc, priv->pFrame, &gotFrame, &priv->packet);
      if (ret < 0) return FALSE;
      ret = FFMIN(ret, priv->packet.size);
      priv->packet.data += ret;
      priv->packet.size -= ret;
#else
      ret = avcodec_decode_video(cc, priv->pFrame, &gotFrame, priv->packet.data, priv->packet.size);
      priv->packet.size = 0;
#endif

#ifdef DEBUG
      fprintf(stderr, "pt 1 %ld %d %ld %ld %d %d\n", tframe, gotFrame, MyPts, gotFrame ? priv->pFrame->best_effort_timestamp : 0,
              priv->pFrame->color_trc, priv->pFrame->color_range);
#endif

#ifdef TEST_CACHING
      if (gotFrame && priv->cachemax > 0) {
        add_to_cache(priv, MyPts);
        fprintf(stderr, "adding to cache: %ld %p %p\n", MyPts, priv->pFrame, priv->pFrame->data);
      }
#endif

      if (priv->packet.size == 0) {
        priv->needs_packet = TRUE;
      }

      if (MyPts >= target_pts - 100) hit_target = TRUE;

      if (hit_target && gotFrame) break;

      // otherwise discard this frame
      if (gotFrame) {
        MyPts += (double)AV_TIME_BASE / cdata->fps;
#ifndef TEST_CACHING
        av_frame_unref(priv->pFrame);
#endif
        priv->pFrame = NULL;
      }
    } while (!(hit_target && gotFrame));
  }

framedone:
  timex = get_current_ticks() - timex;

  // TODO **: if we did a seek and had to decode multiple frames, set needs_calc; if seek direct don't
  // then set fast depending on the time limit
  if (timex > FAST_SEEK_LIMIT)((lives_clip_data_t *)cdata)->seek_flag = LIVES_SEEK_NEEDS_CALCULATION;
  //else if (did_seek) ((lives_clip_data_t *)cdata)->seek_flag = LIVES_SEEK_FAST;

  priv->last_frame = tframe;

#ifdef TEST_CACHING
framedone2:
#endif
  if (priv->pFrame == NULL || pixel_data == NULL) return TRUE;

  if (priv->black_fill) btop = cdata->frame_height;
  else {
    // we are allowed to cast away const-ness for
    // yuv_subspace, yuv_clamping, yuv_sampling, frame_gamma and interlace

    if (priv->pFrame->interlaced_frame) {
      if (priv->pFrame->top_field_first)((lives_clip_data_t *)cdata)->interlace = LIVES_INTERLACE_TOP_FIRST;
      else ((lives_clip_data_t *)cdata)->interlace = LIVES_INTERLACE_BOTTOM_FIRST;
    } else ((lives_clip_data_t *)cdata)->interlace = LIVES_INTERLACE_NONE;

    ((lives_clip_data_t *)cdata)->YUV_sampling = WEED_YUV_SAMPLING_DEFAULT;

    if (priv->pFrame->chroma_location == AVCHROMA_LOC_LEFT)
      ((lives_clip_data_t *)cdata)->YUV_sampling = WEED_YUV_SAMPLING_JPEG;

    if (priv->pFrame->chroma_location == AVCHROMA_LOC_CENTER)
      ((lives_clip_data_t *)cdata)->YUV_sampling = WEED_YUV_SAMPLING_MPEG;

    if (priv->pFrame->chroma_location == AVCHROMA_LOC_TOPLEFT)
      ((lives_clip_data_t *)cdata)->YUV_sampling = WEED_YUV_SAMPLING_DVNTSC;

    if (priv->pFrame->colorspace == AVCOL_SPC_BT709)
      ((lives_clip_data_t *)cdata)->YUV_subspace = WEED_YUV_SUBSPACE_BT709;
    else
      ((lives_clip_data_t *)cdata)->YUV_subspace = WEED_YUV_SUBSPACE_YCBCR;


    if (priv->pFrame->color_range == AVCOL_RANGE_JPEG)
      ((lives_clip_data_t *)cdata)->YUV_clamping = WEED_YUV_CLAMPING_UNCLAMPED;
    else
      ((lives_clip_data_t *)cdata)->YUV_clamping = WEED_YUV_CLAMPING_CLAMPED;
    y_black = (cdata->YUV_clamping == WEED_YUV_CLAMPING_CLAMPED) ? 16 : 0;

    ((lives_clip_data_t *)cdata)->frame_gamma = WEED_GAMMA_SRGB;
    if (priv->pFrame->color_trc == AVCOL_TRC_LINEAR)
      ((lives_clip_data_t *)cdata)->frame_gamma = WEED_GAMMA_LINEAR;
    /* if (priv->pFrame->color_trc == AVCOL_TRC_BT709) */
    /*   ((lives_clip_data_t *)cdata)->frame_gamma = WEED_GAMMA_BT709; */
  }
  for (p = 0; p < nplanes; p++) {
    dst = pixel_data[p];
    src = priv->pFrame->data[p];

    for (i = 0; i < xheight; i++) {
      if (i < btop || i > bbot) {
        // top or bottom border, copy black row
        if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P ||
            pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) {
          memset(dst, black[p], dstwidth + (bleft + bright)*psize);
          dst += dstwidth + (bleft + bright) * psize;
        } else dst += write_black_pixel(dst, pal, dstwidth / psize + bleft + bright, y_black);
        continue;
      }

      if (bleft > 0) {
        if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P ||
            pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) {
          memset(dst, black[p], bleft * psize);
          dst += bleft * psize;
        } else dst += write_black_pixel(dst, pal, bleft, y_black);
      }

      memcpy(dst, src, dstwidth);
      dst += dstwidth;

      if (bright > 0) {
        if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P || pal == WEED_PALETTE_YUV444P ||
            pal == WEED_PALETTE_YUVA4444P || pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) {
          memset(dst, black[p], bright * psize);
          dst += bright * psize;
        } else dst += write_black_pixel(dst, pal, bright, y_black);
      }

      src += priv->pFrame->linesize[p];
    }
    if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P)) {
      dstwidth >>= 1;
      bleft >>= 1;
      bright >>= 1;
    }
    if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P)) {
      xheight >>= 1;
      btop >>= 1;
      bbot >>= 1;
    }
  }

  return TRUE;
}


void clip_data_free(lives_clip_data_t *cdata) {

  if (cdata->palettes != NULL) free(cdata->palettes);
  cdata->palettes = NULL;

  if (cdata->URI != NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  free(cdata->priv);

  free(cdata);
}


void module_unload(void) {
  // do nothing
}
