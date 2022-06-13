// LiVES - libav stream engine
// (c) G. Finch 2017 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define PLUGIN_UID 0X11E4474233EB27EEull

#include <stdio.h>
#include <pthread.h>

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

#include "lives-plugin.h"

#define PLUGIN_NAME "LiVES libav stream"
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 2

#define HAVE_AVUTIL
#define HAVE_AVCODEC

#include "videoplugin.h"

#ifndef NEED_LOCAL_WEED
#include <weed/weed-compat.h>
#else
#include "../../../../libweed/weed-compat.h"
#endif

#include "../../decoders/libav_helper.h"

static int intent;

static int mypalette = WEED_PALETTE_END;
static int palette_list[4];

static int avpalette;

static int clampings[3];
static int myclamp = WEED_YUV_CLAMPING_UNCLAMPED;

static boolean(*play_fn)(weed_layer_t *frame, int64_t tc, weed_layer_t *ret);


static boolean play_frame_unknown(weed_layer_t *frame, int64_t tc, weed_layer_t *ret) {
  //boolean render_frame_unknown(int hsize, int vsize, void **pixel_data) {
  if (mypalette == WEED_PALETTE_END) {
    fprintf(stderr, "libav_stream plugin error: No palette was set !\n");
  }
  return FALSE;
}

//static boolean(*render_fn)(int hsize, int vsize, void **pixel_data);

static boolean play_frame_yuv420(weed_layer_t *frame, int64_t tc, weed_layer_t *ret);

//static boolean render_frame_yuv420(int hsize, int vsize, void **pixel_data);
//static boolean render_frame_unknown(int hsize, int vsize, void **pixel_data);

static int ovsize, ohsize;
static int in_nchans, out_nchans;
static int in_sample_rate, out_sample_rate;
static int in_nb_samples, out_nb_samples;
static int maxabitrate, maxvbitrate;

static float **spill_buffers;
static int spb_len;

static double target_fps;

static pthread_mutex_t write_mutex;

/////////////////////////////////////////////////////////////////////////

static AVFormatContext *fmtctx;
static AVCodecContext *encctx, *aencctx;
static AVStream *vStream, *aStream;

#define DEFAULT_FRAME_RATE 10. /* 10 images/s */
#define SCALE_FLAGS SWS_BICUBIC

boolean stream_encode;

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

static int inited = 0;

//////////////////////////////////////////////

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

const char *get_fps_list(int palette) {
  return STR(DEFAULT_FRAME_RATE);
}

////////////////////////////////////////////////

const char *module_check_init(void) {
  play_fn = &play_frame_unknown;
  ovsize = ohsize = 0;

  fmtctx = NULL;
  encctx = NULL;

  av_register_all();
  avformat_network_init();

  target_fps = DEFAULT_FRAME_RATE;

  in_sample_rate = 0;

  intent = 0;

  pthread_mutex_init(&write_mutex, NULL);
  inited = 1;

  return NULL;
}


const char *get_description(void) {
  return "The libav_stream plugin provides realtime streaming over a local network (UDP)\n";
}


const int *get_palette_list(void) {
  palette_list[0] = WEED_PALETTE_RGB24;
  palette_list[1] = WEED_PALETTE_YUV444P;
  palette_list[2] = WEED_PALETTE_YUV420P;
  palette_list[3] = WEED_PALETTE_END;
  return palette_list;
}


uint64_t get_capabilities(int palette) {
  return 0;//VPP_CAN_RESIZE;
}


/*
  parameter template, these are returned as argc, argv in init_screen() and init_audio()
*/

#define IFBUFSIZE 16384

