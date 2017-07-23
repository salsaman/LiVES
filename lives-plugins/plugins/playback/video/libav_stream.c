// LiVES - libav stream engine
// (c) G. Finch 2017 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include "../../decoders/libav_helper.h"

static int mypalette = WEED_PALETTE_END;
static int palette_list[2];

static int clampings[3];
static int myclamp;

static char plugin_version[64] = "LiVES libav stream engine version 1.0";

static boolean(*render_fn)(int hsize, int vsize, void **pixel_data);

static boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data);
static boolean render_frame_unknown(int hsize, int vsize, void **pixel_data);

static int ovsize, ohsize;
static int in_nchans, out_nchans;
static int in_sample_rate, out_sample_rate;
static int in_nb_samples, out_nb_samples;

static float **spill_buffers;
static int spb_len;

//static int aforms[2];

/////////////////////////////////////////////////////////////////////////

static AVFormatContext *fmtctx;
static AVCodecContext *encctx, *aencctx;
static AVStream *vStream, *aStream;

#define URI "udp://127.0.0.1:5678"
//#define URI "file2.flv"

#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC

#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720

#define STREAM_ENCODE TRUE 

// a wrapper around a single output AVStream
typedef struct OutputStream {
  AVStream *st;
  AVCodecContext *enc;
  AVCodec *codec;
  /* pts of the next frame that will be generated */
  int64_t next_pts;
  int samples_count;
  AVFrame *frame;
  AVFrame *tmp_frame;
  float t, tincr, tincr2;
  struct SwsContext *sws_ctx;
  struct SwrContext *swr_ctx;
} OutputStream;

static OutputStream ostv; // video
static OutputStream osta; // audio


//////////////////////////////////////////////


#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

const char *get_fps_list(int palette) {
  return STR(STREAM_FRAME_RATE);
}


////////////////////////////////////////////////

const char *module_check_init(void) {
  render_fn = &render_frame_unknown;
  ovsize = ohsize = 0;

  fmtctx = NULL;
  encctx = NULL;
  
  av_register_all();
  avformat_network_init();
  
  return NULL;
}


const char *version(void) {
  return plugin_version;
}


const char *get_description(void) {
  return "The libav_stream plugin provides realtime streaming over a local network (UDP)\n";
}


const int *get_palette_list(void) {
  palette_list[0] = WEED_PALETTE_YUV420P;
  palette_list[1] = WEED_PALETTE_END;
  return palette_list;
}


uint64_t get_capabilities(int palette) {
  return 0;
}

/*
  const int *get_audio_fmts() {
  // this is not yet documented in the manual, but is an optional function to get a list of audio formats. 
  aforms[0] = 3; // vorbis - see src/plugins.h
  aforms[1] = -1; // end

  return aforms;
  }
*/

const char *get_init_rfx(void) {
  return \
    "<define>\\n\
|1.7\\n\
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params> \\n\
syncd|A/V Sync _delay (seconds)|num2|4.|0.|20.|\\n\
address|Icecast server _address|string|127.0.0.1|16|\\n\
port|Icecast server _port|num0|8000|1024|65535|\\n\
passwd|Icecast server pass_word|string|hackme|32|\\n\
mountpt|Icecast _mount point|string|/stream.ogg|80|\\n\
</params> \\n\
<param_window> \\n\
special|password|3|\\n\
</param_window> \\n\
<onchange> \\n\
</onchange> \\n\
";
}


const int *get_yuv_palette_clamping(int palette) {
  if (palette == WEED_PALETTE_YUV420P) {
    clampings[0] = WEED_YUV_CLAMPING_CLAMPED;
    clampings[1] = -1;
  } else clampings[0] = -1;
  return clampings;
}


boolean set_yuv_palette_clamping(int clamping_type) {
  myclamp = clamping_type;
  return TRUE;
}


