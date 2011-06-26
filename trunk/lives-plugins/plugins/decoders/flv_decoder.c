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

#define INBUF_SIZE 4096

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
  char *data;
} lives_flv_pack_t;


typedef struct {
  int fd;
  int64_t input_position;
  int64_t data_start;
} lives_flv_priv_t;


static int lives_palettes[2];

//////////////////////////////////////////////

static int amf_get_string(char *inp, char *buf, size_t size) {
  size_t len=(inp[0]<<8)+inp[1];

  if (len>size) {
    memset(buf,0,1);
    return -1;
  }

  memcpy(buf,inp+2,len);
  memset(buf+len,0,1);
  return len+2;
}



static double getfloat64(char *data) {
  int64_t v=(((int64_t)(data[0]&0xFF)<<56)+((int64_t)(data[1]&0xFF)<<48)+((int64_t)(data[2]&0xFF)<<40)+((int64_t)(data[3]&0xFF)<<32)+((int64_t)(data[4]&0xFF)<<24)+((int64_t)(data[5]&0xFF)<<16)+((int64_t)(data[6]&0XFF)<<8)+(int64_t)(data[7]&0xFF));
  return ldexp(((v&((1LL<<52)-1)) + (1LL<<52)) * (v>>63|1), (v>>52&0x7FF)-1075);
}


static boolean lives_flv_parse_pack_header(lives_clip_data_t *cdata, lives_flv_pack_t *pack) {
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

  pack->dts=((data[4]&0xFF)<<24)+((data[5]&0xFF)<<16)+((data[6]&0xFF)<<8)+(data[7]&0xFF); // milliseconds


  //skip bytes 8,9,10 - stream id always 0

  //printf("pack size of %d\n",pack->size);

  return TRUE;
}