contract_status negotiate_contract(pl_contract *contract) {
  // intent / capabilities allows switching between different tailored interfaces
  pl_intentcap *icaps = pl_contract_get_icap(contract);
  pl_attribute *attr;
  static char ifbuf[IFBUFSIZE];
  double xfps = 0.;
  int defchans = 1, defrate, defrateval = 1;
  char *extra;

  switch (icaps->intention) {
  case PL_INTENTION_PLAY: // for now...
  case PL_INTENTION_STREAM: // LiVES VPP (streaming output)
    snprintf(ifbuf, IFBUFSIZE, "%s",
             "<define>\\n\
|1.8.5\\n		  \
</define>\\n	  \
<params>\\n\
form|_Format|string_list|0|mp4/h264/aac|ogm/theora/vorbis||\\n\
\
mbitv|Max bitrate (_video)|num0|500000|100000|1000000000|\\n\
\
achans|Audio _layout|string_list|2|none|mono|stereo||\\n\
arate|Audio _rate (Hz)|string_list|1|22050|44100|48000||\\n\
mbita|Max bitrate (_audio)|num0|320000|16000|10000000|\\n\
\
ip1|_Address to stream to|string|127|3|\\n\
ip2||string|0|3|\\n\
ip3||string|0|3|\\n\
ip4||string|1|3|\\n\
port|_port|num0|8000|1024|65535|\\n\
</params>\\n\
<param_window>\\n\
layout|\\\"Enter an IP address and port to stream to LiVES output to.\\\"|\\n\
layout|\\\"You can play the stream on the remote / local machine with e.g:\\\"|\\n\
layout|\\\"mplayer udp://127.0.0.1:8000\\\"| \\n\
layout|\\\"You are advised to start with a small frame size and low framerate,\\\"|\\n\
layout|\\\"and increase this if your network bandwidth allows it.\\\"|\\n\
layout|p0||\\n\
layout|p1||\\n\
layout|p2||\\n\
layout|p3||\\n\
layout|p4||\\n\
layout|p5|\\\".\\\"|p6|\\\".\\\"|p7|\\\".\\\"|p8|fill|fill|fill|fill|\\n\
</param_window>\\n\
<onchange>\\n\
</onchange>\\n\
");
    break;

  // this is actually play, with local but without realtime or local-display hooks
  // with frame and audio data input hooks, and a transform product of a new clip (media)
  // object in the "external" state
  // caller may pass in clip object template which plugin can use to produce the clip object
  // via the template'screate instance transform
  case PL_INTENTION_TRANSCODE: // LiVES transcoding
    // mandatiry requirements - video with, height, fps, format
    // - audio rate and channels
    // - output URI, with cap local this should be filename format.
    if (pl_has_capacity(icaps, PL_CAPACITY_AUDIO)) {
      // in future these will be mandatroy requirements
      pl_attribute *attr = pl_contract_get_attr(contract, ATTR_AUDIO_RATE);
      if (attr) defrate = pl_attr_get_value(attr, int);
      else {
        // if host did not set channels set a (read / write) default of 2
        // but make setting the value mandatory (we provide a UI template for this)
        attr = pl_declare_attribute(contract, ATTR_AUDIO_RATE, PL_ATTR_INT);
        defrate = 44100; // or wahtever
        pl_attr_set_def_value(attr, defrate);
      }
      if (defrate == 22050) defrateval = 0;
      else if (defrate == 44100) defrateval = 1;
      else if (defrate == 48000) defrateval = 2;

      attr = pl_contract_get_attr(contract, ATTR_AUDIO_CHANNELS);
      if (attr) {
        defchans = pl_attr_get_value(attr, int);
        extra = strdup("");
      } else {
        if (pl_attr_get_rdonly(attr))  extra = strdup("special|ignored|2|3|");
        else {
          // if host did not set channels set a (read / write) default of 2
          // but make setting the value mandatory (we provide a UI template for this)
          attr = pl_declare_attribute(contract, ATTR_AUDIO_CHANNELS, PL_ATTR_INT);
          pl_attr_set_def_value(attr, 2);
          extra = strdup("");
        }
      }
    } else extra = strdup("");
    /* if ((attr = pl_contract_get_attr(contract, ATTR_VIDEO_FPS))) { */
    /*   xfps = pl_attr_get_value(attr, double); */
    /* } */
    if (!(attr = pl_declare_attribute(contract, ATTR_UI_RFX_TEMPLATE, PL_ATTR_STRING)))
      attr = pl_contract_get_attr(contract, ATTR_UI_RFX_TEMPLATE);
    snprintf(ifbuf, IFBUFSIZE, "<define>\\n\
|1.8.5\\n				 \
</define>\\n\
<language_code>\\n\
0xF0\\n\
</language_code>\\n\
<params>\\n\
form|_Format|string_list|0|mp4/h264/aac|ogm/theora/vorbis|webm/vp9/opus||\\n\
\
mbitv|Max bitrate (_video)|num0|500000|100000|1000000000|\\n\
\
achans|Audio _layout|string_list|%d|none|mono|stereo||\\n\
arate|Audio _rate (Hz)|string_list|%d|22050|44100|48000||\\n\
mbita|Max bitrate (_audio)|num0|320000|16000|10000000|\\n\
\
fname|_Output file|string||\\n\
highq|_High quality (larger file size)|bool|0|0|\\n\
</params>\\n\
<param_window>\\n\
layout|hseparator|\\n\
layout|p5|\\n\
layout|p6|\\n\
layout|p0|\\n\
layout|hseparator|\\n\
special|filewrite|5|\\n\
%s\\n\
</param_window>\\n\
<onchange>\\n\
init|$p5 = (split(/\\./,$p5))[0]; if ($p0 == 0) {$p5 .= \".mp4\";} elsif ($p0 == 2) {$p5 .= \".webm\";} else {$p5 .= \".ogm\";}\\n\
0|$p5 = (split(/\\./,$p5))[0]; if ($p0 == 0) {$p5 .= \".mp4\";} elsif ($p0 == 2) {$p5 .= \".webm\";} else {$p5 .= \".ogm\";}\\n\
</onchange>\\n\
", defchans, defrateval, extra);
    free(extra);
    pl_attr_set_value(attr, ifbuf);
    return CONTRACT_STATUS_NOTREADY;
    break;
  default: break;
  }
  return CONTRACT_STATUS_AGREED;
}