boolean set_palette(int palette) {
  if (palette == WEED_PALETTE_YUV420P) {
    mypalette = palette;
    render_fn = &render_frame_yuv420;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}


boolean set_fps(double in_fps) {
  return TRUE;
}


static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height) {
  AVFrame *picture;
  int ret;
  picture = av_frame_alloc();
  if (!picture)
    return NULL;
  picture->format = pix_fmt;
  picture->width  = width;
  picture->height = height;
  /* allocate the buffers for the frame data */
  ret = av_frame_get_buffer(picture, 32);
  if (ret < 0) {
    fprintf(stderr, "Could not allocate frame data.\n");
    return NULL;
  }
  return picture;
}


static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
				  uint64_t channel_layout,
				  int sample_rate, int nb_samples) {
  AVFrame *frame = av_frame_alloc();
  int ret;
 
  if (!frame) {
    fprintf(stderr, "Error allocating an audio frame\n");
    return NULL;
  }
 
  frame->format = sample_fmt;
  frame->channel_layout = channel_layout;
  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;
 
  ret = av_frame_get_buffer(frame, 0);
  if (ret < 0) {
    fprintf(stderr, "Error allocating an audio buffer\n");
    return NULL;
  }
 
  return frame;
}


static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg) {
  AVCodecContext *c;
  int ret;
  AVDictionary *opt = NULL;
 
  c = ost->enc;
 
  /* open it */
  //av_dict_copy(&opt, opt_arg, 0);
  
  ret = avcodec_open2(c, codec, &opt);
  av_dict_free(&opt);
  if (ret < 0) {
    fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
    return;
  }
 
  if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
    fprintf(stderr, "varaudio, nice\n");
    out_nb_samples = 0;
  }
  else {
    out_nb_samples = c->frame_size;
    fprintf(stderr, "nb samples is %d\n", out_nb_samples);
  }

  /* copy the stream parameters to the muxer */
  /*     ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	 if (ret < 0) {
         fprintf(stderr, "Could not copy the stream parameters\n");
         exit(1);
	 }*/
 
}


static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id) {
  AVCodecContext *c;

  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) {
    fprintf(stderr, "Could not find encoder for '%s'\n",
	    avcodec_get_name(codec_id));
    return;
  }

  c = avcodec_alloc_context3(*codec);
  if (!c) {
    fprintf(stderr, "Could not allocate video codec context\n");
    return;
  }

  ost->st = avformat_new_stream(oc, *codec); // stream(s) created from format_ctx and codec
  if (!ost->st) {
    fprintf(stderr, "Could not allocate stream\n");
    return;
  }

  ost->st->codec = ost->enc = c;
  ost->st->id = oc->nb_streams-1;

  /* Some formats want stream headers to be separate. */
  if (!STREAM_ENCODE && oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;
  
}


