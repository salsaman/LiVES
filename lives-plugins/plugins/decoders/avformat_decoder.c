/// LiVES - avformat plugin
// (c) G. Finch 2010 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details
//
// some code adapted from vlc (GPL v2 or higher)


#define PLUGIN_UID 0x6465636C69626176ull

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#ifndef HAVE_SYSTEM_WEED
#include "../../../libweed/weed-palettes.h"
#else
#include <weed/weed-palettes.h>
#endif

#define HAVE_AVCODEC
#define HAVE_AVUTIL

#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavcodec/version.h>
#include <libavutil/mem.h>

#ifdef NEED_LOCAL_WEED_COMPAT
#include "../../../libweed/weed-compat.h"
#else
#include <weed/weed-compat.h>
#endif

/// this is unreliable for libav, it appears to leak memory after each seek
/// but only when using send_packet / receive_frame
/// using the deprecated method does not cause the leak
#undef HAVE_AVCODEC_SEND_PACKET

#define NEED_CLONEFUNC
#define NEED_TIMING
#define NEED_INDEX
#include "decplugin.h"

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

#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/lzo.h>
#include <libavutil/dict.h>

#include "libav_helper.h"

#include "avformat_decoder.h"

static int vmaj = 1;
static int vmin = 1;
static const char *plugin_name = "LiVES avformat";

static pthread_mutex_t avcodec_mutex = PTHREAD_MUTEX_INITIALIZER;

#define FAST_SEEK_LIMIT 50 // milliseconds (default 0.05 sec)
#define FAST_SEEK_REV_LIMIT 200 // milliseconds (default 0.2 sec)
#define NO_SEEK_LIMIT 2000 // milliseconds (default 2 seconds)
#define LNO_SEEK_LIMIT 10000 // milliseconds (default 10 seconds)

#define SEEK_SUCCESS_MIN_RATIO 0.5 // if real frames are l.t. suggested frames * this, then we assume the file is corrupted

// tune this so small jumps forward are efficient (this is done now !)
#define JUMP_FRAMES_SLOW 64   /// if seek is slow, how many frames to read forward rather than seek
#define JUMP_FRAMES_FAST 16 /// if seek is fast, how many frames to read forward rather than seek

static inline void lives_avcodec_lock(void) {pthread_mutex_lock(&avcodec_mutex);}
static inline void lives_avcodec_unlock(void) {pthread_mutex_unlock(&avcodec_mutex);}

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