const int *get_yuv_palette_clamping(int palette) {
  if (palette == WEED_PALETTE_YUV420P) {
    clampings[0] = WEED_YUV_CLAMPING_UNCLAMPED;
    clampings[1] = -1;
  } else clampings[0] = -1;
  return clampings;
}


boolean set_yuv_palette_clamping(int clamping_type) {
  myclamp = clamping_type;
  avpalette = weed_palette_to_avi_pix_fmt(avi_pix_fmt_to_weed_palette(avpalette, NULL), &myclamp);
  return TRUE;
}


boolean set_palette(int palette) {
  mypalette = palette;
  play_fn = &play_frame_yuv420;
  avpalette = weed_palette_to_avi_pix_fmt(WEED_PALETTE_YUV420P, &myclamp);
  return TRUE;
}


boolean set_fps(double in_fps) {
  target_fps = in_fps;
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


static boolean open_audio() {
  AVCodecContext *c;
  AVCodec *codec;
  AVDictionary *opt = NULL;
  int ret;
  int i;

  codec = osta.codec;
  c = osta.enc;

  c->sample_fmt  = AV_SAMPLE_FMT_FLTP;
  if (codec->sample_fmts) {
    c->sample_fmt = codec->sample_fmts[0];
    for (i = 0; codec->sample_fmts[i]; i++) {
      if (codec->sample_fmts[i] == AV_SAMPLE_FMT_FLTP) {
        c->sample_fmt = AV_SAMPLE_FMT_FLTP;
        break;
      }
    }
  }

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
  out_sample_rate = c->sample_rate;

  c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
  c->channel_layout = (out_nchans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO);
  if (codec->channel_layouts) {
    c->channel_layout = codec->channel_layouts[0];
    for (i = 0; codec->channel_layouts[i]; i++) {
      if (codec->channel_layouts[i] == (out_nchans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO)) {
        c->channel_layout = (out_nchans == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO);
        break;
      }
    }
  }
  c->channels = out_nchans = av_get_channel_layout_nb_channels(c->channel_layout);

  c->bit_rate = maxabitrate;
  c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
  ret = avcodec_open2(c, codec, &opt);
  if (ret < 0) {
    fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
    return FALSE;
  }

  if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
    fprintf(stderr, "varaudio\n");
  } else {
    out_nb_samples = c->frame_size;
    fprintf(stderr, "nb samples is %d\n", out_nb_samples);
  }

  /* create resampler context */
  osta.swr_ctx = swr_alloc();
  if (!osta.swr_ctx) {
    fprintf(stderr, "Could not allocate resampler context\n");
    return FALSE;
  }

  /* set options */
  av_opt_set_int(osta.swr_ctx, "in_channel_count",   in_nchans,       0);
  av_opt_set_int(osta.swr_ctx, "in_sample_rate",     in_sample_rate,    0);
  av_opt_set_sample_fmt(osta.swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_FLTP, 0);
  av_opt_set_int(osta.swr_ctx, "out_channel_count",  c->channels,       0);
  av_opt_set_int(osta.swr_ctx, "out_sample_rate",    c->sample_rate,    0);
  av_opt_set_sample_fmt(osta.swr_ctx, "out_sample_fmt",     c->sample_fmt, 0);

  /* initialize the resampling context */
  if ((ret = swr_init(osta.swr_ctx)) < 0) {
    fprintf(stderr, "Failed to initialize the resampling context\n");
    fprintf(stderr, "%d %d - %d %d %d\n", in_nchans, in_sample_rate, c->channels, c->sample_rate, c->sample_fmt);
    return FALSE;
  }

  in_nb_samples = out_nb_samples;
  if (out_nb_samples != 0) {
    /* compute src number of samples */
    in_nb_samples = av_rescale_rnd(swr_get_delay(osta.swr_ctx, c->sample_rate) + out_nb_samples,
                                   in_sample_rate, c->sample_rate, AV_ROUND_UP);

    /* confirm destination number of samples */
    int dst_nb_samples = av_rescale_rnd(in_nb_samples,
                                        c->sample_rate, in_sample_rate, AV_ROUND_DOWN);

    av_assert0(dst_nb_samples == out_nb_samples);
  }

  if (out_nb_samples > 0)
    osta.frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, out_nb_samples);
  else
    osta.frame = NULL;

  spill_buffers = NULL;

  if (in_nb_samples != 0) {
    spill_buffers = (float **) malloc(in_nchans * sizeof(float *));
    for (i = 0; i < in_nchans; i++) {
      spill_buffers[i] = (float *) malloc(in_nb_samples * sizeof(float));
    }
  }
  spb_len = 0;

  osta.samples_count = 0;

  osta.st->time_base = (AVRational) {
    1, c->sample_rate
  };

  fprintf(stderr, "Opened audio stream\n");
  fprintf(stderr, "%d %d - %d %d %d\n", in_nchans, in_sample_rate, c->channels, c->sample_rate, c->sample_fmt);
  return TRUE;
}