static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get a handle
  lives_flv_priv_t *priv=cdata->priv;
  lives_flv_pack_t *pack;
  char header[FLV_PROBE_SIZE];
  char buffer[FLV_META_SIZE];
  unsigned char flags;
  int type,size,vcodec;
  boolean gotmeta=FALSE,in_array=FALSE;
  boolean haskeyframes=FALSE,canseekend=FALSE,hasaudio;
  boolean got_astream=FALSE,got_vstream=FALSE;
  char *key=NULL;
  size_t offs=0;
  double num_val,lasttimestamp;

  AVCodec *codec;
  AVCodecContext *c= NULL;
  AVFrame *picture;
  uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
  AVPacket avpkt;

  //avcodec palettes
  enum PixelFormat pforms[8],palette;

  boolean got_picture;


  // TODO - printf to #debug
  printf("\n");

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

  //  fstat(priv->fd,&sb);
  // if (sb.st_size) cdata->nframes = (int)(sb.st_size / priv->frame_size);

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
	printf("%s\n",buffer);
	// read eoo
	if (!in_array) offs++;
      }
      else {
	key=strdup(buffer);
	printf("%s:",key);
      }


      break;

    case AMF_DATA_TYPE_MIXEDARRAY:
      printf("mixed");
      in_array=TRUE;
    case AMF_DATA_TYPE_ARRAY:
      printf("array\n");
      offs+=4; // max array elem
      if (key!=NULL) free(key);
      key=NULL;
      //eoo++;
      break;

    case AMF_DATA_TYPE_NUMBER:
      num_val = getfloat64(&pack->data[offs]);
      printf("%f\n",num_val);

      offs+=8;

      if (!strcmp(key,"framerate")) cdata->fps=num_val;
      if (!strcmp(key,"audiosamplerate")) cdata->arate=num_val;
      if (!strcmp(key,"audiosamplesize")) cdata->asamps=num_val;
      if (!strcmp(key,"lastimestamp")) lasttimestamp=num_val;
      if (!strcmp(key,"height")) cdata->height=num_val;
      if (!strcmp(key,"width")) cdata->width=num_val;

      if (key!=NULL) free(key);
      key=NULL;

      break;
      
    case AMF_DATA_TYPE_BOOL:
      num_val = (int)((pack->data[offs]&0xFF));
      printf("%d\n",(int)num_val);

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
      printf("\n");
      break;
      
    default:
      if (key!=NULL) free(key);
      key=NULL;
      break;

    }

  }

  free(pack->data);

  if (!gotmeta) {
    fprintf(stderr, "flv_decoder: no metadata for %s\n",cdata->URI);
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  if (!hasaudio) {
    cdata->arate=0;
    cdata->achans=0;
    cdata->asamps=16;
  }

  cdata->nframes=(int)(lasttimestamp*cdata->fps+1.5);

  if (!haskeyframes) {
    // cant seek, so not good
    fprintf(stderr, "flv_decoder: non-seekable file (%d,%d) %s\n",canseekend,haskeyframes,cdata->URI);
    free(pack);
    close(priv->fd);
    return FALSE;
  }

  cdata->seek_flag=LIVES_SEEK_FAST;

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
    
  
    pack->data=malloc(pack->size);  // actually we only need at most 2 bytes here
    if (read (priv->fd, pack->data, pack->size) < pack->size) {
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

      if ((flags & 0xf0) == 0x50) { // video info / command frame
	free(pack->data);
	continue;
      }

      if (got_vstream) printf("flv_decoder: got duplicate video stream in %s\n",cdata->URI);
      got_vstream=TRUE;

      vcodec = flags & FLV_VIDEO_CODECID_MASK;


      // let avcodec do some of the work now

      av_init_packet(&avpkt);
      memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);


      switch (vcodec) {
      case FLV_CODECID_H263  : sprintf(cdata->video_name,"%s","flv1");
	codec = avcodec_find_decoder(CODEC_ID_FLV1);
	break;
      case FLV_CODECID_SCREEN: sprintf(cdata->video_name,"%s","flashsv"); 
	codec = avcodec_find_decoder(CODEC_ID_FLASHSV);
	break;
      case FLV_CODECID_SCREEN2: sprintf(cdata->video_name,"%s","flashsv2"); 
	codec = avcodec_find_decoder(CODEC_ID_FLASHSV2);
	break;
      case FLV_CODECID_VP6   : sprintf(cdata->video_name,"%s","vp6f"); 
	cdata->offs_x=(pack->data[1]&0X0F)>>1; // divide by 2 for offset
	cdata->offs_y=(pack->data[1]&0XF0)>>5; // divide by 2 for offset
	cdata->width=pack->data[6]*16-cdata->offs_x*2;
	cdata->height=pack->data[7]*16-cdata->offs_y*2;
	codec = avcodec_find_decoder(CODEC_ID_VP6F);
	break;
      case FLV_CODECID_VP6A  :
	sprintf(cdata->video_name,"%s","vp6a");
	cdata->offs_x=(pack->data[1]&0X0F)>>1; // divide by 2 for offset
	cdata->offs_y=(pack->data[1]&0XF0)>>5; // divide by 2 for offset
	cdata->width=pack->data[6]*16-cdata->offs_x*2;
	cdata->height=pack->data[7]*16-cdata->offs_y*2;
	codec = avcodec_find_decoder(CODEC_ID_VP6A);
	break;
      case FLV_CODECID_H264:
	sprintf(cdata->video_name,"%s","h264");
	codec = avcodec_find_decoder(CODEC_ID_H264);
	break;
      default:
	printf("flv_decoder: got unknown video stream type (%d) in %s\n",vcodec,cdata->URI);
	break;
      }

      // packet data follows
      printf("next bytes %02X %02X %02X %02X %02X %02X %02X\n",pack->data[1],pack->data[2],pack->data[3],pack->data[4],pack->data[5],pack->data[6],pack->data[7]);

    }

    free(pack->data);

  }

  printf("video type is %s %d x %d\n",cdata->video_name,cdata->width,cdata->height);

  free(pack);

  if (!codec) {
    fprintf(stderr, "flv_decoder: Could not open avcodec codec\n");
    close(priv->fd);
    return FALSE;
  }

  // TODO *** - get width, height, par, interlace and palette(s)
  c= avcodec_alloc_context();
  if (avcodec_open(c, codec) < 0) {
    fprintf(stderr, "flv_decoder: Could not open avcodec context\n");
    close(priv->fd);
    return FALSE;
  }

  // re-scan with avcodec; priv->data_start holds data start
  lseek(priv->fd,0,SEEK_SET);
  
  avpkt.size=read (priv->fd, inbuf, INBUF_SIZE);
  avpkt.data = inbuf;

  avcodec_decode_video2(c, picture, &got_picture, &avpkt);

  pforms[0]=PIX_FMT_YUV444P;  // I can has 444 plz ?
  pforms[1]=PIX_FMT_RGBA;  
  pforms[2]=PIX_FMT_BGRA;  
  pforms[3]=PIX_FMT_RGB24;  
  pforms[4]=PIX_FMT_BGR24;  
  pforms[5]=PIX_FMT_YUV422P;
  pforms[6]=PIX_FMT_YUV420P;
  pforms[7]=PIX_FMT_NONE;

  palette=c->get_format(c,pforms);

  cdata->palettes=lives_palettes;
  cdata->palettes[1]=WEED_PALETTE_END;

  switch(palette) {
  case PIX_FMT_YUV444P:
    cdata->palettes[0]=WEED_PALETTE_YUV444P;
    break;

  case PIX_FMT_YUV422P:
    cdata->palettes[0]=WEED_PALETTE_YUV422P;
    break;

  case PIX_FMT_YUV420P:
    cdata->palettes[0]=WEED_PALETTE_YUV420P;
    break;

  case PIX_FMT_RGB24:
    cdata->palettes[0]=WEED_PALETTE_RGB24;
    break;

  case PIX_FMT_BGR24:
    cdata->palettes[0]=WEED_PALETTE_BGR24;
    break;

  case PIX_FMT_RGBA:
    cdata->palettes[0]=WEED_PALETTE_RGBA32;
    break;

  case PIX_FMT_BGRA:
    cdata->palettes[0]=WEED_PALETTE_BGRA32;
    break;

  default:
    fprintf(stderr, "flv_decoder: Could not open avcodec context\n",cdata->URI);
    close(priv->fd);
    return FALSE;
    break;
  }

  cdata->current_palette=cdata->palettes[0];

  printf("negotiated palette %d\n",cdata->current_palette);

  // TODO - re-get fps, width, height, nframes




  return TRUE;
}



