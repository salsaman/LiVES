// LiVES - flv decoder plugin
// (c) G. Finch 2011 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "decplugin.h"


///////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

const char *plugin_version="LiVES flv decoder version 0.1";

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include <libavcodec/avcodec.h>

#define INBUF_SIZE 4096000

////////////////////////////////////////////////////

// system header flags
enum {
    FLV_HEADER_FLAG_HASVIDEO = 1,
    FLV_HEADER_FLAG_HASAUDIO = 4,
};

// packet flag masks
#define FLV_AUDIO_CHANNEL_MASK    0x01
#define FLV_AUDIO_SAMPLESIZE_MASK 0x02
#define FLV_AUDIO_SAMPLERATE_MASK 0x0c
#define FLV_AUDIO_CODECID_MASK    0xf0

#define FLV_VIDEO_CODECID_MASK    0x0f
#define FLV_VIDEO_FRAMETYPE_MASK  0xf0


// amf data types for metadata
typedef enum {
    AMF_DATA_TYPE_NUMBER      = 0x00,
    AMF_DATA_TYPE_BOOL        = 0x01,
    AMF_DATA_TYPE_STRING      = 0x02,
    AMF_DATA_TYPE_OBJECT      = 0x03,
    AMF_DATA_TYPE_NULL        = 0x05,
    AMF_DATA_TYPE_UNDEFINED   = 0x06,
    AMF_DATA_TYPE_REFERENCE   = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    AMF_DATA_TYPE_OBJECT_END  = 0x09,
    AMF_DATA_TYPE_ARRAY       = 0x0a,
    AMF_DATA_TYPE_DATE        = 0x0b,
    AMF_DATA_TYPE_LONG_STRING = 0x0c,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFDataType;


// video codecs
enum {
    FLV_CODECID_H263    = 2,
    FLV_CODECID_SCREEN  = 3,
    FLV_CODECID_VP6     = 4,
    FLV_CODECID_VP6A    = 5,
    FLV_CODECID_SCREEN2 = 6,
    FLV_CODECID_H264    = 7,
};


// packet tags
enum {
    FLV_TAG_TYPE_ERROR = 0x00,
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META  = 0x12,
};


// max data read sizes
#define FLV_PROBE_SIZE 12
#define FLV_PACK_HEADER_SIZE 11
#define FLV_META_SIZE 1024


///////////////////////////////////////////////


typedef struct {
  int type;
  int size;
  int64_t dts;
  unsigned char *data;
} lives_flv_pack_t;


typedef struct {
  int fd;
  int pack_offset;
  int64_t input_position;
  int64_t data_start;
  AVCodec *codec;
  AVCodecContext *ctx;
  AVFrame *picture;
  AVPacket avpkt;
  int64_t last_frame;
} lives_flv_priv_t;


////////////////////////////////////////////////////////////////////////////

static int pix_fmt_to_palette(enum PixelFormat pix_fmt, int *clamped) {
 
    switch (pix_fmt) {
    case PIX_FMT_RGB24:
	return WEED_PALETTE_RGB24;
    case PIX_FMT_BGR24:
	return WEED_PALETTE_BGR24;
    case PIX_FMT_RGBA:
	return WEED_PALETTE_RGBA32;
    case PIX_FMT_BGRA:
	return WEED_PALETTE_BGRA32;
    case PIX_FMT_ARGB:
	return WEED_PALETTE_ARGB32;
    case PIX_FMT_YUV444P:
	return WEED_PALETTE_YUV444P;
    case PIX_FMT_YUV422P:
	return WEED_PALETTE_YUV422P;
    case PIX_FMT_YUV420P:
	return WEED_PALETTE_YUV420P;
    case PIX_FMT_YUYV422:
	return WEED_PALETTE_YUYV;
    case PIX_FMT_UYVY422:
	return WEED_PALETTE_UYVY;
    case PIX_FMT_UYYVYY411:
	return WEED_PALETTE_YUV411;
    case PIX_FMT_GRAY8:
    case PIX_FMT_Y400A:
	return WEED_PALETTE_A8;
    case PIX_FMT_MONOWHITE:
    case PIX_FMT_MONOBLACK:
	return WEED_PALETTE_A1;
    case PIX_FMT_YUVJ422P:
	if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
	return WEED_PALETTE_YUV422P;
    case PIX_FMT_YUVJ444P:
	if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
	return WEED_PALETTE_YUV444P;
    case PIX_FMT_YUVJ420P:
	if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
	return WEED_PALETTE_YUV420P;

    default:
	return WEED_PALETTE_END;
    }
}


//////////////////////////////////////////////

/// here we assume that pts of interframes > pts of previous keyframe
// should be true for most formats (except eg. dirac)

static int64_t get_pos_for_pts(const lives_clip_data_t *cdata, int64_t pts) {
  lives_flv_priv_t *priv=cdata->priv;
  return priv->data_start;  // for testing
}





static int get_last_dts(int fd) {
  int tagsize;
  int dts;
  unsigned char data[4];

  lseek(fd,-4,SEEK_END);
  if (read (fd, data, 4) < 4) return -1;
  tagsize=((data[0]&0xFF)<<24)+((data[1]&0xFF)<<16)+((data[2]&0xFF)<<8)+(data[3]&0xFF);

  lseek(fd,-tagsize,SEEK_END);

  if (read (fd, data, 4) < 4) return -1;
  dts=((data[3]&0xFF)<<24)+((data[0]&0xFF)<<16)+((data[1]&0xFF)<<8)+(data[2]&0xFF); // milliseconds

  printf("got vals %d and %d\n",tagsize,dts);

  return dts;
}

//////////////////////////////////////////////

static int amf_get_string(unsigned char *inp, char *buf, size_t size) {
  size_t len=(inp[0]<<8)+inp[1];

  if (len>=size) {
    memset(buf,0,1);
    return -1;
  }

  memcpy(buf,inp+2,len);
  memset(buf+len,0,1);
  return len+2;
}



static double getfloat64(unsigned char *data) {
  int64_t v=(((int64_t)(data[0]&0xFF)<<56)+((int64_t)(data[1]&0xFF)<<48)+((int64_t)(data[2]&0xFF)<<40)+((int64_t)(data[3]&0xFF)<<32)+((int64_t)(data[4]&0xFF)<<24)+((int64_t)(data[5]&0xFF)<<16)+((int64_t)(data[6]&0XFF)<<8)+(int64_t)(data[7]&0xFF));
  return ldexp(((v&((1LL<<52)-1)) + (1LL<<52)) * (v>>63|1), (v>>52&0x7FF)-1075);
}


static boolean lives_flv_parse_pack_header(const lives_clip_data_t *cdata, lives_flv_pack_t *pack) {
  lives_flv_priv_t *priv=cdata->priv;
  char data[FLV_PACK_HEADER_SIZE];

  lseek(priv->fd,priv->input_position,SEEK_SET);

  if (read (priv->fd, data, FLV_PACK_HEADER_SIZE) < FLV_PACK_HEADER_SIZE) {
    fprintf(stderr, "flv_decoder: unable to read packet header for %s\n",cdata->URI);
    return FALSE;
  }

  priv->input_position+=FLV_PACK_HEADER_SIZE;

  pack->type=data[0];

  pack->size=((data[1]&0XFF)<<16)+((data[2]&0XFF)<<8)+(data[3]&0xFF);

  pack->dts=((data[7]&0xFF)<<24)+((data[4]&0xFF)<<16)+((data[5]&0xFF)<<8)+(data[6]&0xFF); // milliseconds


  //skip bytes 8,9,10 - stream id always 0

  //printf("pack size of %d\n",pack->size);

  return TRUE;
}

static void detach_stream (lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_flv_priv_t *priv=cdata->priv;

  cdata->seek_flag=0;

  if (priv->ctx!=NULL) {
    avcodec_close(priv->ctx);
    av_free(priv->ctx);
  }

  if (priv->picture!=NULL) av_free(priv->picture);

  priv->ctx=NULL;
  priv->codec=NULL;
  priv->picture=NULL;

  if (cdata->palettes!=NULL) free(cdata->palettes);

  close(priv->fd);
}



static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get a handle
  lives_flv_priv_t *priv=cdata->priv;
  lives_flv_pack_t *pack;
  char header[FLV_PROBE_SIZE];
  char buffer[FLV_META_SIZE];
  unsigned char flags;
  int type,size,vcodec,ldts;
  boolean gotmeta=FALSE,in_array=FALSE;
  boolean haskeyframes=FALSE,canseekend=FALSE,hasaudio;
  boolean got_astream=FALSE,got_vstream=FALSE;
  char *key=NULL;
  size_t offs=0;
  double num_val,fps,lasttimestamp=-1.;

  uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];

  AVCodec *codec=NULL;
  AVCodecContext *ctx;

  boolean got_picture;