static boolean add_stream(OutputStream *ost, AVFormatContext *oc,
                          AVCodec **codec,
                          enum AVCodecID codec_id) {
  AVCodecContext *c;

  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) {
    fprintf(stderr, "Could not find encoder for '%s'\n",
#ifdef HAVE_AVCODEC_GET_NAME
            avcodec_get_name(codec_id)
#else
            ((AVCodec *)(*codec))->name
#endif
           );
    return FALSE;
  }

  c = avcodec_alloc_context3(*codec);
  if (!c) {
    fprintf(stderr, "Could not allocate video / audio codec context\n");
    return FALSE;
  }

  ost->st = avformat_new_stream(oc, *codec); // stream(s) created from format_ctx and codec
  if (!ost->st) {
    fprintf(stderr, "Could not allocate stream\n");
    return FALSE;
  }

  ost->st->codec = ost->enc = c;
  ost->st->id = oc->nb_streams - 1;

  /* Some formats want stream headers to be separate. */
  if (!stream_encode && oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  return TRUE;
}


boolean init_audio(int sample_rate, int nchans, int argc, char **argv) {
  // must be called before init_screen()
  // gets the same argc, argv as init_screen() [created from get_init_rfx() template]
  in_sample_rate = sample_rate;
  in_nchans = nchans;
  return TRUE;
}


boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  AVCodec *codec, *acodec;

  const char *fmtstring;

  AVDictionary *fmt_opts = NULL;
  char uri[PATH_MAX];

  int vcodec_id, acodec_id;
  int ret;

  uri[0] = 0;

  //fprintf(stderr, "init_screen %d x %d %d\n", width, height, argc);

  ostv.frame = osta.frame = NULL;
  vStream = aStream = NULL;

  ostv.sws_ctx = NULL;
  osta.swr_ctx = NULL;

  if (mypalette == WEED_PALETTE_END) {
    fprintf(stderr, "libav stream plugin error: No palette was set !\n");
    return FALSE;
  }
  if (intent == PL_INTENTION_STREAM)
    fmtstring = "flv";
  else
    fmtstring = "mp4";
  vcodec_id = AV_CODEC_ID_H264;
  acodec_id = AV_CODEC_ID_MP3;
  maxvbitrate = 500000;

  if (argc == 1) {
    snprintf(uri, PATH_MAX, "%s", argv[0]);
    snprintf(uri + strlen(argv[0]), PATH_MAX, ".%s", fmtstring);
    argc = 0;
  }

  if (argc > 0) {
    switch (atoi(argv[0])) {
    case 0:
      fmtstring = "mp4";
      vcodec_id = AV_CODEC_ID_H264;
      //acodec_id = AV_CODEC_ID_MP3;
      acodec_id = AV_CODEC_ID_AAC;
      break;
    case 1:
      fmtstring = "ogg";
      vcodec_id = AV_CODEC_ID_THEORA;
      acodec_id = AV_CODEC_ID_VORBIS;
      break;
    case 2:
      fmtstring = "webm";
      vcodec_id = AV_CODEC_ID_VP9;
      acodec_id = AV_CODEC_ID_OPUS;
      break;
    default:
      return FALSE;
    }

    maxvbitrate = atoi(argv[1]);

    switch (intent) {
    case PL_INTENTION_STREAM:
      stream_encode = TRUE;
      snprintf(uri, PATH_MAX, "udp://%s.%s.%s.%s:%s", argv[5], argv[6], argv[7], argv[8], argv[9]);
      break;
    default:
      stream_encode = FALSE;
      snprintf(uri, PATH_MAX, "%s", argv[5]);
      break;
    }
  }

  if (!*uri) {
    fprintf(stderr, "No output location set\n");
    return FALSE;
  }

  ret = avformat_alloc_output_context2(&fmtctx, NULL, fmtstring, uri);
  if (ret < 0) {
    fprintf(stderr, "Could not open fmt '%s': %s\n", fmtstring,
            av_err2str(ret));
  }

  if (!fmtctx) {
    printf("Could not deduce output format from file extension %s: using flv.\n", fmtstring);
    avformat_alloc_output_context2(&fmtctx, NULL, "flv", uri);
  }
  if (!fmtctx) return FALSE;

  // add the video stream
  if (!add_stream(&ostv, fmtctx, &codec, vcodec_id)) {
    avformat_free_context(fmtctx);
    fmtctx = NULL;
    return FALSE;
  }

  vStream = ostv.st;
  ostv.codec = codec;

  ostv.enc = encctx = vStream->codec;