boolean init_audio(int _in_sample_rate, int _in_nchans, int _out_sample_rate, int _out_nchans) {
  // must be called after init_screen(), before next render_audio_frame()

  AVCodecContext *c;
  AVCodec *codec = osta.codec;
  int i, ret;

  in_sample_rate = _in_sample_rate;
  in_nchans = _in_nchans;
  out_sample_rate = _out_sample_rate;
  out_nchans = _out_nchans;
  
  c = osta.enc;
  c->sample_rate = out_sample_rate;
  if (codec->supported_samplerates) {
    c->sample_rate = codec->supported_samplerates[0];
    for (i = 0; codec->supported_samplerates[i]; i++) {
      if (codec->supported_samplerates[i] == out_sample_rate) {
	c->sample_rate = out_sample_rate;
	break;
      }
    }
  }

  /* create resampler context */
  osta.swr_ctx = swr_alloc();
  if (!osta.swr_ctx) {
    fprintf(stderr, "Could not allocate resampler context\n");
    return FALSE;
  }

  in_nb_samples = out_nb_samples;
  
  if (out_nb_samples != 0) {
    /* compute src number of samples */
    in_nb_samples = av_rescale_rnd(swr_get_delay(osta.swr_ctx, c->sample_rate) + out_nb_samples,
				    in_sample_rate, c->sample_rate, AV_ROUND_DOWN);

    /* confirm destination number of samples */
    int dst_nb_samples = av_rescale_rnd(in_nb_samples,
				    c->sample_rate, in_sample_rate, AV_ROUND_UP);

    av_assert0(dst_nb_samples == out_nb_samples);
  }
  
  c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
  c->channel_layout = (out_nchans == 2 ? AV_CH_LAYOUT_STEREO: AV_CH_LAYOUT_MONO);
  if (codec->channel_layouts) {
    c->channel_layout = codec->channel_layouts[0];
    for (i = 0; codec->channel_layouts[i]; i++) {
      if (codec->channel_layouts[i] == (out_nchans == 2 ? AV_CH_LAYOUT_STEREO: AV_CH_LAYOUT_MONO)) {
	c->channel_layout = (out_nchans == 2 ? AV_CH_LAYOUT_STEREO: AV_CH_LAYOUT_MONO);
	break;
      }
    }
  }
  c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);

  c->bit_rate    = 320000;

  osta.st->time_base = (AVRational){ 1, c->sample_rate };

  if (out_nb_samples > 0) 
    osta.frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, out_nb_samples);
  else
    osta.frame = NULL;
  
  /* set options */
  av_opt_set_int       (osta.swr_ctx, "in_channel_count",   in_nchans,       0);
  av_opt_set_int       (osta.swr_ctx, "in_sample_rate",     in_sample_rate,    0);
  av_opt_set_sample_fmt(osta.swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_FLTP,0);
  av_opt_set_int       (osta.swr_ctx, "out_channel_count",  c->channels,       0);
  av_opt_set_int       (osta.swr_ctx, "out_sample_rate",    c->sample_rate,    0);
  av_opt_set_sample_fmt(osta.swr_ctx, "out_sample_fmt",     AV_SAMPLE_FMT_S16, 0);
 
  /* initialize the resampling context */
  if ((ret = swr_init(osta.swr_ctx)) < 0) {
    fprintf(stderr, "Failed to initialize the resampling context\n");
    return FALSE;
  }

  if (in_nb_samples != 0) {
    spill_buffers = (float **) malloc(in_nchans * sizeof(float *));
    for (i = 0; i < in_nchans; i++) {
      spill_buffers[i] = (float *) malloc(in_nb_samples * sizeof(float));
    }
  }
  spb_len = 0;

  osta.samples_count = 0;
  
  return TRUE;
}


boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  int vcodec_id = AV_CODEC_ID_H264;
  int acodec_id = AV_CODEC_ID_MP3;
  AVOutputFormat *opfmt;
  AVCodec *codec, *acodec;

  //AVDictionary *opts = NULL;

  int ret;

  fprintf(stderr,"init_screen\n");
  
  ostv.frame = osta.frame = NULL;
  vStream = aStream = NULL;

  ostv.sws_ctx = NULL;
  osta.swr_ctx = NULL;

  if (mypalette == WEED_PALETTE_END) {
    fprintf(stderr, "libav stream plugin error: No palette was set !\n");
    return FALSE;
  }
    
  fmtctx = avformat_alloc_context();

  // open flv
  opfmt = av_guess_format("flv", NULL, NULL);
  avformat_alloc_output_context2(&fmtctx, opfmt, "flv", URI);
  
  if (!fmtctx) {
    printf("Could not deduce output format from file extension: using flv.\n");
    avformat_alloc_output_context2(&fmtctx, NULL, "flv", URI);
  }
  if (!fmtctx) return FALSE;

  fprintf(stderr,"init_screen2 %p\n", fmtctx);

  // add the video stream
  add_stream(&ostv, fmtctx, &codec, vcodec_id);
  vStream = ostv.st;
  ostv.codec = codec;
  
  ostv.enc = encctx = vStream->codec;

  // override defaults
  vStream->time_base = (AVRational){1, STREAM_FRAME_RATE};  // 25.0 fps
  vStream->codec->time_base = vStream->time_base;
  
  vStream->codec->width = width;
  vStream->codec->height = height;
  vStream->codec->pix_fmt = PIX_FMT_YUV420P;
  
  vStream->codec->bit_rate = 7280000 ; // allow user to set

  av_opt_set(encctx->priv_data, "preset", "ultrafast", 0);
  vStream->codec->gop_size = 10;

  if (vcodec_id == AV_CODEC_ID_MPEG2VIDEO) {
    /* just for testing, we also add B frames */
    vStream->codec->max_b_frames = 2;
  }
  if (vcodec_id == AV_CODEC_ID_MPEG1VIDEO) {
    /* Needed to avoid using macroblocks in which some coeffs overflow.
     * This does not happen with normal video, it just happens here as
     * the motion of the chroma plane does not match the luma plane. */
    vStream->codec->mb_decision = 2;
  }

  /* open vido codec */
  if (avcodec_open2(encctx, codec, NULL) < 0) {
    fprintf(stderr, "Could not open codec\n");
    return FALSE;
  }

  // audio
  //#define ADD_AUDIO