#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"\n");
#endif

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "flv_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

  if (read (priv->fd, header, FLV_PROBE_SIZE) < FLV_PROBE_SIZE) {
    fprintf(stderr, "flv_decoder: unable to read header for %s\n",cdata->URI);
    close(priv->fd);
    return FALSE;
  }

  if (header[0] != 'F' || header[1] != 'L' || header[2] != 'V' || header[3] >= 5 || header[5]!=0) {
    close(priv->fd);
    return FALSE;
  }

  flags=header[4];

  if (!(flags&FLV_HEADER_FLAG_HASVIDEO)) {
    close(priv->fd);
    return FALSE;
  }

  hasaudio=flags&FLV_HEADER_FLAG_HASAUDIO;

  priv->input_position=4+((header[5]&0xFF)<<24)+((header[6]&0xFF)<<16)+((header[7]&0xFF)<<8)+((header[8]&0xFF));

  cdata->fps=0.;

  pack=(lives_flv_pack_t *)malloc(sizeof(lives_flv_pack_t));

  if (!lives_flv_parse_pack_header(cdata,pack)) {
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  if (pack->type!=FLV_TAG_TYPE_META) {
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  // first packet should be metadata

  if (pack->size<19) {
    fprintf(stderr, "flv_decoder: invalid metadata for %s\n",cdata->URI);
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  pack->data=malloc(pack->size);
  if (read (priv->fd, pack->data, pack->size) < pack->size) {
    fprintf(stderr, "flv_decoder: error in metadata for %s\n",cdata->URI);
    free(pack->data);
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  priv->input_position+=pack->size+4; // 4 bytes for backwards size

  while (offs<pack->size-2) {
    if (in_array&&key==NULL) type=AMF_DATA_TYPE_STRING;
    else {
      type=(int)(pack->data[offs]&0xFF);
      offs++;
    }

    switch (type) {
    case AMF_DATA_TYPE_NULL:
    case AMF_DATA_TYPE_UNDEFINED:
    case AMF_DATA_TYPE_UNSUPPORTED:
      break; //these take up no additional space


    case AMF_DATA_TYPE_STRING:
    case AMF_DATA_TYPE_LONG_STRING:
    case AMF_DATA_TYPE_OBJECT:
      size=amf_get_string(pack->data+offs, buffer, FLV_META_SIZE);
      if (size<=0) size=10000000;
      offs+=size;
      if (!gotmeta) {
	if (!strcmp(buffer, "onMetaData")) {
	  gotmeta=TRUE;
	}
      }

      // deal with string
      if (key!=NULL) {
	free(key);
	key=NULL;
#ifdef DEBUG
	fprintf(stderr,"%s\n",buffer);
#endif

	// read eoo
	if (!in_array) offs++;
      }
      else {
	key=strdup(buffer);
#ifdef DEBUG
	fprintf(stderr,"%s:",key);
#endif
      }
      break;

    case AMF_DATA_TYPE_MIXEDARRAY:
#ifdef DEBUG
      fprintf(stderr,"mixed");
#endif
      in_array=TRUE;
    case AMF_DATA_TYPE_ARRAY:
#ifdef DEBUG
      fprintf(stderr,"array\n");
#endif
      offs+=4; // max array elem
      if (key!=NULL) free(key);
      key=NULL;
      //eoo++;
      break;

    case AMF_DATA_TYPE_NUMBER:
      num_val = getfloat64(&pack->data[offs]);
#ifdef DEBUG
      fprintf(stderr,"%f\n",num_val);
#endif

      offs+=8;

      if (!strcmp(key,"framerate")) cdata->fps=num_val;
      if (!strcmp(key,"audiosamplerate")) cdata->arate=num_val;
      if (!strcmp(key,"audiosamplesize")) cdata->asamps=num_val;
      if (!strcmp(key,"lasttimestamp")) lasttimestamp=num_val;
      if (!strcmp(key,"height")) cdata->height=num_val;
      if (!strcmp(key,"width")) cdata->width=num_val;

      if (key!=NULL) free(key);
      key=NULL;
      break;
      
    case AMF_DATA_TYPE_BOOL:
      num_val = (int)((pack->data[offs]&0xFF));
#ifdef DEBUG
      fprintf(stderr,"%s\n",((int)num_val==1)?"true":"false");
#endif

      offs+=1;

      if (!strcmp(key,"hasKeyframes")&&num_val==1.) haskeyframes=TRUE;
      else if (!strcmp(key,"canSeekToEnd")&&num_val==1.) canseekend=TRUE;
      else if (!strcmp(key,"hasAudio")&&num_val==0.) hasaudio=FALSE;
      if (!strcmp(key,"stereo")&&num_val==1.) cdata->achans=2;

      if (key!=NULL) free(key);
      key=NULL;

      break;

    case AMF_DATA_TYPE_DATE:
      offs+=10;
      if (key!=NULL) free(key);
      key=NULL;
#ifdef DEBUG
      fprintf(stderr,"\n");
#endif
      break;
      
    default:
      if (key!=NULL) free(key);
      key=NULL;
      break;

    }

  }

  free(pack->data);

  if (!gotmeta) {
    fprintf(stderr, "flv_decoder: no metadata found for %s\n",cdata->URI);
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  if (!hasaudio) {
    cdata->arate=0;
    cdata->achans=0;
    cdata->asamps=16;
  }

  if (!haskeyframes) {
    // cant seek, so not good
    fprintf(stderr, "flv_decoder: non-seekable file %s\n",cdata->URI);
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;

  cdata->offs_x=0;
  cdata->offs_y=0;

  priv->data_start=priv->input_position;

  if (!hasaudio) got_astream=TRUE;

  // now we get the stream data
  while (!got_astream || !got_vstream) {
    do {
      if (!lives_flv_parse_pack_header(cdata,pack)) {
	free(pack);
	close(priv->fd);
	return FALSE;
      }
      if (pack->size==0) priv->input_position+=4; // backwards size
    } while (pack->size==0);
    
  
    pack->data=malloc(8);  // we only need at most 8 bytes here
    if (read (priv->fd, pack->data, 8) < 8) {
      fprintf(stderr, "flv_decoder: error in stream header for %s\n",cdata->URI);
      free(pack->data);
      free(pack);
      close(priv->fd);
      return FALSE;
    }

    priv->input_position+=pack->size+4;

    // audio just for info
    if (pack->type==FLV_TAG_TYPE_AUDIO) {
      flags=pack->data[0];
      sprintf(cdata->audio_name,"%s","pcm"); // TODO
      got_astream=TRUE;
    }

    // only care about video here

    if (pack->type==FLV_TAG_TYPE_VIDEO) {
      flags=pack->data[0];
      priv->data_start=priv->input_position-pack->size-4-11;

      if ((flags & 0xf0) == 0x50) { // video info / command frame
	free(pack->data);
	continue;
      }

      if (got_vstream) {
	fprintf(stderr,"flv_decoder: got duplicate video stream in %s\n",cdata->URI);
	free(pack->data);
	free(pack);
	close(priv->fd);
	return FALSE;
      }
      got_vstream=TRUE;

      vcodec = flags & FLV_VIDEO_CODECID_MASK;


      // let avcodec do some of the work now

       priv->pack_offset=0;

      switch (vcodec) {
      case FLV_CODECID_H263  : sprintf(cdata->video_name,"%s","flv1");
	codec = avcodec_find_decoder(CODEC_ID_FLV1);
	priv->pack_offset=1;
	break;
      case FLV_CODECID_SCREEN: sprintf(cdata->video_name,"%s","flashsv"); 
	codec = avcodec_find_decoder(CODEC_ID_FLASHSV);
	priv->pack_offset=1; // *** needs checking
	break;
      case FLV_CODECID_SCREEN2: sprintf(cdata->video_name,"%s","flashsv2"); 
	codec = avcodec_find_decoder(CODEC_ID_FLASHSV2);
	priv->pack_offset=1; // *** needs checking
	break;
      case FLV_CODECID_VP6   : sprintf(cdata->video_name,"%s","vp6f"); 
	cdata->offs_x=(pack->data[1]&0X0F)>>1; // divide by 2 for offset
	cdata->offs_y=(pack->data[1]&0XF0)>>5; // divide by 2 for offset
	if (cdata->width==0) cdata->width=pack->data[7]*16-cdata->offs_x*2;
	if (cdata->height==0) cdata->height=pack->data[6]*16-cdata->offs_y*2;
	codec = avcodec_find_decoder(CODEC_ID_VP6F);
	priv->pack_offset=2;
	break;
      case FLV_CODECID_VP6A  :
	sprintf(cdata->video_name,"%s","vp6a");
	cdata->offs_x=(pack->data[1]&0X0F)>>1; // divide by 2 for offset
	cdata->offs_y=(pack->data[1]&0XF0)>>5; // divide by 2 for offset
	if (cdata->width==0) cdata->width=pack->data[7]*16-cdata->offs_x*2;
	if (cdata->height==0) cdata->height=pack->data[6]*16-cdata->offs_y*2;
	codec = avcodec_find_decoder(CODEC_ID_VP6A);
	priv->pack_offset=2; /// *** needs checking
	break;
      case FLV_CODECID_H264:
	// broken....
	sprintf(cdata->video_name,"%s","h264");
	//codec = avcodec_find_decoder(CODEC_ID_H264);
	priv->pack_offset=0;  /// *** 
	break;
      default:
	fprintf(stderr,"flv_decoder: unknown video stream type (%d) in %s\n",vcodec,cdata->URI);
	memset(cdata->video_name,0,1);
	break;
      }

    }

    free(pack->data);

  }

#ifdef DEBUG
  fprintf(stderr,"video type is %s %d x %d (%d x %d +%d +%d)\n",cdata->video_name,
	  cdata->width,cdata->height,cdata->frame_width,cdata->frame_height,cdata->offs_x,cdata->offs_y);
#endif


  if (!codec) {
    if (strlen(cdata->video_name)>0) 
      fprintf(stderr, "flv_decoder: Could not find avcodec codec for video type %s\n",cdata->video_name);
    free(pack);
    detach_stream(cdata);
    return FALSE;
  }

  ctx = avcodec_alloc_context();
  if (avcodec_open(ctx, codec) < 0) {
    fprintf(stderr, "flv_decoder: Could not open avcodec context\n");
    free(pack);
    detach_stream(cdata);
    return FALSE;
  }

  // re-scan with avcodec; priv->data_start holds video data start position
  priv->input_position=priv->data_start;

  lives_flv_parse_pack_header(cdata,pack);

  if (priv->pack_offset!=0) lseek(priv->fd,priv->pack_offset,SEEK_CUR);

  memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

  av_init_packet(&priv->avpkt);
  priv->avpkt.size=read (priv->fd, inbuf, INBUF_SIZE);
  priv->avpkt.data = inbuf;

  priv->picture = avcodec_alloc_frame();

#if LIBAVCODEC_VERSION_MAJOR >= 53
  avcodec_decode_video2(ctx, priv->picture, &got_picture, &priv->avpkt );
#else 
  avcodec_decode_video(ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size );
#endif

  free(pack);

  if (!got_picture) {
    fprintf(stderr,"flv_decoder: could not get picture.\n");
    detach_stream(cdata);
    return FALSE;
  }

  priv->input_position+=pack->size+4;

  cdata->YUV_clamping=WEED_YUV_CLAMPING_UNCLAMPED;
  if (ctx->color_range==AVCOL_RANGE_MPEG) cdata->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;

  cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT;
  if (ctx->chroma_sample_location!=AVCHROMA_LOC_LEFT) cdata->YUV_sampling=WEED_YUV_SAMPLING_MPEG;

  cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
  if (ctx->colorspace==AVCOL_SPC_BT709) cdata->YUV_subspace=WEED_YUV_SUBSPACE_BT709;

  cdata->palettes=(int *)malloc(2*sizeof(int));

  cdata->palettes[0]=pix_fmt_to_palette(ctx->pix_fmt,&cdata->YUV_clamping);
  cdata->palettes[1]=WEED_PALETTE_END;

  if (cdata->palettes[0]==WEED_PALETTE_END) {
    fprintf(stderr, "flv_decoder: Could not find a usable palette for %s\n",cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  cdata->current_palette=cdata->palettes[0];

  // re-get fps, width, height, nframes - actually avcodec is pretty useless at getting this
  // so we fall back on the values we obtained ourselves

  if (cdata->width==0) cdata->width=ctx->width-cdata->offs_x*2;
  if (cdata->height==0) cdata->height=ctx->height-cdata->offs_y*2;
  

#ifdef DEBUG
  fprintf(stderr,"using palette %d, size %d x %d\n",
	  cdata->current_palette,cdata->width,cdata->height);
#endif

  cdata->par=(double)ctx->sample_aspect_ratio.num/(double)ctx->sample_aspect_ratio.den;
  if (cdata->par==0.) cdata->par=1.;

  if (ctx->time_base.den>0&&ctx->time_base.num>0) {
    fps=(double)ctx->time_base.den/(double)ctx->time_base.num;
    if (fps!=1000.) cdata->fps=fps;
  }

  if (cdata->fps==0.||cdata->fps==1000.) {
    fprintf(stderr, "flv_decoder: invalid framerate %.4f (%d / %d)\n",cdata->fps,ctx->time_base.den,ctx->time_base.num);
    detach_stream(cdata);
    return FALSE;
  }
  
  if (ctx->ticks_per_frame==2) {
    // TODO - needs checking
    cdata->fps/=2.;
    cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;
  }

  priv->ctx=ctx;
  priv->codec=codec;

  priv->last_frame=-1;

  if (cdata->width*cdata->height==0) return FALSE;

  ldts=get_last_dts(priv->fd);

  if (ldts==-1) {
    fprintf(stderr, "flv_decoder: could not read last dts\n");
    detach_stream(cdata);
    return FALSE;
  }

  cdata->nframes=(int64_t)((double)ldts/1000.*cdata->fps+1.5);

#ifdef DEBUG
  fprintf(stderr,"fps is %.4f %ld\n",cdata->fps,cdata->nframes);
#endif


  return TRUE;
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  avcodec_init();
  avcodec_register_all();
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata (void) {
  lives_flv_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->URI=NULL;
  
  cdata->priv=priv=malloc(sizeof(lives_flv_priv_t));

  cdata->seek_flag=0;

  priv->ctx=NULL;
  priv->codec=NULL;
  priv->picture=NULL;

  cdata->palettes=NULL;
  
  return cdata;
}






lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different current_clip, this must be called again with the same
  // cdata as the second parameter

  // value returned should be freed with clip_data_free() when no longer required

  // should be thread-safe



  if (cdata!=NULL&&cdata->current_clip>0) {
    // currently we only support one clip per container

    clip_data_free(cdata);
    return NULL;
  }

  if (cdata==NULL) {
    cdata=init_cdata();
  }

  if (cdata->URI==NULL||strcmp(URI,cdata->URI)) {
    if (cdata->URI!=NULL) {
      detach_stream(cdata);
      free(cdata->URI);
    }
    cdata->URI=strdup(URI);
    if (!attach_stream(cdata)) {
      free(cdata->URI);
      cdata->URI=NULL;
      clip_data_free(cdata);
      return NULL;
    }
    cdata->current_palette=cdata->palettes[0];
    cdata->current_clip=0;
  }

  cdata->nclips=1;

  ///////////////////////////////////////////////////////////

  sprintf(cdata->container_name,"%s","flv");

  // cdata->height was set when we attached the stream

  cdata->interlace=LIVES_INTERLACE_NONE;

  cdata->frame_width=cdata->width+cdata->offs_x*2;
  cdata->frame_height=cdata->height+cdata->offs_y*2;

  ////////////////////////////////////////////////////////////////////

  cdata->asigned=TRUE;
  cdata->ainterleaf=TRUE;

  return cdata;
}




boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, void **pixel_data) {
  // seek to frame,

  int64_t target_pts=(double)tframe*cdata->fps*1000.;
  int64_t pos;
  lives_flv_priv_t *priv=cdata->priv;
  lives_flv_pack_t *pack;
  int height=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->frame_width;
  boolean got_picture=FALSE;
  unsigned char *dst,*src;
  register int i,p;


  if (tframe!=priv->last_frame) {

    if ((pos=get_pos_for_pts(cdata,target_pts)>-1)) priv->input_position=pos;

    // if frame is not in index:
    // jump to highest kframe before target (a), or lowest after target (b)
    // (b) - parse backwards until we find kframe <= target, noting kframes as we go
    // (a) - parse forwards until we get to pts, noting kframes; jump back to target's kframe



    // we are now at the kframe before or at target - parse packets until we hit target

    avcodec_flush_buffers (priv->ctx);

    pack=(lives_flv_pack_t *)malloc(sizeof(lives_flv_pack_t));


    // do this until we reach target frame //////////////

    // skip_idct and skip_frame. ???

    priv->input_position=priv->data_start;

    if (!lives_flv_parse_pack_header(cdata,pack)) {
      free(pack);
      return FALSE;
    }

    pack->data=malloc(pack->size-priv->pack_offset);

    if (priv->pack_offset!=0) lseek(priv->fd,priv->pack_offset,SEEK_CUR);

    priv->avpkt.size=read (priv->fd, pack->data, pack->size-priv->pack_offset);
    priv->avpkt.data = pack->data;

    priv->input_position+=pack->size-priv->pack_offset+4;

    priv->ctx->width=dstwidth;
    priv->ctx->height=height;

    while (!got_picture) {
      int len;
#if LIBAVCODEC_VERSION_MAJOR >= 52
      len=avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt );
#else 
      len=avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size );
#endif
      priv->avpkt.size-=len;
      priv->avpkt.data+=len;

      if (!got_picture&&priv->avpkt.size<=0) return FALSE;

    }

    free(pack->data);

    /////////////////////////////////////////////////////

    free(pack);

  }
  
  if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P) nplanes=3;
  else if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) dstwidth*=3;
  else if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32) dstwidth*=4;

  for (p=0;p<nplanes;p++) {
      dst=pixel_data[p];
      src=priv->picture->data[p];

      for (i=0;i<height;i++) {
	  memcpy(dst,src,dstwidth);

	  dst+=dstwidth;
	  src+=priv->picture->linesize[p];
      }
      if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P)) dstwidth>>=1;
      if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P)) height>>=1;
  }
  

  priv->last_frame=tframe;

  return TRUE;
}




void clip_data_free(lives_clip_data_t *cdata) {

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  free(cdata->priv);
  free(cdata);
}


void module_unload(void) {

}

int main(void) {
  // for testing
  module_check_init();
  get_clip_data("vid.flv",NULL);
  return 1;
}