#ifdef API_3_1
  // needs testing
  ret = avcodec_parameters_from_context(vStream->codecpar, encctx);
  if (ret < 0) {
    fprintf(stderr, "avcodec_decoder: avparms from context failed\n");
    return FALSE;
  }
#endif

  // override defaults
  if (fabs(target_fps * 100100. - (double)((int)(target_fps + .5) * 100000)) < 1.) {
    vStream->time_base = (AVRational) {
      1001, (int)(target_fps + .5) * 1000
    };
  } else {
    vStream->time_base = (AVRational) {
      1000, (int)(target_fps + .5) * 1000
    };
  }

  vStream->codec->time_base = vStream->time_base;

  vStream->codec->width = width;
  vStream->codec->height = height;
  vStream->codec->pix_fmt = avpalette;

  // seems not to make a difference
  //vStream->codec->color_trc = AVCOL_TRC_IEC61966_2_1;

  vStream->codec->bit_rate = maxvbitrate;
  // vStream->codec->bit_rate_tolerance = 0;

  if (vcodec_id == AV_CODEC_ID_H264) {
    av_opt_set(encctx->priv_data, "preset", "ultrafast", 0);
    //av_opt_set(encctx->priv_data, "crf", "0", 0);
    av_opt_set(encctx->priv_data, "qscale", "1", 0);
    av_opt_set(encctx->priv_data, "crf", "1", 0);

    if (!argc || !atoi(argv[6])) {
      // lower q, about half the size
      vStream->codec->qmin = 10;
      vStream->codec->qmax = 51;
      av_opt_set(encctx->priv_data, "profile", "main", 0);
    } else {
      // highq mode
      avpalette = weed_palette_to_avi_pix_fmt(WEED_PALETTE_YUV420P, &myclamp);
      av_opt_set(encctx->priv_data, "profile", "high444", 0);
      /* // highest quality - may break compliance */
      /* vStream->codec->me_subpel_quality = 11; */
      /* vStream->codec->trellis = 2; */

      /* // 3 for black enhance */
      /* av_opt_set(encctx->priv_data, "aq-mode", "2", 0); */
    }
  }

  //vStream->codec->gop_size = 10; // maybe only streaming, breaks whatsapp

  if (vcodec_id == AV_CODEC_ID_MPEG2VIDEO) {
    /* just for testing, we also add B frames */
    vStream->codec->max_b_frames = 2;
  }
  if (vcodec_id == AV_CODEC_ID_MPEG1VIDEO) {
    /* Needed to avoid using macroblocks in which some coeffs overflow.
       This does not happen with normal video, it just happens here as
       the motion of the chroma plane does not match the luma plane. */
    vStream->codec->mb_decision = 2;
  }

  //fprintf(stderr, "init_screen2 %d x %d %d\n", width, height, argc);

  /* open video codec */
  if (avcodec_open2(encctx, codec, NULL) < 0) {
    fprintf(stderr, "Could not open codec\n");
    avformat_free_context(fmtctx);
    fmtctx = NULL;
    return FALSE;
  }

  // audio

  if (in_sample_rate > 0) {
    if (!add_stream(&osta, fmtctx, &acodec, acodec_id)) {
      avformat_free_context(fmtctx);
      fmtctx = NULL;
      return FALSE;
    }
    osta.codec = acodec;
    aStream = osta.st;
    osta.enc = aencctx = aStream->codec;

#ifdef API_3_1
    ret = avcodec_parameters_from_context(aStream->codecpar, aencctx);
    if (ret < 0) {
      fprintf(stderr, "avcodec_decoder: avparms from context failed\n");
      avformat_free_context(fmtctx);
      fmtctx = NULL;
      return FALSE;
    }
#endif

    out_nchans = 2;
    out_sample_rate = 44100;
    maxabitrate = 320000;

    if (argc > 0) {
      out_nchans = atoi(argv[2]);
      if (out_nchans) {
        switch (atoi(argv[3])) {
        case 0:
          out_sample_rate = 22050;
          break;
        case 1:
          out_sample_rate = 44100;
          break;
        case 2:
          out_sample_rate = 48000;
          break;
        default:
          break;
        }
        maxabitrate = atoi(argv[4]);

        fprintf(stderr, "added audio stream\n");
        if (!open_audio()) {
          avformat_free_context(fmtctx);
          fmtctx = NULL;
          return FALSE;
        }
      } else {
        fprintf(stderr, "no audio stream selected\n");
      }
    }
  }

  av_dump_format(fmtctx, 0, uri, 1);

  // container

  /* open output file */
  if (!(fmtctx->oformat->flags & AVFMT_NOFILE)) {
    //fprintf(stderr, "opening file %s\n", uri);
    ret = avio_open(&fmtctx->pb, uri, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open '%s': %s\n", uri,
              av_err2str(ret));
      avformat_free_context(fmtctx);
      fmtctx = NULL;
      return FALSE;
    }

    av_dict_set(&fmt_opts, "movflags", "faststart", 0);
    av_dict_set(&fmt_opts, "movflags", "frag_keyframe", 0);
    ret = avformat_write_header(fmtctx, &fmt_opts);
    if (ret < 0) {
      fprintf(stderr, "Error occurred when writing header: %s\n",
              av_err2str(ret));
      avformat_free_context(fmtctx);
      fmtctx = NULL;
      return FALSE;
    }
  }

  /* create (container) libav video frame */
  ostv.frame = alloc_picture(avpalette, width, height);
  if (!ostv.frame) {
    fprintf(stderr, "Could not allocate video frame\n");
    avformat_free_context(fmtctx);
    fmtctx = NULL;
    return FALSE;
  }

  ostv.next_pts = osta.next_pts = 0;
  return TRUE;
}