#ifdef ADD_AUDIO
  // add the audio stream
  add_stream(&osta, fmtctx, &acodec, acodec_id);
  osta.codec = acodec;
  aStream = osta.st;
  osta.enc = aencctx = aStream->codec;
  open_audio(fmtctx, acodec, &osta, NULL); 
#endif
  
  /* open output file */
  if (!(fmtctx->oformat->flags & AVFMT_NOFILE)) {
    fprintf(stderr, "opening file");
    ret = avio_open(&fmtctx->pb, URI, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open '%s': %s\n", URI,
	      av_err2str(ret));
      return FALSE;
    }

    /* write header */
    ret = avformat_write_header(fmtctx, NULL);
    if (ret < 0) {
      fprintf(stderr, "Error occurred when writing header: %s\n",
	      av_err2str(ret));
      return FALSE;
    }
  }

  /* create (container) libav video frame */
  ostv.frame = alloc_picture(PIX_FMT_YUV420P, FRAME_WIDTH, FRAME_HEIGHT);
  if (ostv.frame == NULL) {
    fprintf(stderr, "Could not allocate video frame\n");
    return FALSE;
  }

  av_dump_format(fmtctx, 0, URI , 1);
  
  ostv.next_pts = osta.next_pts = 0;
  return TRUE;
}


boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **rd, void **pp) {
  // call the function which was set in set_palette
  return render_fn(hsize, vsize, pixel_data);
}


static void log_packet(const AVPacket *pkt) {
  AVRational *time_base = &fmtctx->streams[pkt->stream_index]->time_base;
  printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
	 av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
	 av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
	 av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
	 pkt->stream_index);
}


static int write_video_frame(const AVRational *time_base, AVPacket *pkt) {
  /* rescale output packet timestamp values from codec to stream timebase */
  av_packet_rescale_ts(pkt, *time_base, vStream->time_base);
  pkt->stream_index = vStream->index;
  /* Write the compressed frame to the media file. */
  //log_packet(pkt);
  return av_interleaved_write_frame(fmtctx, pkt);
}


static void copy_yuv_image(AVFrame *pict, int width, int height, const uint8_t * const *pixel_data) {
  int y, ret;
  int hwidth = width >> 1;
  int hheight = height >> 1;
  
  /* when we pass a frame to the encoder, it may keep a reference to it
   * internally;
   * make sure we do not overwrite it here
   */
  ret = av_frame_make_writable(pict);
  if (ret < 0) return;

  /* Y */
  for (y = 0; y < height; y++)
    memcpy(&pict->data[0][y * pict->linesize[0]], &pixel_data[0][y*width], width);
  /* Cb and Cr */
  for (y = 0; y < hheight; y++) {
    memcpy(&pict->data[1][y * pict->linesize[1]], &pixel_data[1][y * hwidth], hwidth);
    memcpy(&pict->data[2][y * pict->linesize[2]], &pixel_data[2][y * hwidth], hwidth);
  }
}