static void detach_stream (lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_flv_priv_t *priv=cdata->priv;
  close(priv->fd);
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata (void) {
  register int i;
  lives_flv_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));
  
  av_register_all() ;

  cdata->URI=NULL;
  
  cdata->priv=priv=malloc(sizeof(lives_flv_priv_t));

  cdata->seek_flag=0;
  
  return cdata;
}






lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different URI or a different current_clip, this must be called again with the same
  // cdata as the second parameter

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

  cdata->par=1.;

  cdata->frame_width=cdata->width+cdata->offs_x*2;
  cdata->frame_height=cdata->height+cdata->offs_y*2;

  ////////////////////////////////////////////////////////////////////

  cdata->asigned=TRUE;
  cdata->ainterleaf=TRUE;

  return cdata;
}


void main(void) {
  lives_clip_data_t *cdata=init_cdata();

  get_clip_data("/home/gabriel/Pictures/vid.mpg",cdata);

}

/*


boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, void **pixel_data) {
  // seek to frame,

  // if frame is not in index:
  // jump to highest kframe before target (a), or lowest after target (b)
  // (b) - parse backwards until we find kframe <= target, noting kframes as we go
  // (a) - parse forwards until we get to dts, noting kframes; jump back to target's kframe

  // we are now at the kframe before or at target - parse packets until we hit target

  //  return pixel_data

  return TRUE;
}

*/


void clip_data_free(lives_clip_data_t *cdata) {

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  free(cdata->priv);

  free(cdata->palettes);
  free(cdata);
}


void module_unload(void) {

}