boolean play_frame(weed_layer_t *frame, int64_t tc, weed_layer_t *ret) {
  // call the function which was set in set_palette
  return play_fn(frame, tc, ret);
}

/*
  static void log_packet(const AVPacket *pkt) {
  AVRational *time_base = &fmtctx->streams[pkt->stream_index]->time_base;
  printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
         av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
         av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
         av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
         pkt->stream_index);
  }
*/


static int write_frame(const AVRational *time_base, AVStream *stream, AVPacket *pkt) {
  int ret;
  /* rescale output packet timestamp values from codec to stream timebase */
  av_packet_rescale_ts(pkt, *time_base, stream->time_base);
  pkt->stream_index = stream->index;
  /* Write the compressed frame to the media file. */
  //log_packet(pkt);
  pthread_mutex_lock(&write_mutex);
  ret = av_interleaved_write_frame(fmtctx, pkt);
  pthread_mutex_unlock(&write_mutex);
  return ret;
}


static void copy_yuv_image(AVFrame *pict, int width, int height, const uint8_t *const *pixel_data) {
  int y, ret;
  int hwidth = width >> 1;
  int hheight = height >> 1;

  /* when we pass a frame to the encoder, it may keep a reference to it
     internally;
     make sure we do not overwrite it here
  */
  ret = av_frame_make_writable(pict);
  if (ret < 0) return;

  /* Y */
  for (y = 0; y < height; y++)
    memcpy(&pict->data[0][y * pict->linesize[0]], &pixel_data[0][y * width], width);
  /* Cb and Cr */
  for (y = 0; y < hheight; y++) {
    memcpy(&pict->data[1][y * pict->linesize[1]], &pixel_data[1][y * hwidth], hwidth);
    memcpy(&pict->data[2][y * pict->linesize[2]], &pixel_data[2][y * hwidth], hwidth);
  }
}


static AVFrame *get_video_frame(const uint8_t *const *pixel_data, int hsize, int vsize, int *rows) {
  AVCodecContext *c = ostv.enc;
  const uint8_t *ipd[4];
  const uint8_t *opd[4];
  static int istrides[4];
  int ostrides[4];
  int avp;

  if (ostv.sws_ctx && (hsize != ohsize || vsize != ovsize)) {
    sws_freeContext(ostv.sws_ctx);
    ostv.sws_ctx = NULL;
  }

  if (hsize != c->width || vsize != c->height || mypalette != avpalette || !ostv.sws_ctx) {
    if (!ostv.sws_ctx) {
      ostv.sws_ctx = sws_getContext(hsize, vsize,
                                    weed_palette_to_avi_pix_fmt(mypalette, &myclamp),
                                    c->width, c->height, avpalette,
                                    SCALE_FLAGS, NULL, NULL, NULL);
      if (!ostv.sws_ctx) {
        fprintf(stderr, "libav_stream: Could not initialize the conversion context\n");
        return NULL;
      }
      ohsize = hsize;
      ovsize = vsize;
      istrides[0] = rows[0];
      if (mypalette == WEED_PALETTE_YUV420P) {
        istrides[1] = rows[1];
        istrides[2] = rows[2];
      } else {
        istrides[1] = istrides[2] = 0;
      }
      istrides[3] = 0;
    }
    ipd[0] = pixel_data[0];
    if (mypalette == WEED_PALETTE_YUV420P) {
      ipd[1] = pixel_data[1];
      ipd[2] = pixel_data[2];
    } else ipd[1] = ipd[2] = NULL;
    ipd[3] = NULL;

    opd[0] = ostv.frame->data[0];
    ostrides[0] = ostv.frame->linesize[0];
    avp = avi_pix_fmt_to_weed_palette(avpalette, NULL);
    if (avp == WEED_PALETTE_YUV420P || avp == WEED_PALETTE_YUV444P) {
      ostrides[1] = ostv.frame->linesize[1];
      ostrides[2] = ostv.frame->linesize[2];
      opd[1] = ostv.frame->data[1];
      opd[2] = ostv.frame->data[2];
    } else {
      ostrides[1] = ostrides[2] = 0;
      opd[1] = opd[2] = NULL;
    }
    ostrides[3] = 0;
    opd[3] = NULL;

    sws_scale(ostv.sws_ctx, (const uint8_t *const *)ipd, istrides,
              0, vsize, (uint8_t *const *)opd, ostrides);
  } else {
    copy_yuv_image(ostv.frame, hsize, vsize, pixel_data);
  }

  ostv.frame->pts = ostv.next_pts++;
  return ostv.frame;
}