#define FRAMES_GUESS 32768
#define HALF_ROUND_UP(x) (x > 0 ? (int)(x / 2. + .5) : (int)(x / 2. - .5))
//#define DEBUG_RLF
static int64_t get_real_last_frame(lives_clip_data_t *cdata, boolean allow_longer_seek) {
  int64_t diff = 0;

  // lframe is the frame we will check, olframe is the current seek base frame, maxframes is the largest found
  int64_t olframe = cdata->nframes - 1, lframe = olframe;
  int64_t timex;
  long no_seek_limit = NO_SEEK_LIMIT * 1000;

  boolean have_upper_bound = FALSE;
  boolean have_lower_bound = FALSE;

  if (allow_longer_seek) no_seek_limit = LNO_SEEK_LIMIT * 1000;
  cdata->seek_flag = LIVES_SEEK_FAST | LIVES_SEEK_FAST_REV;

  // check we can get at least one frame
  if (!get_frame(cdata, 0, NULL, 0, NULL)) {
#ifdef DEBUG_RLF
    fprintf(stderr, "Could not get even 1 frame\n");
#endif
    return -1;
  }

  if (cdata->nframes == 0) {
    // if we got a broken file with 0 last frame, search for it
    olframe = lframe = FRAMES_GUESS - 1;
    diff = -FRAMES_GUESS;
    have_lower_bound = TRUE;
  }

  while (1) {
    // #define DEBUG_RLF
#ifdef DEBUG_RLF
    fprintf(stderr, "will check frame %ld of (allegedly) %ld...", lframe + 1, cdata->nframes);
#endif

    timex = -get_current_ticks();

    // see if we can find lframe
    if (!get_frame(cdata, lframe, NULL, 0, NULL)) {
      // lframe not found

      have_upper_bound = TRUE; // we got an upper bound

      timex += get_current_ticks();

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
        lframe--;
        break;
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

      timex += get_current_ticks();

      if (timex > no_seek_limit) {
#ifdef DEBUG_RLF
        fprintf(stderr, "avcodec_decoder: seek was %ld, longer than limit of %ld; giving up.\n", timex, no_seek_limit);
#endif
        return -1;
      }

      if (timex > FAST_SEEK_LIMIT * 1000) {
        cdata->seek_flag = LIVES_SEEK_NEEDS_CALCULATION;
        cdata->jump_limit = JUMP_FRAMES_SLOW;
      }

#ifdef DEBUG_RLF
      fprintf(stderr, "yes ! (%ld)\n", timex);
#endif
      if (diff == 0) diff = 1; // initial frame found, start a forward scan
      else {
        if (diff < 2 && diff > -2) {
          // this is the ideal situation, we got lframe, but not lframe + 1
          break;
        }
        if (have_upper_bound) diff = HALF_ROUND_UP(diff); // we found a null frame
        else diff *= 2;
        if (diff < 0) diff = -diff; // we were searching backwards, now we want to go forwards
      }
    }
    olframe = lframe;
    lframe += diff;
  }

  // found it..

#ifdef DEBUG_RLF
  fprintf(stderr, "av fwd seek was %ld\n", cdate->fwd_seek_time);
  fprintf(stderr, "max decode fps would be %f\n", cdata->max_decode_fps);
  fprintf(stderr, "jump_limit was reset to %ld\n", cdata->jump_limit);
#endif

#ifdef DEBUG_RLF
  fprintf(stderr, "av rev seek was %f\n", cdata->adv_timing.seekback_time);
#endif

#ifdef DEBUG_RLF
  fprintf(stderr, "jump_limit was reset again to %ld\n", cdata->jump_limit);
#endif

  return lframe;
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

  int i;

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

  if ((err = avformat_open_input(&fmt_ctx, cdata->URI, NULL, NULL)) < 0) {
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
  priv->idxc = idxc_for(cdata);

  priv->found_pts = -1;

  priv->last_frame = -1;

  av_init_packet(&priv->packet);
  priv->needs_packet = FALSE;

  /* Open it */
  if (avformat_open_input(&priv->ic, cdata->URI, priv->fmt, NULL)) {
    fprintf(stderr, "avformat_open_input failed\n");
    return FALSE;
  }

  if (!priv->ic->pb) {
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

      priv->ctx = cc;

      if (isclone) return TRUE;

      cdata->frame_width = cdata->width = cc->width;
      cdata->frame_height = cdata->height = cc->height;

      /*if( cc->palctrl )
        {
        fmt.video.p_palette = malloc( sizeof(video_palette_t) );
        fmt.video.p_palette = *(video_palette_t *)cc->palctrl;
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
      fprintf(stderr, "fps is %.4f %d %d %d %d %d\n", cdata->fps, s->time_base.den, s->time_base.num, cc->time_base.den,
              cc->time_base.num,
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

  if (cdata->palettes) free(cdata->palettes);
  cdata->palettes = NULL;

  if (!priv) return;

  priv->ctx = NULL;

  // will close and free the context
  if (priv->ic) {
    avformat_close_input(&priv->ic);
  }

  if (!priv->needs_packet)
    av_packet_unref(&priv->packet);

  if (priv->pFrame) {
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
  //avcodec_register_all();
  //avformat_network_init();
  lives_avcodec_unlock();
  return NULL;
}


const lives_plugin_id_t *get_plugin_id(void) {
  return _make_plugin_id(plugin_name, vmaj, vmin);
}


static lives_clip_data_t *init_cdata(lives_clip_data_t *data) {
  lives_av_priv_t *priv;
  lives_clip_data_t *cdata;

  if (!data) {
    cdata = cdata_new(NULL);
    cdata->palettes = malloc(2 * sizeof(int));
    cdata->palettes[1] = WEED_PALETTE_END;
    cdata->priv = calloc(1, sizeof(lives_av_priv_t));
  } else cdata = data;

  priv = (lives_av_priv_t *)cdata->priv;

  if (!priv) cdata->priv = priv = calloc(1, sizeof(lives_av_priv_t));

  priv->astream = -1;
  priv->vstream = -1;

  cdata->seek_flag = LIVES_SEEK_FAST | LIVES_SEEK_FAST_REV;

  cdata->sync_hint = SYNC_HINT_AUDIO_PAD_START | SYNC_HINT_AUDIO_TRIM_END;

  cdata->jump_limit = JUMP_FRAMES_FAST;

#ifdef TEST_CACHING
  // TODO: needs to have refcounting enabled
  priv->cachemax = 128;
  priv->cache = NULL;
#endif
  return cdata;
}


static lives_clip_data_t *avf_clone(lives_clip_data_t *cdata) {
  lives_clip_data_t *clone = clone = clone_cdata(cdata);
  lives_av_priv_t *dpriv, *spriv;

  // create "priv" elements
  spriv = cdata->priv;

  if (spriv) {
    clone->priv = dpriv = calloc(1, sizeof(lives_av_priv_t));
    dpriv->vstream = spriv->vstream;
    dpriv->astream = spriv->astream;
    dpriv->fps_avg = spriv->fps_avg;
    dpriv->fmt = spriv->fmt;
    dpriv->inited = TRUE;
  } else {
    clone = init_cdata(clone);
    dpriv = clone->priv;
  }

  if (!clone->palettes) {
    clone->palettes = malloc(2 * sizeof(int));
    clone->palettes[1] = WEED_PALETTE_END;
  }

  if (!attach_stream(clone, TRUE)) {
    clip_data_free(clone);
    return NULL;
  }

  if (!spriv) {
    if (clone->palettes) clone->current_palette = clone->palettes[0];
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

  if (!URI && cdata) {
    // create a clone of cdata - we also need to be able to handle a "fake" clone
    // with only URI, nframes and fps set (priv == NULL_
    cdata = avf_clone(cdata);
    if (!cdata) return NULL;
    priv = cdata->priv;
    if (priv->longer_seek) goto rescan;
    return cdata;
  }

  if (cdata && cdata->current_clip > 0) {
    // currently we only support one clip per container
    clip_data_free(cdata);
    return NULL;
  }

  if (!cdata) {
    cdata = init_cdata(NULL);
  }

  if (!cdata->URI || strcmp(URI, cdata->URI)) {
    if (cdata->URI) {
      detach_stream(cdata);
      free(cdata->URI);
    }
    cdata->URI = strdup(URI);
    if (!attach_stream(cdata, FALSE)) {
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

  if (cdata->seek_flag == 0 || (cdata->seek_flag & LIVES_SEEK_FAST) != 0) {
    real_frames = get_real_last_frame(cdata, priv->longer_seek) + 1;

    if (real_frames <= 0) {
      fprintf(stderr,
              "avformat_decoder: ERROR - could not find the last frame\navformat_decoder: I will pass on this file as it may be broken.\n");
      clip_data_free(cdata);
      return NULL;
    }

    if (cdata->nframes > 100 && real_frames < cdata->nframes * SEEK_SUCCESS_MIN_RATIO) {
      fprintf(stderr,
              "avformat_decoder: ERROR - could only seek to %ld frames out of %ld\navformat_decoder: "
              "I will pass on this file as it may be broken.\n",
              real_frames, cdata->nframes);
      clip_data_free(cdata);
      return NULL;
    }
  } else real_frames = cdata->nframes;

  priv = cdata->priv;

  if (priv->fps_avg && cdata->nframes > 1) {
    //cdata->fps=cdata->fps*(cdata->nframes-1.)/(float)real_frames;
  }

  cdata->nframes = real_frames;

  if (priv->pFrame) av_frame_unref(priv->pFrame);
  priv->pFrame = NULL;

  return cdata;
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


boolean chill_out(const lives_clip_data_t *cdata) {
  // free buffers because we are going to chill out for a while
  // (seriously, host can call this to free any buffers when we aren't palying sequentially)
  if (cdata) {
    lives_av_priv_t *priv = cdata->priv;
    if (priv) {
      AVStream *s = priv->ic->streams[priv->vstream];
      if (priv->pFrame) av_frame_unref(priv->pFrame);
      priv->pFrame = NULL;
      if (s) {
        AVCodecContext *cc = s->codec;
        if (cc) {
          //avcodec_flush_buffers(cc);
        }
      }
    }
  }
  return TRUE;
}


static double est_noseek;

//#define DEBUG
boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, int *rowstrides, int height, void **pixel_data) {
  // seek to frame, and return pixel_data

  // tframe starts at 0

  lives_av_priv_t *priv = cdata->priv;
  double time = 0.;

  AVStream *s = priv->ic->streams[priv->vstream];
  AVCodecContext *cc;

  int64_t target_pts, MyPts = -1, seek_target = 10000000000;
  int64_t timex, xtimex = -1, sbtime;

  unsigned char *dst, *src;

  boolean hit_target = FALSE;
  boolean do_seek = FALSE;
  boolean did_seek = FALSE;
  boolean rev = FALSE;
  boolean is_keyframe;
#ifdef HAVE_AVCODEC_SEND_PACKET
  boolean snderr;
#endif
  int gotFrame;

  int xheight = cdata->frame_height, pal = cdata->current_palette, nplanes = 1, dstwidth = cdata->width, psize = 1;
  int btop = cdata->offs_y, bbot = xheight - btop;
  int bleft = cdata->offs_x, bright = cdata->frame_width - cdata->width - bleft;
  int ret;
  int64_t jump_limit = cdata->jump_limit;
  int64_t xframe;
  int rowstride, xrowstride;
  int p, i;

  timex = -get_current_ticks();

  // if pixel_data is NULL, just check if the frame exists
  if (tframe < 0 || ((tframe >= cdata->nframes || cdata->fps == 0.) && pixel_data != NULL)) {
    fprintf(stderr, "avformat decoder: frame %ld not in range 0 to %ld, or fps is zero\n", tframe, cdata->nframes);
    goto cleanup;
  }
  if (jump_limit == 0) {
    if (cdata->seek_flag & LIVES_SEEK_FAST) jump_limit = JUMP_FRAMES_FAST;
    else jump_limit = JUMP_FRAMES_SLOW;
  }

  cc = s->codec;

#ifdef API_3_1
  ret = avcodec_parameters_to_context(cc, s->codecpar);
  if (ret < 0) {
    fprintf(stderr, "avcodec_decoder: avparms to context failed\n");
    goto cleanup;
  }
#endif

  //cc->get_buffer = our_get_buffer;
  //cc->release_buffer = our_release_buffer;

  if (pixel_data) {
    // calc frame width and height, including any border
    if (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P
        || pal == WEED_PALETTE_YUV444P) nplanes = 3;
    else if (pal == WEED_PALETTE_YUVA4444P) nplanes = 4;
    if (pal == WEED_PALETTE_RGB24 || pal == WEED_PALETTE_BGR24) psize = 3;
    if (pal == WEED_PALETTE_RGBA32 || pal == WEED_PALETTE_BGRA32 || pal == WEED_PALETTE_ARGB32
        || pal == WEED_PALETTE_UYVY8888 ||
        pal == WEED_PALETTE_YUYV8888 || pal == WEED_PALETTE_YUV888 || pal == WEED_PALETTE_YUVA8888) psize = 4;
    if (pal == WEED_PALETTE_YUV411) psize = 6;
    if (pal == WEED_PALETTE_A1) dstwidth >>= 3;

    dstwidth *= psize;

    if (cdata->frame_height > cdata->height && height == cdata->height) {
      // host ignores vertical border
      btop = 0;
      xheight = cdata->height;
      bbot = xheight;
    }

    if (cdata->frame_width > cdata->width && rowstrides[0] < cdata->frame_width * psize) {
      // host ignores horizontal border
      bleft = bright = 0;
    }
  }

  //#define DEBUG
  if (cdata->fps) time = ((double)tframe) / cdata->fps;
  target_pts = time * (double)AV_TIME_BASE;

#ifdef TEST_CACHING
  priv->pFrame = get_from_cache(priv, target_pts);
#ifdef DEBUG
  if (priv->pFrame) fprintf(stderr, "got frame from cache for target %ld %p %d\n", target_pts, priv->pFrame, pal);
  else fprintf(stderr, "got frame from cache FAIL for target %ld %p %d\n", target_pts, priv->pFrame, pal);
#endif
  if (priv->pFrame) goto framedone2;
#endif

  // same frame -> reuse priv-pFrame if we have it
  if (tframe == priv->last_frame && priv->pFrame && priv->pFrame->data[0]) {
    xtimex = get_current_ticks();
    goto framedone;
  }

  if (tframe <= priv->last_frame) rev = TRUE;
  if (priv->last_frame > -1 && tframe > priv->last_frame) {
    double est = estimate_delay(cdata, tframe);
    if (est <= 0. || est_noseek <= 0.) {
      if (tframe - priv->last_frame > cdata->jump_limit) do_seek = TRUE;
    } else if (est < est_noseek) do_seek = TRUE;
  }

  if (priv->last_frame == -1 || rev || do_seek) {
    int64_t xtarget_pts;
    xtimex = get_current_ticks();

#ifdef DEBUG
    fprintf(stderr, "pt a1 %ld %ld\n", priv->last_frame, tframe);
#endif
    // seek to new frame
    if (priv->pFrame) {
      av_frame_unref(priv->pFrame);
      priv->pFrame = NULL;
    }

    if (tframe == priv->last_frame) {
      // seek to same frame - we need to ensure we go back at least one frame, else we will
      // read the next packets and be one frame ahead
      if (cdata->fps)
        time = (tframe >= 0 ? (double)tframe - 1. : tframe) / cdata->fps;
      xtarget_pts = time * (double)AV_TIME_BASE;
    } else {
      // try to seek straight to keyframe
      if (!(cdata->seek_flag & LIVES_SEEK_FAST_REV)
          && tframe < priv->last_frame && priv->found_pts != -1
          && target_pts > priv->found_pts)
        xtarget_pts = priv->found_pts;
      else xtarget_pts = target_pts;
    }

    xtarget_pts += priv->ic->start_time;

    seek_target = av_rescale_q(xtarget_pts, AV_TIME_BASE_Q, s->time_base);
    if (seek_target < -priv->ic->start_time) seek_target = 0;

    av_seek_frame(priv->ic, priv->vstream, seek_target, AVSEEK_FLAG_BACKWARD);
#ifdef DEBUG
    fprintf(stderr, "new seek: %ld %ld %ld\n", priv->last_frame, seek_target, priv->ic->start_time);
#endif
    if (priv->pkt_inited) {
      // seems to be necessary before flushing buffers, but after doing seek
      if (priv->ovpdata) {
        priv->packet.data = priv->ovpdata;
        av_packet_unref(&priv->packet);
      }
      priv->ovpdata = priv->packet.data = NULL;
      priv->packet.size = 0;
    }

    avcodec_flush_buffers(cc);

    MyPts = -1;
    priv->needs_packet = TRUE;
    did_seek = TRUE;
    xtimex = (timex = get_current_ticks()) - xtimex;
    if (!rev)((lives_clip_data_t *)cdata)->fwd_seek_time = (cdata->fwd_seek_time + xtimex) / 2;
    else ((lives_clip_data_t *)cdata)->adv_timing.seekback_time = (cdata->adv_timing.seekback_time + (double)xtimex) / 2.;
  } else {
    if (cdata->fps)
      MyPts = priv->last_pts;//(double)(priv->last_frame + 1) / cdata->fps * (double)AV_TIME_BASE;
    timex = get_current_ticks();
  }
#ifdef HAVE_AVCODEC_SEND_PACKET
  snderr = FALSE;
#endif
  //
  xtimex = timex;
  do {
    while (1) {
      if (priv->needs_packet) {
        if (!priv->pkt_inited) {
          av_init_packet(&priv->packet);
          priv->pkt_inited = TRUE;
        }
        do {
          if (priv->ovpdata) {
            priv->packet.data = priv->ovpdata;
            av_packet_unref(&priv->packet);
          }
          priv->ovpdata = priv->packet.data = NULL;
          priv->packet.size = 0;

          ret = av_read_frame(priv->ic, &priv->packet);
          priv->ovpdata = priv->packet.data;

#ifdef DEBUG
          fprintf(stderr, "ret was %d for tframe %ld\n", ret, tframe);
#endif
          if (ret < 0) {
            xframe = (int64_t)((double)MyPts / (double)AV_TIME_BASE * cdata->fps - .5);
            priv->last_frame = xframe;
            goto cleanup;
          }
        } while (priv->packet.stream_index != priv->vstream);
        priv->needs_packet = FALSE;
      }

      if (MyPts == -1) {
        index_entry *idx;
        MyPts = priv->packet.pts;
        MyPts = av_rescale_q(MyPts, s->time_base, AV_TIME_BASE_Q) - priv->ic->start_time;
        priv->found_pts = MyPts; // PTS of start of frame, set so start_time is zero
        idx = index_get(priv->idxc, MyPts);
        if (!idx || idx->dts != MyPts)
          idx = index_add(priv->idxc, tframe, MyPts);
        else if (tframe > idx->offs) idx->offs = tframe;
        xframe = (int64_t)((double)MyPts / (double)AV_TIME_BASE * cdata->fps - .5);
        if (tframe - xframe + 1 > cdata->kframe_dist_max)
          ((lives_clip_data_t *)cdata)->kframe_dist_max = tframe - xframe + 1;
        //fprintf(stderr, "IDX ADD %ld, %ld %ld\n", MyPts, (int64_t)((double)MyPts / (double)AV_TIME_BASE * cdata->fps), idx->offs);
      }

#ifdef DEBUG
      fprintf(stderr, "pt b1 %ld %ld %ld %ld\n", MyPts, target_pts, seek_target,
              (int64_t)((double)MyPts / (double)AV_TIME_BASE * cdata->fps - .5));
#endif
      // decode any frames from this packet
      if (!priv->pFrame) priv->pFrame = av_frame_alloc();

#ifdef HAVE_AVCODEC_SEND_PACKET
      ret = avcodec_send_packet(cc, &priv->packet);
      priv->needs_packet = TRUE;
      if (!ret || (!snderr && ret == AVERROR(EAGAIN))) {
        ret = avcodec_receive_frame(cc, priv->pFrame);
        if (ret && ret != AVERROR(EAGAIN)) {
          //avcodec_flush_buffers(cc);
          continue;
        }
        gotFrame = TRUE;
        snderr = FALSE;
        if (ret) priv->needs_packet = FALSE;
      } else {
        if (!ret || ret == AVERROR(EAGAIN)) snderr = FALSE;
        else {
          snderr = TRUE;
          //avcodec_flush_buffers(cc);
        }
        continue;
      }
#else

#if LIBAVCODEC_VERSION_MAJOR >= 52
      ret = avcodec_decode_video2(cc, priv->pFrame, &gotFrame, &priv->packet);
      if (ret < 0) {
        fprintf(stderr, "avcode_decode_video2 returned %d for frame %ld !\n", ret, tframe);
        goto cleanup;
      }
      ret = FFMIN(ret, priv->packet.size);
      priv->packet.data += ret;
      priv->packet.size -= ret;
#else
      ret = avcodec_decode_video(cc, priv->pFrame, &gotFrame, priv->packet.data, priv->packet.size);
      priv->packet.size = 0;
#endif
      if (priv->packet.size == 0) {
        priv->needs_packet = TRUE;
      }
#endif
      if (gotFrame) break;
    }
#ifdef DEBUG
    fprintf(stderr, "pt 1 %ld %d %ld %ld %d %d\n", tframe, gotFrame, MyPts, gotFrame ? priv->pFrame->best_effort_timestampy : 0,
            priv->pFrame->color_trc, priv->pFrame->color_range);
#endif

#ifdef TEST_CACHING
    if (gotFrame && priv->cachemax > 0) {
      add_to_cache(priv, MyPts);
      fprintf(stderr, "adding to cache: %ld %p %p\n", MyPts, priv->pFrame, priv->pFrame->data);
    }
#endif

    xtimex = (timex = get_current_ticks()) - xtimex;

    if (cdata->max_decode_fps > 0.) {
      ((lives_clip_data_t *)cdata)->max_decode_fps = (cdata->max_decode_fps + 1000000. / (double)xtimex) / 2.;
    } else ((lives_clip_data_t *)cdata)->max_decode_fps = 1000000. / (double)xtimex;

    is_keyframe = FALSE;

    if (cdata->kframe_dist && cdata->fps) {
      int64_t frame = (int64_t)((double)MyPts / (double)AV_TIME_BASE * cdata->fps - .5) + 1;
      if (!(frame % cdata->kframe_dist)) is_keyframe = TRUE;
    }

    if (did_seek) {
      if (is_keyframe) {
        if (cdata->adv_timing.k_time > 0.)
          ((lives_clip_data_t *)cdata)->adv_timing.ks_time = (cdata->adv_timing.ks_time + (double)xtimex) / 2.;
        else
          ((lives_clip_data_t *)cdata)->adv_timing.ks_time = (double)xtimex;
      } else {
        if (cdata->adv_timing.ib_time > 0.)
          ((lives_clip_data_t *)cdata)->adv_timing.ib_time = (cdata->adv_timing.ib_time * 3. + (double)xtimex) / 4.;
        else
          ((lives_clip_data_t *)cdata)->adv_timing.ib_time = (double)xtimex;
      }
    } else {
      if (is_keyframe) {
        if (cdata->adv_timing.k_time > 0.)
          ((lives_clip_data_t *)cdata)->adv_timing.k_time = (cdata->adv_timing.k_time + (double)xtimex) / 2.;
        else
          ((lives_clip_data_t *)cdata)->adv_timing.k_time = (double)xtimex;
      } else {
        if (cdata->adv_timing.ib_time > 0.)
          ((lives_clip_data_t *)cdata)->adv_timing.ib_time = (cdata->adv_timing.ib_time * 3. + (double)xtimex) / 4.;
        else
          ((lives_clip_data_t *)cdata)->adv_timing.ib_time = (double)xtimex;
      }
    }

    //fprintf(stderr, "VALS %ld -> %ld, %d\n", MyPts, target_pts, gotFrame);
    if (MyPts >= target_pts - 1) {
      //fprintf(stderr, "frame found !\n");
      if (cdata->fps) {
        MyPts += (double)AV_TIME_BASE / cdata->fps;
        priv->last_pts = MyPts;
      }
      hit_target = TRUE;
      xtimex = timex;
      break;
    }

    // otherwise discard this frame
    if (cdata->fps) MyPts += (double)AV_TIME_BASE / cdata->fps;
    //#ifndef TEST_CACHING
    //av_frame_unref(priv->pFrame);
    //#endif
    priv->pFrame = NULL;
    did_seek = FALSE;
    xtimex = timex;
  } while (!hit_target);

  if (cdata->max_decode_fps > 0. && tframe > priv->last_frame) {
    int64_t jump_limit = (double)cdata->fwd_seek_time / 1000000. / cdata->max_decode_fps + .5;
    //fprintf(stderr, "JL is %ld %f %f\n", jump_limit, (double)cdata->fwd_seek_time, cdata->max_decode_fps);
    if (jump_limit < 2) jump_limit = 2;
    ((lives_clip_data_t *)cdata)->jump_limit = jump_limit;
  }

  if (cdata->adv_timing.seekback_time <= 0.) sbtime = (double)cdata->fwd_seek_time;
  else sbtime = cdata->adv_timing.seekback_time;

  if (sbtime > 0. && sbtime > FAST_SEEK_LIMIT * 1000)
    ((lives_clip_data_t *)cdata)->seek_flag &= ~LIVES_SEEK_FAST_REV;
  else
    ((lives_clip_data_t *)cdata)->seek_flag |= LIVES_SEEK_FAST_REV;

  if (cdata->fwd_seek_time && cdata->fwd_seek_time > FAST_SEEK_LIMIT) {
    ((lives_clip_data_t *)cdata)->seek_flag |= LIVES_SEEK_NEEDS_CALCULATION;
    ((lives_clip_data_t *)cdata)->seek_flag &= ~LIVES_SEEK_FAST;
  } else {
    ((lives_clip_data_t *)cdata)->seek_flag &= ~LIVES_SEEK_NEEDS_CALCULATION;
    ((lives_clip_data_t *)cdata)->seek_flag |= LIVES_SEEK_FAST;
  }

  /////////////////////////////////////////////////////

framedone:
  priv->last_frame = tframe;

#ifdef TEST_CACHING
framedone2:
#endif
  if (!priv->pFrame || !pixel_data) return TRUE;

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

  ((lives_clip_data_t *)cdata)->frame_gamma = WEED_GAMMA_SRGB;
  if (priv->pFrame->color_trc == AVCOL_TRC_LINEAR)
    ((lives_clip_data_t *)cdata)->frame_gamma = WEED_GAMMA_LINEAR;
  if (priv->pFrame->color_trc == AVCOL_TRC_BT709)
    ((lives_clip_data_t *)cdata)->frame_gamma = WEED_GAMMA_BT709;

  for (p = 0; p < nplanes; p++) {
    dst = pixel_data[p];
    src = priv->pFrame->data[p];

    if (!src) {
      fprintf(stderr, "avformat decoder: src pixel data was NULL for frame %ld plane %d\n", tframe, p);
      goto cleanup;
    }
    if ((rowstride = rowstrides[p]) <= 0) {
      fprintf(stderr, "avformat decoder: rowstride was %d for frame %ld plane %d\n", rowstride, tframe, p);
      goto cleanup;
    }

    xrowstride = rowstride - dstwidth + (bleft + bright) * psize;
    if (xrowstride < 0) {
      bleft += xrowstride / (psize * 2);
      bright += xrowstride / (psize * 2);
      if (bleft < 0 && bright > 0) {
        bright += bleft;
        bleft = 0;
      }
      if (bright < 0 && bleft > 0) {
        bleft += bright;
        bright = 0;
      }
      if (bleft < 0 || bright < 0) {
        dstwidth += (bleft + bright) * psize;
        bleft = bright = 0;
      }
    }

    dst += bleft * psize + btop * rowstride;
    xheight = bbot - btop;

    if (cdata->rec_rowstrides) {
      ((lives_clip_data_t *)cdata)->rec_rowstrides[p] = priv->pFrame->linesize[p];
    }

    if (rowstride == priv->pFrame->linesize[p] && bleft == bright && bleft == 0) {
      (*cdata->ext_memcpy)(dst, src, rowstride * xheight);
    } else {
      for (i = 0; i < xheight; i++) {
        (*cdata->ext_memcpy)(&dst[rowstride * i], &src[priv->pFrame->linesize[p] * i], dstwidth);
      }
    }
    if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P || pal == WEED_PALETTE_YUV422P)) {
      dstwidth >>= 1;
      bleft >>= 1;
      bright >>= 1;
    }
    if (p == 0 && (pal == WEED_PALETTE_YUV420P || pal == WEED_PALETTE_YVU420P)) {
      btop >>= 1;
      bbot >>= 1;
    }
  }

  xtimex = get_current_ticks() - xtimex;
  if (cdata->adv_timing.const_time > 0.)
    ((lives_clip_data_t *)cdata)->adv_timing.const_time = (cdata->adv_timing.const_time + (double)xtimex) / 2.;
  else
    ((lives_clip_data_t *)cdata)->adv_timing.const_time = (double)xtimex;
  return TRUE;

cleanup:
  if (priv->ovpdata) {
    priv->packet.data = priv->ovpdata;
    av_packet_unref(&priv->packet);
  }
  priv->ovpdata = priv->packet.data = NULL;
  priv->packet.size = 0;
  priv->needs_packet = TRUE;
  return FALSE;
}


int get_frac(double *num, double *den) {
  double res;
  int a = 0, b = 1, c = 1, d = 1, m, n, i;

  *den /= *num;

  for (i = 0; i < 10000; i++) {
    m = a + b;
    n = c + d;
    res = (double)m / (double)n;
    //fprintf(stderr, "frac is %d / %d %f %f\n", m, n, res, 1. / *den);
    if (fabs(res - 1. / *den) < 0.001) break;
    if (res > 1. / *den) {
      b = m;
      d = n;
    } else {
      a = m;
      c = n;
    }
  }
  if (i == 10000) return 0;
  *num = m;
  *den = n;
  return 1;
}

void dump_kframes(const lives_clip_data_t *cdata) {
  lives_av_priv_t *priv = cdata->priv;
  index_entry *xidx = priv->idxc->idxhh;
  while (xidx) {
    //fprintf(stderr, "VALS %ld %ld\n", pts, xidx->dts);
    //if (xidx->next)
    //fprintf(stderr, "VALS2 %ld\n", xidx->next->dts);
    fprintf(stderr, "KFRAME %ld -> %ld\n", (int64_t)((double)xidx->dts / (double)AV_TIME_BASE * cdata->fps - .5), xidx->offs);
    xidx = xidx->next;
  }
}

static int64_t kf_regular(const lives_clip_data_t *cdata) {
  lives_av_priv_t *priv = cdata->priv;
  index_entry *xidx = priv->idxc->idxhh;
  int64_t reg = 0, framea, frameb, diff;
  double num, den;

  framea = (double)xidx->dts / (double)AV_TIME_BASE * cdata->fps - .5 - 1;

  while (xidx) {
    //fprintf(stderr, "VALS %ld %ld\n", pts, xidx->dts);
    //if (xidx->next)
    //fprintf(stderr, "VALS2 %ld\n", xidx->next->dts);
    if (xidx->next) {
      frameb = (double)xidx->next->dts / (double)AV_TIME_BASE * cdata->fps - .5;
      //int64_t diff = xidx->next->dts - xidx->dts;
      diff = frameb - framea;
      if (!reg) reg = diff;
      else {
        num = (double)reg;
        den = (double)diff;
        //fprintf(stderr, "FRAC22 %f / %f\n", num,den);
        if (fabs(num - den) < (double)AV_TIME_BASE) {
          xidx = xidx->next;
          continue;
        }
        if (!get_frac(&num, &den) || !num || !den) {
          reg = 0;
          break;
        }
        //fprintf(stderr, "FRAC %f / %f\n", num,den);
        reg = (int64_t)((double)reg / num);
      }
    }
    framea = frameb;
    xidx = xidx->next;
  }

  //fprintf(stderr, "REGG is %ld\n", reg);
  if (!reg) return 0;

  //dump_kframes(cdata);

  xidx = priv->idxc->idxhh;
  while (xidx) {
    double xtime = (double)xidx->dts / (double)AV_TIME_BASE;
    framea = (int64_t)(xtime * cdata->fps - .5);
    //fprintf(stderr, "CHECK %ld (%ld - %ld) vs %ld\n", xidx->offs - frame + 1, frame, xidx->offs, reg);
    if (xidx->offs - framea + 1 > reg) return 0;
    xidx = xidx->next;
  }

  //fprintf(stderr, "REGG2 is %ld\n", reg);
  return reg;
}


static int64_t kf_before(const lives_clip_data_t *cdata, int64_t tframe) {
  index_entry *idx;
  int64_t target_pts, kf = -1, reg;
  double xtime;
  lives_av_priv_t *priv = cdata->priv;
  xtime = ((double)tframe - .5) / cdata->fps;
  target_pts = xtime * (double)AV_TIME_BASE;
  idx = index_get(priv->idxc, target_pts);
  if (!idx) return -1;
  //fprintf(stderr, "KF FOUND %ld %ld\n", (int64_t)((double)idx->dts / (double)AV_TIME_BASE * cdata->fps - .5), idx->offs);
  if (idx->offs >= tframe) {
    xtime = (double)idx->dts / (double)AV_TIME_BASE;
    kf = (int64_t)(xtime * cdata->fps - .5);
    if (kf > tframe) {
      fprintf(stderr, "KF %ld %ld %ld\n", kf, tframe, idx->offs);
      abort();
    }
  } else if ((reg = kf_regular(cdata))) {
    kf = (int64_t)(tframe / reg) * reg;
    if (kf > tframe) abort();
    ((lives_clip_data_t *)cdata)->kframe_dist = reg;
  }
  return kf;
}


double estimate_delay(const lives_clip_data_t *xcdata, int64_t tframe) {
  // return as accurate as we can, an estimate of the time (seconds) to decode and return frame tframe
  // - for the calculation we start by looking at priv->last_frame
  // then, determine if we can reach target by decoding frames in sequence, or if we need to seek first
  // if we need to seek then try to guess the keyframe we will seek to, then add the decode time from keyframe to target
  // for the former, the calculation is just delta X decode_time
  // for the latter, we have seek time (may depend on direction and distance)
  // then delta_from_keyframe * decode_time
  // notes: time for decoding a keyframe may be different from decoding and I or B frame, almost certainly after a seek
  // relevant values: k_decode_seq, i_decode, seek_fwd (per frame delta), seek_back (per frame delta)
  // k_decode_seek, memcpy_time

  // if we cannot calculate an estimate, we return a value < 0.

  lives_clip_data_t *cdata = (lives_clip_data_t *)xcdata;
  lives_av_priv_t *priv;
  double est = -1.;

  est_noseek = 0.;

  if (!cdata) return est;
  priv = cdata->priv;
  if (tframe == priv->last_frame && priv->pFrame) est = cdata->adv_timing.const_time;
  else {
    if (cdata && cdata->fps) {
      double sbtime, ibtime, ktime, kstime;
      int64_t delta = tframe - priv->last_frame, kf, kfd;

      cdata->adv_timing.ctiming_ratio = 1.;
      ibtime = cdata->adv_timing.ib_time;
      ktime = cdata->adv_timing.k_time;
      kstime = cdata->adv_timing.ks_time;

      if (ibtime <= 0. && cdata->max_decode_fps >= 0.) ibtime = 1000000. / cdata->max_decode_fps;
      if (ktime <= 0. && cdata->max_decode_fps >= 0.) ktime = 1000000. / cdata->max_decode_fps;
      if (kstime <= 0. && cdata->max_decode_fps >= 0.) kstime = 1000000. / cdata->max_decode_fps;

      if ((sbtime = cdata->adv_timing.seekback_time) <= 0.) sbtime = (double)cdata->fwd_seek_time;

      //estimate with seek
      kf = kf_before(cdata, tframe);
      //fprintf(stderr, "KF is %ld for %ld %ld\n", kf, tframe, cdata->kframe_dist_max);
      if (kf < 0) {
        if (tframe < cdata->kframe_dist_max) kfd = tframe;
        else kfd = cdata->kframe_dist_max;
        //fprintf(stderr, "KFD1 is %ld\n", kfd);
      } else {
        kfd = tframe - kf;
        if (kfd > cdata->kframe_dist_max) kfd = cdata->kframe_dist_max;
        //fprintf(stderr, "KFD2 is %ld\n", kfd);
      }

      est = cdata->adv_timing.const_time + kstime + kfd * ibtime;
      if (delta > 0) est += (double)cdata->fwd_seek_time;
      else est += sbtime;
      /* fprintf(stderr, "ESTIMa is %f %f %f %ld %f %ld %f\n", est, cdata->adv_timing.const_time, kstime, */
      /* 	    kfd, ibtime, cdata->fwd_seek_time, sbtime); */
      if (priv->last_frame != -1 && delta > 0) {
        int nks = 0;
        if (cdata->kframe_dist) nks = (tframe + 1) / cdata->kframe_dist - (priv->last_frame + 1) / cdata->kframe_dist;
        est_noseek = cdata->adv_timing.const_time + nks * ktime + (delta - nks) * ibtime;
        //fprintf(stderr, "ESTIMb is %f\n", est_noseek);
        if (est_noseek < est) est = est_noseek;
        est_noseek *= cdata->adv_timing.ctiming_ratio / 1000000.;
      }
    }
  }
  if (est > 0.) est *= cdata->adv_timing.ctiming_ratio / 1000000.;
  //fprintf(stderr, "ESTIM is %f\n", est);
  return est;
}


void clip_data_free(lives_clip_data_t *cdata) {
  lives_av_priv_t *priv = cdata->priv;
  if (cdata->URI) {
    detach_stream(cdata);
  }
  if (priv && priv->idxc) idxc_release(cdata, priv->idxc);
  lives_struct_free(&cdata->lsd);
}


void module_unload(void) {
  idxc_release_all();
}