static AVFrame *get_video_frame(const uint8_t * const *pixel_data, int hsize, int vsize) {
  AVCodecContext *c = ostv.enc;
  int istrides[3];

  if (ostv.sws_ctx != NULL && (hsize != ohsize || vsize != ovsize)) {
    sws_freeContext(ostv.sws_ctx);
    ostv.sws_ctx = NULL;
  }
  
  if (hsize != c->width || vsize != c->height) {
    if (ostv.sws_ctx == NULL) {
      ostv.sws_ctx = sws_getContext(hsize, vsize,
				    AV_PIX_FMT_YUV420P,
				    c->width, c->height,
				    AV_PIX_FMT_YUV420P,
				    SCALE_FLAGS, NULL, NULL, NULL);
      if (ostv.sws_ctx == NULL) {
	fprintf(stderr,
		"libav_stream: Could not initialize the conversion context\n");
	return NULL;
      }
      ohsize = hsize;
      ovsize = vsize;
      istrides[0] = hsize;
      istrides[1] = istrides[2] = hsize >> 1;
  
      sws_scale(ostv.sws_ctx,
		(const uint8_t * const *)pixel_data, istrides,
		0, vsize, ostv.frame->data, ostv.frame->linesize);
    }
  }
  else {
    copy_yuv_image(ostv.frame, hsize, vsize, pixel_data);
  }
  
  ostv.frame->pts = ostv.next_pts++;
  return ostv.frame;
}


boolean render_audio_frame(float **audio, int nsamps)  {
  AVCodecContext *c;
  AVPacket pkt = { 0 }; // data and size must be 0;
  
  float *abuff[in_nchans];
  
  int ret;
  int got_packet;
  int nb_samples;
  int i;

  av_init_packet(&pkt);
  c = osta.enc;

  for (i= 0; i < in_nchans; i++) {
    abuff[i] = audio[i];
  }

  if (osta.frame != NULL) {
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally;
     * make sure we do not overwrite it here
     */
    ret = av_frame_make_writable(osta.frame);
    if (ret < 0) return FALSE;
  }
 
  while (nsamps > 0) {
    if (out_nb_samples != 0) {
      if (nsamps + spb_len < in_nb_samples) {
	// have l.t. one full buffer to send, store this for next time
	for (i= 0; i < in_nchans; i++) {
	  memcpy(&spill_buffers[i + spb_len], &abuff[i], nsamps * sizeof(float));
	}
	spb_len += nsamps;
	break;
      }
    
      if (spb_len > 0) {
	// have data in buffers from last call. fill these up and clear them first 
	for (i= 0; i < in_nchans; i++) {
	  memcpy(&spill_buffers[i + spb_len], &abuff[i], (in_nb_samples - spb_len) * sizeof(float));
	  abuff[i] += in_nb_samples - spb_len;
	}
      }
      nb_samples = out_nb_samples;
    }
    else {
      // codec accepts variable nb_samples, so encode all
      in_nb_samples = nsamps;
      nb_samples = av_rescale_rnd(in_nb_samples,
				  c->sample_rate, in_sample_rate, AV_ROUND_UP);
      osta.frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
    }
      
    /* convert to destination format */
    ret = swr_convert(osta.swr_ctx,
		      osta.frame->data, nb_samples,
		      spb_len == 0 ? (const uint8_t **)abuff : (const uint8_t **)spill_buffers, in_nb_samples);
    if (ret < 0) {
      fprintf(stderr, "Error while converting audio\n");
      return FALSE;
    }
    
    osta.frame->pts = av_rescale_q(osta.samples_count, (AVRational){1, c->sample_rate}, c->time_base);
    osta.samples_count += nb_samples;
    
    ret = avcodec_encode_audio2(c, &pkt, osta.frame, &got_packet);
    if (ret < 0) {
      fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
      return FALSE;
    }
    /*
    if (got_packet) {
      ret = write_audio_frame(oc, &c->time_base, ost->st, &pkt);
      if (ret < 0) {
	fprintf(stderr, "Error while writing audio frame: %s\n",
		av_err2str(ret));
	exit(1);
      }
    }
    */
    for (i= 0; i < in_nchans; i++) {
      abuff[i] += in_nb_samples - spb_len;
    }
    nsamps -= in_nb_samples - spb_len;
    spb_len = 0;

    if (out_nb_samples == 0) {
      if (osta.frame != NULL) av_frame_unref(&osta.frame);
      osta.frame = NULL;
      in_nb_samples = 0;
    }
  }
  return TRUE;
}


boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data) {
  AVCodecContext *c;
  AVPacket pkt = { 0 };

  int got_packet = 0;
  int ret;

  c = ostv.enc;

  // copy and scale pixel_data
  if ((ostv.frame = get_video_frame((const uint8_t * const *)pixel_data, hsize, vsize)) != NULL) {
    av_init_packet(&pkt);

    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, ostv.frame, &got_packet);

    if (ret < 0) {
      fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
      return FALSE;
    }
    if (got_packet) {
      ret = write_video_frame(&c->time_base, &pkt);
    } else {
      ret = 0;
    }
    if (ret < 0) {
      fprintf(stderr, "Error writing video frame: %s\n", av_err2str(ret));
      return FALSE;
    }
  }

  return TRUE;
}


boolean render_frame_unknown(int hsize, int vsize, void **pixel_data) {
  if (mypalette == WEED_PALETTE_END) {
    fprintf(stderr, "libav_stream plugin error: No palette was set !\n");
  }
  return FALSE;
}


void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  AVCodecContext *c;
  AVPacket pkt = { 0 };

  int got_packet = 0;
  int ret;

  int i;

  if (!STREAM_ENCODE && !(fmtctx->oformat->flags & AVFMT_NOFILE)) {
    c = ostv.enc;

    do {
      av_init_packet(&pkt);

      /* encode the image */
      ret = avcodec_encode_video2(c, &pkt, ostv.frame, &got_packet);

      if (ret < 0) {
	fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
	break;
      }
      if (got_packet) {
	ret = write_video_frame(&c->time_base, &pkt);
      } else {
	ret = 0;
      }
      if (ret < 0) {
	break;
      }
    } while (got_packet);
  }

  if (fmtctx != NULL) {
    if (!(fmtctx->oformat->flags & AVFMT_NOFILE))
      /* Write the trailer, if any. The trailer must be written before you
       * close the CodecContexts open when you wrote the header; otherwise
       * av_write_trailer() may try to use memory that was freed on
       * av_codec_close(). */
      av_write_trailer(fmtctx);
  
    /* Close the output file. */
    avio_closep(&fmtctx->pb);
  }

  if (vStream != NULL) {
    avcodec_close(vStream->codec);
    vStream = NULL;
  }

  if (aStream != NULL) {
    avcodec_close(aStream->codec);
    aStream = NULL;
  }

  if (fmtctx != NULL) {
    avformat_free_context(fmtctx);
    fmtctx = NULL;
  }

  if (ostv.frame != NULL) av_frame_unref(&ostv.frame);
  if (osta.frame != NULL) av_frame_unref(&osta.frame);

  if (ostv.sws_ctx != NULL) sws_freeContext(ostv.sws_ctx);
  if (osta.swr_ctx != NULL) swr_free(&(osta.swr_ctx));

  ostv.sws_ctx = NULL;
  osta.swr_ctx = NULL;

  for (i = 0; i < in_nchans; i++) {
    free(spill_buffers[i]);
  }
  free(spill_buffers);

}


void module_unload(void) {
  avformat_network_deinit();
}