boolean render_audio_frame_float(float **audio, int nsamps)  {
  if (!out_nchans) return TRUE;
  else {
    AVCodecContext *c = osta.enc;
    AVPacket pkt = { 0 }; // data and size must be 0;

    float *abuff[in_nchans];

    int ret, i;
    int got_packet = 0;
    int nb_samples;

    av_init_packet(&pkt);

    if (!audio || nsamps == 0) {
      // flush buffers
      ret = avcodec_encode_audio2(c, &pkt, NULL, &got_packet);
      if (ret < 0) {
        fprintf(stderr, "Error 1 encoding audio frame: %s %d %d %d %ld\n", av_err2str(ret),
                nsamps, c->sample_rate, c->sample_fmt, c->channel_layout);
        return FALSE;
      }

      if (got_packet) {
        ret = write_frame(&c->time_base, aStream, &pkt);
        if (ret < 0) {
          fprintf(stderr, "Error while writing audio frame: %s\n", av_err2str(ret));
          return FALSE;
        }
      }
      return TRUE;
    }

    for (i = 0; i < in_nchans; i++) abuff[i] = audio[i];

    while (nsamps > 0) {
      if (out_nb_samples != 0) {
        if (nsamps + spb_len < in_nb_samples) {
          // have l.t. one full buffer to send, store this for next time
          for (i = 0; i < in_nchans; i++) {
            memcpy(&(spill_buffers[i][spb_len]), abuff[i], nsamps * sizeof(float));
          }
          spb_len += nsamps;
          return TRUE;
        }
        if (spb_len > 0) {
          // have data in buffers from last call. fill these up and clear them first
          for (i = 0; i < in_nchans; i++) {
            memcpy(&(spill_buffers[i][spb_len]), audio[i], (in_nb_samples - spb_len) * sizeof(float));
          }
        }
        nb_samples = out_nb_samples;
      } else {
        // codec accepts variable nb_samples, so encode all
        in_nb_samples = nsamps;
        nb_samples = av_rescale_rnd(in_nb_samples,
                                    c->sample_rate, in_sample_rate, AV_ROUND_DOWN);
        osta.frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
      }

      ret = av_frame_make_writable(osta.frame);
      if (ret < 0) return FALSE;

      ret = swr_convert(osta.swr_ctx, osta.frame->data, nb_samples,
                        spb_len == 0 ? (const uint8_t **)abuff
                        : (const uint8_t **)spill_buffers, in_nb_samples);
      if (ret < 0) {
        fprintf(stderr, "Error while converting audio\n");
        return FALSE;
      }

      osta.frame->pts = av_rescale_q(osta.samples_count,
      (AVRational) {1, c->sample_rate}, c->time_base);

      osta.samples_count += nb_samples;

      ret = avcodec_encode_audio2(c, &pkt, osta.frame, &got_packet);

      if (ret < 0) {
        fprintf(stderr, "Error 2 encoding audio frame: %s %d %d %d %d %ld\n", av_err2str(ret), nsamps,
                nb_samples, c->sample_rate, c->sample_fmt, c->channel_layout);
        return FALSE;
      }

      if (got_packet) {
        ret = write_frame(&c->time_base, aStream, &pkt);
        if (ret < 0) {
          fprintf(stderr, "Error 2 while writing audio frame: %s\n", av_err2str(ret));
          return FALSE;
        }
      }

      for (i = 0; i < in_nchans; i++) {
        abuff[i] += in_nb_samples - spb_len;
      }

      nsamps -= in_nb_samples - spb_len;
      spb_len = 0;

      if (out_nb_samples == 0) {
        if (osta.frame) av_frame_unref(osta.frame);
        osta.frame = NULL;
        in_nb_samples = 0;
        return TRUE;
      }
    }
  }
  return TRUE;
}

//#define NEWVER

static boolean play_frame_yuv420(weed_layer_t *frame, int64_t tc, weed_layer_t *ret) {
  AVCodecContext *c;
  AVPacket pkt = { 0 };
  uint8_t **pixel_data = (uint8_t **)weed_channel_get_pixel_data_planar(frame, NULL);
  int hsize = weed_channel_get_width(frame);
  int vsize = weed_channel_get_height(frame);
  int *rows = weed_channel_get_rowstrides(frame, NULL);
  int got_packet = 0;
  int iret;

  c = ostv.enc;

  // copy and scale pixel_data
  if ((ostv.frame = get_video_frame((const uint8_t *const *)pixel_data, hsize, vsize, rows)) != NULL) {
    weed_free(rows);
    weed_free(pixel_data);
    av_init_packet(&pkt);
#ifndef NEWVER
    /* encode the image */
    iret = avcodec_encode_video2(c, &pkt, ostv.frame, &got_packet);

    if (iret < 0) {
      fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(iret));
      return FALSE;
    }
    if (got_packet) {
#if 0
    }
#endif
#else
    iret = avcodec_send_frame(c, ostv.frame);

    if (iret < 0) {
      fprintf(stderr, "Error sending a frame for encoding: %s\n",
              av_err2str(iret));
      return FALSE;
    }
    while (iret >= 0) {
      iret = avcodec_receive_packet(c, &pkt);
      if (iret == AVERROR(EAGAIN) || iret == AVERROR_EOF) {
        return (iret == AVERROR(EAGAIN)) ? TRUE : FALSE;
      } else if (iret < 0) {
        fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(iret));
        return FALSE;
      }
    }
#endif

    if (iret < 0) {
      fprintf(stderr, "Error writing video frame: %s\n", av_err2str(iret));
      return FALSE;
    }

    iret = write_frame(&c->time_base, ostv.st, &pkt);

    if (iret < 0) {
      fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(iret));
      return FALSE;;
    }
#ifndef NEWVER
  }
} else {
  iret = 0;
  weed_free(rows);
  weed_free(pixel_data);
#endif
  av_init_packet(&pkt);
}
av_packet_unref(&pkt);
return ostv.frame ? TRUE : FALSE;
}


void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  AVCodecContext *c;
  AVPacket pkt = { 0 };

  int got_packet = 0;
  int ret;

  int i;

  if (fmtctx) {
    if (!stream_encode && !(fmtctx->oformat->flags & AVFMT_NOFILE)) {
      if (out_nchans && in_sample_rate) {
        // flush final audio
        c = osta.enc;

        do {
          av_init_packet(&pkt);

          ret = avcodec_encode_audio2(c, &pkt, NULL, &got_packet);
          if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame: %s %d %d %d %d %ld\n", av_err2str(ret),
                    0, 0, c->sample_rate, c->sample_fmt, c->channel_layout);
            break;
          }

          if (got_packet) {
            ret = write_frame(&c->time_base, aStream, &pkt);
            if (ret < 0) {
              fprintf(stderr, "Error while writing audio frame: %s\n", av_err2str(ret));
              break;
            }
          }
        } while (got_packet);
      }

      // flush final few frames
      c = ostv.enc;

      do {
        av_init_packet(&pkt);

        ret = avcodec_encode_video2(c, &pkt, NULL, &got_packet);

        if (ret < 0) {
          fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
          break;
        }
        if (got_packet) ret = write_frame(&c->time_base, vStream, &pkt);
        else ret = 0;
        if (ret < 0) break;
      } while (got_packet);
    }

    if (!(fmtctx->oformat->flags & AVFMT_NOFILE))
      /* Write the trailer, if any. The trailer must be written before you
         close the CodecContexts open when you wrote the header; otherwise
         av_write_trailer() may try to use memory that was freed on
         av_codec_close(). */
      av_write_trailer(fmtctx);

    /* Close the output file. */
    avio_closep(&fmtctx->pb);
  }

  if (vStream) {
    avcodec_close(vStream->codec);
    vStream = NULL;
  }

  if (aStream) {
    avcodec_close(aStream->codec);
    aStream = NULL;
  }

  if (fmtctx) {
    avformat_free_context(fmtctx);
    fmtctx = NULL;
  }

  if (ostv.frame) av_frame_unref(ostv.frame);
  if (osta.frame) av_frame_unref(osta.frame);

  if (ostv.sws_ctx) sws_freeContext(ostv.sws_ctx);
  if (osta.swr_ctx) swr_free(&(osta.swr_ctx));

  ostv.sws_ctx = NULL;
  osta.swr_ctx = NULL;

  if (spill_buffers) {
    for (i = 0; i < in_nchans; i++) free(spill_buffers[i]);
    free(spill_buffers);
    spill_buffers = NULL;
  }

  in_sample_rate = 0;
}


void module_unload(void) {
  if (inited) avformat_network_deinit();
  pthread_mutex_destroy(&write_mutex);
  inited = 0;
}

WEED_SETUP_START(200, 200)
WEED_SETUP_END
