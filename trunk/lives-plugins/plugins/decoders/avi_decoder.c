// LiVES - avi decoder plugin
// (c) G. Finch 2011 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// partly based on ffmpeg avi decoder by Fabrice Bellard and others


// BROKEN and needs fixing by so,ebody who knows avi / ffmpeg



#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/stat.h>

const char *plugin_version="LiVES avi decoder version 0.1";

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>

#include "decplugin.h"
#include "avi_decoder.h"

#define ER_EXPLODE 5 // set to lower to crap out on small errors

#define print_tag(str, tag, size)		\
  printf("%s: tag=%c%c%c%c size=0x%x\n",	\
	 str, tag & 0xff,			\
	 (tag >> 8) & 0xff,			\
	 (tag >> 16) & 0xff,			\
	 (tag >> 24) & 0xff,			\
	 size)

static const char avi_headers[][8] = {
  { 'R', 'I', 'F', 'F',    'A', 'V', 'I', ' ' },
  { 'R', 'I', 'F', 'F',    'A', 'V', 'I', 'X' },
  { 'O', 'N', '2', ' ',    'O', 'N', '2', 'f' },
  { 'R', 'I', 'F', 'F',    'A', 'M', 'V', ' ' },
  { 0 }
};

static enum CodecID ff_codec_get_id(const AVCodecTag *tags, unsigned int tag) {
  int i;
  for (i=0; tags[i].id != CODEC_ID_NONE; i++) {
    if (tag == tags[i].tag)
      return tags[i].id;
  }
  for (i=0; tags[i].id != CODEC_ID_NONE; i++) {
    if (toupper((tag >> 0)&0xFF) == toupper((tags[i].tag >> 0)&0xFF)
        && toupper((tag >> 8)&0xFF) == toupper((tags[i].tag >> 8)&0xFF)
        && toupper((tag >>16)&0xFF) == toupper((tags[i].tag >>16)&0xFF)
        && toupper((tag >>24)&0xFF) == toupper((tags[i].tag >>24)&0xFF))
      return tags[i].id;
  }
  return CODEC_ID_NONE;
}

static inline int get_duration(AVIStream *ast, int len) {
  if (ast->sample_size) {
    return len;
  } else if (ast->dshow_block_align) {
    return (len + ast->dshow_block_align - 1)/ast->dshow_block_align;
  } else
    return 1;
}

static void freep(void **x) {
  if (*x!=NULL) free(*x);
}

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



/* can enable this later to handle pix_fmt (35) - YUVA4204P */
/*
  static void convert_quad_chroma(guchar *src, int width, int height, guchar *dest) {
  // width and height here are width and height of dest chroma planes, in bytes

  // double the chroma samples vertically and horizontally, with interpolation, eg. 420p to 444p

  // output to planes

  register int i,j;
  guchar *d_u=dest,*s_u=src;
  gboolean chroma=FALSE;
  int height2=height;
  int width2=width;

  height>>=1;
  width>>=1;

  // for this algorithm, we assume chroma samples are aligned like mpeg

  for (i=0;i<height2;i++) {
  d_u[0]=d_u[1]=s_u[0];
  for (j=2;j<width2;j+=2) {
  d_u[j+1]=d_u[j]=s_u[(j>>1)];
  d_u[j-1]=avg_chroma(d_u[j-1],d_u[j]);
  if (!chroma&&i>0) {
  // pass 2
  // average two src rows (e.g 2 with 1, 4 with 3, ... etc) for odd dst rows
  // thus dst row 1 becomes average of src chroma rows 0 and 1, etc.)
  d_u[j-width2]=avg_chroma(d_u[j-width2],d_u[j]);
  d_u[j-1-width2]=avg_chroma(d_u[j-1-width2],d_u[j-1]);
  }
  }
  if (!chroma&&i>0) {
  d_u[j-1-width2]=avg_chroma(d_u[j-1-width2],d_u[j-1]);
  }
  if (chroma) {
  s_u+=width;
  }
  chroma=!chroma;
  d_u+=width2;
  }
  }

*/

//////////////////////////////////////////////


static int frame_to_dts(const lives_clip_data_t *cdata, int64_t frame) {
  lives_avi_priv_t *priv=cdata->priv;
  return (int)((double)(frame)*1000./cdata->fps)+priv->start_dts;
}

static int64_t dts_to_frame(const lives_clip_data_t *cdata, int dts) {
  lives_avi_priv_t *priv=cdata->priv;
  return (int64_t)((double)(dts-priv->start_dts)/1000.*cdata->fps+.5);
}


////////////////////////////////////////////////////////////////

static boolean check_eof(const lives_clip_data_t *cdata) {
  lives_avi_priv_t *priv=cdata->priv;
  if (priv->input_position>=priv->filesize) return TRUE;
  return FALSE;
}



/////////////////////////////////////////////////////

static void index_free(index_entry *idx) {
  index_entry *cidx=idx,*next;

  while (cidx!=NULL) {
    next=cidx->next;
    free(cidx);
    cidx=next;
  }
}


static index_entry *idx_alloc(int64_t offs, int frag, int dts) {
  index_entry *nidx=(index_entry *)malloc(sizeof(index_entry));
  nidx->offs=offs;
  nidx->dts=dts;
  nidx->frag=frag;
  return nidx;
}




/// here we assume that pts of interframes > pts of previous keyframe
// should be true for most formats (except eg. dirac)

// we further assume that pts == dts for all frames

static index_entry *add_keyframe(const lives_clip_data_t *cdata, int64_t offs, int frag, int dts) {
  // dts 0 is start (i.e excluding start_dts)
  lives_avi_priv_t *priv=cdata->priv;
  index_entry *idx,*lidx,*nidx;

  lidx=idx=priv->idx;

  while (idx!=NULL) {
    if (idx->dts==dts) return idx; // already indexed
    if (idx->dts>dts) {
      // insert before idx
      nidx=idx_alloc(offs,frag,dts);
      nidx->next=idx;
      if (idx==priv->idx) priv->idx=nidx;
      else lidx->next=nidx;
      return nidx;
    }
    lidx=idx;
    idx=idx->next;
  }

  // insert at tail

  nidx=idx_alloc(offs,frag,dts);

  if (lidx!=NULL) lidx->next=nidx;
  else priv->idx=nidx;

  nidx->next=NULL;

  return nidx;
}


static int get_stream_idx(int *d) {
  if (d[0] >= '0' && d[0] <= '9'
      && d[1] >= '0' && d[1] <= '9') {
    return (d[0] - '0') * 10 + (d[1] - '0');
  } else {
    return 100; //invalid stream ID
  }
}


static boolean get_sync(lives_avi_priv_t *priv, int exit_early) {
  AVFormatContext *s=priv->s;
  AVIContext *avi = s->priv_data;
  int n, d[8];
  unsigned int size;
  int64_t i, sync;
  boolean eof=FALSE;
  unsigned char buffer[1];

start_sync:
  lseek(priv->fd,priv->input_position,SEEK_SET);
  memset(d, -1, sizeof(int)*8);

  for (i=sync=priv->input_position; !eof; i++) {
    int j;

    for (j=0; j<7; j++)
      d[j]= d[j+1];

    if (read(priv->fd,buffer,1)<1) {
      printf("eof1 %d %x %d %d\n",priv->fd,priv->input_position,errno,avi->fsize);
      eof=1;
      break;
    }

    priv->input_position++;

    d[7]=*buffer;

    size= d[4] + (d[5]<<8) + (d[6]<<16) + (d[7]<<24);

    n= get_stream_idx(d+2);

    //printf("got %d and %d\n",size,n);

    if (i + (uint64_t)size > avi->fsize || d[0]<0)
      continue;

    //printf("pt a2\n");

    //parse ix##
    if ((d[0] == 'i' && d[1] == 'x' && n < s->nb_streams)
        //parse JUNK
        ||(d[0] == 'J' && d[1] == 'U' && d[2] == 'N' && d[3] == 'K')
        ||(d[0] == 'i' && d[1] == 'd' && d[2] == 'x' && d[3] == '1')) {
      priv->input_position+=size-8;
      //av_log(s, AV_LOG_DEBUG, "SKIP\n");
      goto start_sync;
    }

    //printf("pt a23\n");
    //parse stray LIST
    if (d[0] == 'L' && d[1] == 'I' && d[2] == 'S' && d[3] == 'T') {
      priv->input_position+=4;
      goto start_sync;
    }

    n= get_stream_idx(d);
    //printf("pt a2xx %d\n",n);

    if (!((i-avi->last_pkt_pos)&1) && get_stream_idx(d+1) < s->nb_streams)
      continue;
    //printf("pt a2xxzzzz %d\n",n);

    if (n!=priv->st->id) {
      priv->input_position+=size-8;
      goto start_sync;
    }
    //printf("pt a2xxaaaa %d\n",n);

    //detect ##ix chunk and skip
    if (d[2] == 'i' && d[3] == 'x' && n < s->nb_streams) {
      priv->input_position+=size-8;
      goto start_sync;
    }
    printf("pt a24\n");

    //parse ##dc/##wb
    if (n < s->nb_streams) {
      AVStream *st;
      AVIStream *ast;
      st = s->streams[n];
      ast = st->priv_data;

      if (s->nb_streams>=2) {
        AVStream *st1  = s->streams[1];
        AVIStream *ast1= st1->priv_data;
        //workaround for broken small-file-bug402.avi
        if (d[2] == 'w' && d[3] == 'b'
            && n==0
            && st ->codec->codec_type == AVMEDIA_TYPE_VIDEO
            && st1->codec->codec_type == AVMEDIA_TYPE_AUDIO
            && ast->prefix == 'd'*256+'c'
            && (d[2]*256+d[3] == ast1->prefix || !ast1->prefix_count)
           ) {
          n=1;
          st = st1;
          ast = ast1;
          fprintf(stderr, "av_decoder: Invalid stream + prefix combination, assuming audio.\n");
          priv->input_position+=size-8;
          goto start_sync;
        }
      }

      //printf("pt a25\n");

      if ((st->discard >= AVDISCARD_DEFAULT && size==0)
          /*|| (st->discard >= AVDISCARD_NONKEY && !(pkt->flags & AV_PKT_FLAG_KEY))*/ //FIXME needs a little reordering
          || st->discard >= AVDISCARD_ALL) {
        if (!exit_early) {
          ast->frame_offset += get_duration(ast, size);
        }
        goto start_sync;
      }
      //printf("pt a26\n");

      if (d[2] == 'p' && d[3] == 'c' && size<=4*256+4) {
        int k;
        int last;
        uint8_t buffer[4];

        if (read(priv->fd, buffer, 4)<4) return -4;

        priv->input_position+=4;

        k    = buffer[0];
        last = (k + buffer[1] - 1) & 0xFF;

        for (; k <= last; k++) {

          if (read(priv->fd,buffer,4)<4) {
            fprintf(stderr, "avi_decoder: read error getting value\n");
            return FALSE;
          }

          priv->input_position+=4;

          ast->pal[k] = get_le32int(buffer)>>8; // b + (g << 8) + (r << 16);
          size-=4;
        }
        ast->has_pal= 1;
        priv->input_position+=size-8;
        goto start_sync;
      } else if (((ast->prefix_count<5 || sync+9 > i) && d[2]<128 && d[3]<128) ||
                 d[2]*256+d[3] == ast->prefix /*||
						  (d[2] == 'd' && d[3] == 'c') ||
						  (d[2] == 'w' && d[3] == 'b')*/) {
        //printf("pt a27\n");

        if (exit_early)
          return TRUE;
        //av_log(s, AV_LOG_DEBUG, "OK\n");
        if (d[2]*256+d[3] == ast->prefix)
          ast->prefix_count++;
        else {
          ast->prefix= d[2]*256+d[3];
          ast->prefix_count= 0;
        }

        avi->stream_index= n;
        ast->packet_size= size + 8;
        ast->remaining= size;

        /*	if(size || !ast->sample_size){
        	uint64_t pos= avio_tell(pb) - 8;
        	if(!st->index_entries || !st->nb_index_entries || st->index_entries[st->nb_index_entries - 1].pos < pos){
        	av_add_index_entry(st, pos, ast->frame_offset, size, 0, AVINDEX_KEYFRAME);
        	}
        	}*/
        return TRUE;
      }
    }
  }

  if (eof) printf("EOF !!\n");

  return FALSE;

}




static int get_next_video_packet(const lives_clip_data_t *cdata, int tfrag, int64_t tdts) {
  lives_avi_priv_t *priv=cdata->priv;
  uint32_t size;
  AVFormatContext *s=priv->s;
  AVIContext *avi=priv->avi;
  AVStream *vidst=priv->st;
  AVIStream *ast = vidst->priv_data;

  int res;

  // create avpacket

  // if (target dts>=0):
  // seek to input_pos
  // a:
  // sync to hdr start

  // seek to offs

  // b)

  // do until avpacket is full:

  // if no more data in packet, goto a)

  // read from packet until remaining is 0

  // if (targetdts==-1):
  // create avpacket
  // set packet offs to 0
  // goto b)

  // if we have target (tdts>0) we parse only, until we reach a packet/offset with dts>=tdts

  //priv->offs=0;

  while (1) {
#define DEBUG
#ifdef DEBUG
    printf("pos is %x %d\n",priv->input_position,ast->packet_size);
#endif

    res=get_sync(priv,0);
    printf("2 pos is %x, val=%d %d\n",priv->input_position,res,priv->fd);

    if (!res) return -2;

    //    priv->input_position+=8;
    //lseek(priv->fd,priv->input_position,SEEK_SET);

    if (tdts==-1&&priv->offs==0) {
      priv->avpkt.size=1000000+FF_INPUT_BUFFER_PADDING_SIZE;
      priv->avpkt.data=malloc(priv->avpkt.size);
      memset(priv->avpkt.data,0,priv->avpkt.size);
      //priv->offs=0;
      priv->avpkt.size=0;
    }


resync:

    if (ast->sample_size <= 1) // minorityreport.AVI block_align=1024 sample_size=1 IMA-ADPCM
      size= INT_MAX;
    else if (ast->sample_size < 32)
      // arbitrary multiplier to avoid tiny packets for raw PCM data
      size= 1024*ast->sample_size;
    else
      size= ast->sample_size;

    // TODO ***
    //err= av_get_packet(pb, pkt, size);

    if (ast->has_pal && priv->avpkt.data && priv->avpkt.size<(unsigned)INT_MAX/2) {

      // TODO ***
      /*
      uint8_t *pal;
      pal = av_packet_new_side_data(&priv->avpkt, AV_PKT_DATA_PALETTE, AVPALETTE_SIZE);
      if(!pal){
      frpintf(stderr, "av_decoder: Failed to allocate data for palette\n");
      }else{
      memcpy(pal, ast->pal, AVPALETTE_SIZE);
      ast->has_pal = 0;
      }*/



    }
    if (size>ast->remaining) size=ast->remaining;

    priv->offs+=read(priv->fd, priv->avpkt.data, size);

    priv->input_position+=size;
    priv->avpkt.size+=size;

    // TODO ***
    //assert(st->index_entries);

    /* XXX: How to handle B-frames in AVI? */
    priv->avpkt.dts = ast->frame_offset;
    //                priv->avpkt->dts += ast->start;
    if (ast->sample_size)
      priv->avpkt.dts /= ast->sample_size;
    //av_log(s, AV_LOG_DEBUG, "dts:%"PRId64" offset:%"PRId64" %d/%d smpl_siz:%d base:%d st:%d size:%d\n", pkt->dts, ast->frame_offset, ast->scale, ast->rate, ast->sample_size, AV_TIME_BASE, avi->stream_index, size);
    priv->avpkt.stream_index = avi->stream_index;

    if (priv->st->codec->codec_id == CODEC_ID_MPEG4) {
      int key=1;
      int i;
      uint32_t state=-1;
      for (i=0; i<FFMIN(size,256); i++) {
        if (state == 0x1B6) {
          key= !(priv->avpkt.data[i]&0xC0);

          // TODO ***
          //if (key) add_index();

          break;
        }
        state= (state<<8) + priv->avpkt.data[i];
      }
    }
    ast->frame_offset += get_duration(ast, priv->avpkt.size);

    printf("rem %d, size %d %d\n",ast->remaining,size,ast->packet_size);
    ast->remaining -= size;

    if (ast->remaining<=0) {
      return 0;
      //avi->stream_index= -1;
      //ast->packet_size= 0;
    }

    /*    if(!avi->non_interleaved && priv->avpkt.pos >= 0 && ast->seek_pos > priv->avpkt.pos){
      av_packet_unref(&priv->avpkt);
      goto resync;
      }*/

    ast->seek_pos= 0;

    // TODO *** - check nb_index_entries, check prefix
    if (!avi->non_interleaved && priv->st->nb_index_entries>1) {
      int64_t dts= av_rescale_q(priv->avpkt.dts, priv->st->time_base, AV_TIME_BASE_Q);

      if (avi->dts_max - dts > 2*AV_TIME_BASE) {
        avi->non_interleaved= 1;
        av_log(s, AV_LOG_INFO, "Switching to NI mode, due to poor interleaving\n");
      } else if (avi->dts_max < dts)
        avi->dts_max = dts;
    }

  }

  // will never reach here
  return 0;
}


static index_entry *get_idx_for_pts(const lives_clip_data_t *cdata, int64_t pts) {
  lives_avi_priv_t *priv=cdata->priv;
  int64_t tdts=pts-priv->start_dts;
  index_entry *idx=priv->idx,*lidx=idx;
  int ret;

  while (idx!=NULL) {
    if (idx->dts>tdts) return lidx;
    lidx=idx;
    idx=idx->next;
  }
  priv->kframe=lidx;
  priv->input_position=lidx->offs;
  do {
    ret=get_next_video_packet(cdata, lidx->frag, tdts);
  } while (ret<0&&ret!=-2);
  if (ret!=-2) return priv->kframe;
  return NULL;
}




static int get_last_video_dts(const lives_clip_data_t *cdata) {
  // get last vido frame dts (relative to start dts)
  lives_avi_priv_t *priv=cdata->priv;

  priv->kframe=get_idx_for_pts(cdata,frame_to_dts(cdata,0)-priv->start_dts);
  // this will parse through the file, adding keyframes, until we reach EOF

  priv->input_position=priv->kframe->offs;

  // TODO ***
  //avi_reset_header(priv->s);

  get_next_video_packet(cdata,priv->kframe->frag,INT_MAX);

  // priv->kframe will hold last kframe in the clip
  return priv->frame_dts-priv->start_dts;
}


//////////////////////////////////////////////


static void detach_stream(lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_avi_priv_t *priv=cdata->priv;

  cdata->seek_flag=0;

  if (priv->avpkt.data!=NULL) free(priv->avpkt.data);

  if (priv->ctx!=NULL) {
    avcodec_close(priv->ctx);
    av_free(priv->ctx);
  }

  if (priv->picture!=NULL) av_frame_unref(priv->picture);

  priv->ctx=NULL;
  priv->picture=NULL;

  if (priv->idx!=NULL)
    index_free(priv->idx);

  priv->idx=NULL;

  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;

  free(priv->avi);

  av_free(priv->s);
  //avformat_free_context(priv->s);

  if (priv->st!=NULL) {
    if (priv->st->codec->extradata_size!=0) free(priv->st->codec->extradata);
  }

  if (priv->avi_st!=NULL) free(priv->avi_st);

  close(priv->fd);
}

static boolean check_header(char *buf) {
  int i;
  for (i=0; avi_headers[i][0]; i++)
    if (!memcmp(buf  , avi_headers[i]  , 4) &&
        !memcmp(buf+8, avi_headers[i]+4, 4))
      return TRUE;
  return FALSE;
}

static int get_bmp_header(lives_clip_data_t *cdata) {
  lives_avi_priv_t *priv=cdata->priv;
  int tag1;
  unsigned char buffer[4];

  if (read(priv->fd,buffer,4)<4) {
    return -1;
  }

  priv->input_position+=4;

  if (read(priv->fd,buffer,4)<4) {
    return -1;
  }

  priv->input_position+=4;

  cdata->width=get_le32int(buffer);

  if (read(priv->fd,buffer,4)<4) {
    return -1;
  }

  priv->input_position+=4;

  cdata->height=get_le32int(buffer);

  if (read(priv->fd,buffer,2)<2) {
    return -1;
  }

  priv->input_position+=2;

  if (read(priv->fd,buffer,2)<2) {
    return -1;
  }

  priv->input_position+=2;

  priv->st->codec->bits_per_coded_sample = get_le16int(buffer);

  if (read(priv->fd,buffer,4)<4) {
    return -1;
  }

  priv->input_position+=4;

  tag1=get_le32int(buffer);

  priv->input_position+=20;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  return tag1;
}

static boolean avi_read_info(lives_clip_data_t *cdata, uint64_t end) {
  uint32_t tag,size;
  unsigned char buffer[4];
  lives_avi_priv_t *priv=cdata->priv;

  while (priv->input_position < end) {
    if (read(priv->fd,buffer,4)<4) {
      return;
    }

    priv->input_position+=4;
    tag=get_le32int(buffer);

    if (read(priv->fd,buffer,4)<4) {
      return;
    }

    priv->input_position+=4;
    size=get_le32int(buffer);

    size += (size & 1);

    if (size == UINT_MAX)
      return;

    priv->input_position+=size;
    lseek(priv->fd, priv->input_position, SEEK_SET);

  }

}


static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get metadata
  lives_avi_priv_t *priv=cdata->priv;
  char header[16];
  unsigned char buffer[4096];

  int i,len,retval;
  int size;
  int codec_type, stream_index, frame_period;
  int amv_file_format=0;
  int ni;
  int avih_width=0, avih_height=0;

  unsigned int tag, tag1, handler;

  boolean got_picture=FALSE;

  double fps;

  AVCodec *codec=NULL;
  AVCodecContext *ctx;
  AVStream *vidst=NULL;
  AVIStream *ast = NULL;

  uint64_t list_end = 0;

  int64_t pts=AV_NOPTS_VALUE;

  int error_recognition=2;

  AVRational dar[128];

  struct stat sb;

#ifdef DEBUG
  fprintf(stderr,"\n");
#endif

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "avi_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

  if (read(priv->fd, header, AVI_PROBE_SIZE) < AVI_PROBE_SIZE) {
    // for example, might be a directory
#ifdef DEBUG
    fprintf(stderr, "avi_decoder: unable to read header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }

  priv->input_position=AVI_PROBE_SIZE;

  // test header
  if (!check_header(header)) {
#ifdef DEBUG
    fprintf(stderr, "avi_decoder: not avi header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }


  stream_index = -1;
  codec_type = -1;
  frame_period = 0;

  cdata->par=1.;
  cdata->fps=0.;
  cdata->width=cdata->frame_width=cdata->height=cdata->frame_height=0;
  cdata->offs_x=cdata->offs_y=0;

  cdata->arate=0;
  cdata->achans=0;
  cdata->asamps=0;
  snprintf(cdata->audio_name,16,"none");

  priv->idx=NULL;

  cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;

  cdata->offs_x=0;
  cdata->offs_y=0;

  priv->avi=(AVIContext *)malloc(sizeof(AVIContext));
  //  memset(&priv->avi->aviid2avid, -1, sizeof(priv->avi->aviid2avid));

  priv->s = avformat_alloc_context();
  priv->s->priv_data=priv->avi;
  av_init_packet(&priv->avpkt);
  priv->avpkt.data=NULL;
  priv->st=NULL;
  priv->avi_st=NULL;
  priv->ctx=NULL;

  for (i=0; i<128; i++) dar[i].num=dar[i].den=0;

  fstat(priv->fd,&sb);
  priv->avi->fsize=priv->filesize=sb.st_size;

  for (;;) {

    if (read(priv->fd,buffer,4)<4) {
      fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
      detach_stream(cdata);
      return FALSE;
    }

    priv->input_position+=4;

    tag = get_le32int(buffer);


    if (read(priv->fd,buffer,4)<4) {
      fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
      detach_stream(cdata);
      return FALSE;
    }

    priv->input_position+=4;

    size = get_le32int(buffer);

    print_tag("tag", tag, size);


    switch (tag) {
    case MKTAG('L', 'I', 'S', 'T'):
      list_end = priv->input_position + size;
      /* Ignored, except at start of video packets. */

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      tag1 = get_le32int(buffer);

      print_tag("list", tag1, 0);

      if (tag1 == MKTAG('m', 'o', 'v', 'i')) {
        priv->avi->movi_list = priv->input_position - 4;
        if (size) priv->avi->movi_end = priv->avi->movi_list + size + (size & 1);
        else priv->avi->movi_end=priv->filesize;
        goto end_of_header;
      } else if (tag1 == MKTAG('I', 'N', 'F', 'O')) {
        avi_read_info(cdata, list_end);
        if (check_eof(cdata)) {
          fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }
      } else if (tag1 == MKTAG('n', 'c', 'd', 't')) {
        fprintf(stderr, "avi_decoder: detected dv stream in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }
      break;

    case MKTAG('I', 'D', 'I', 'T'): {
      size += (size & 1);
      size=FFMIN(size, 63);
      priv->input_position+=size;
      lseek(priv->fd,priv->input_position,SEEK_SET);

      break;
    }

    case MKTAG('d', 'm', 'l', 'h'):
      priv->avi->is_odml = 1;
      priv->input_position += size + (size & 1);
      lseek(priv->fd,priv->input_position,SEEK_SET);
      break;

    case MKTAG('a', 'm', 'v', 'h'):
      amv_file_format=1;
    case MKTAG('a', 'v', 'i', 'h'):
      /* AVI header */
      /* using frame_period is bad idea */

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      frame_period = get_le32int(buffer);

      priv->input_position+=8;
      lseek(priv->fd,priv->input_position,SEEK_SET);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      ni=get_le32int(buffer);


      priv->avi->non_interleaved |= ni & AVIF_MUSTUSEINDEX;

      priv->input_position+=16;
      lseek(priv->fd,priv->input_position,SEEK_SET);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      avih_width=get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      avih_height=get_le32int(buffer);

      priv->input_position += size - 10 * 4;
      lseek(priv->fd,priv->input_position,SEEK_SET);
      break;

    case MKTAG('s', 't', 'r', 'h'):
      /* stream header */

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      tag1=get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      handler=get_le32int(buffer);


      if (tag1 == MKTAG('p', 'a', 'd', 's')) {
        priv->input_position += size - 8;
        lseek(priv->fd,priv->input_position,SEEK_SET);
        break;
      } else {
        stream_index++;
        priv->st = av_new_stream(priv->s, stream_index);
        if (!priv->st)
          goto fail;

        ast = malloc(sizeof(AVIStream));
        if (!ast)
          goto fail;
        memset(ast,0,sizeof(AVIStream));
        priv->st->priv_data = ast;
      }
      if (amv_file_format)
        tag1 = stream_index ? MKTAG('a','u','d','s') : MKTAG('v','i','d','s');

      print_tag("strh", tag1, -1);


      if (tag1 == MKTAG('i', 'a', 'v', 's') || tag1 == MKTAG('i', 'v', 'a', 's')) {

        ast = priv->s->streams[0]->priv_data;
        freep(&priv->s->streams[0]->codec->extradata);
        freep(&priv->s->streams[0]->codec);
        freep(&priv->s->streams[0]);

        fprintf(stderr, "avi_decoder: detected dv stream in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      // TODO ***
      //assert(stream_index < s->nb_streams);

      priv->st->codec->stream_codec_tag= handler;

      priv->input_position+=12;
      lseek(priv->fd,priv->input_position,SEEK_SET);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      ast->scale=get_le32int(buffer);


      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      ast->rate=get_le32int(buffer);


      if (!(ast->scale && ast->rate)) {
        fprintf(stderr, "avi_decoder: scale/rate is %u/%u which is invalid. (This file has been generated by broken software.)\n", ast->scale,
                ast->rate);
        if (frame_period) {
          ast->rate = 1000000;
          ast->scale = frame_period;
        } else {
          ast->rate = 25;
          ast->scale = 1;
        }
      }

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      ast->cum_len=get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      priv->st->nb_frames=get_le32int(buffer);

      priv->st->start_time = 0;

      priv->input_position+=8;
      lseek(priv->fd,priv->input_position,SEEK_SET);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      ast->sample_size=get_le32int(buffer);

      ast->cum_len *= FFMAX(1, ast->sample_size);

      switch (tag1) {
      case MKTAG('v', 'i', 'd', 's'):
        codec_type = AVMEDIA_TYPE_VIDEO;

        ast->sample_size = 0;
        break;
      case MKTAG('a', 'u', 'd', 's'):
        codec_type = AVMEDIA_TYPE_AUDIO;
        break;
      case MKTAG('t', 'x', 't', 's'):
        codec_type = AVMEDIA_TYPE_SUBTITLE;
        break;
      default:
        codec_type = AVMEDIA_TYPE_DATA;
      }

      if (ast->sample_size == 0)
        priv->st->duration = priv->st->nb_frames;
      ast->frame_offset= ast->cum_len;

      priv->input_position+=size - 12 * 4;

      lseek(priv->fd,priv->input_position,SEEK_SET);
      break;


    case MKTAG('s', 't', 'r', 'f'):
      /* stream header */
      if (stream_index >= (unsigned)priv->s->nb_streams) {
        priv->input_position+= size;
        lseek(priv->fd,priv->input_position,SEEK_SET);
      } else {
        uint64_t cur_pos = priv->input_position;
        if (cur_pos < list_end)
          size = FFMIN(size, list_end - cur_pos);
        priv->st = priv->s->streams[stream_index];
        switch (codec_type) {
        case AVMEDIA_TYPE_VIDEO:
          fprintf(stderr,"pt 1\n");
          if (amv_file_format) {
            fprintf(stderr,"pt 12\n");
            priv->st->codec->width=avih_width;
            priv->st->codec->height=avih_height;
            priv->st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
            priv->st->codec->codec_id = CODEC_ID_AMV;
            priv->input_position+=size;
            lseek(priv->fd,priv->input_position,SEEK_SET);
            break;
          }

          tag1 = get_bmp_header(cdata);
          print_tag("xvideo", tag1, 0);

          if (check_eof(cdata)) {
            fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
            detach_stream(cdata);
            return FALSE;
          }

          if (tag1 == MKTAG('D', 'X', 'S', 'B') || tag1 == MKTAG('D','X','S','A')) {
            priv->st->codec->codec_type = AVMEDIA_TYPE_SUBTITLE;
            priv->st->codec->codec_tag = tag1;
            priv->st->codec->codec_id = CODEC_ID_XSUB;
            break;
          }

          if (size > 10*4 && size<(1<<30)) {
            priv->st->codec->extradata_size= size - 10*4;
            priv->st->codec->extradata= malloc(priv->st->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
            if (!priv->st->codec->extradata) {
              priv->st->codec->extradata_size= 0;
              return AVERROR(ENOMEM);
            }

            if (read(priv->fd,priv->st->codec->extradata, priv->st->codec->extradata_size) < priv->st->codec->extradata_size) {
              fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
              detach_stream(cdata);
              return FALSE;
            }
          }

          if (priv->st->codec->extradata_size & 1) {
            //FIXME check if the encoder really did this correctly
            priv->input_position+=1;
            lseek(priv->fd,priv->input_position,SEEK_SET);
          }
          /* Extract palette from extradata if bpp <= 8. */
          /* This code assumes that extradata contains only palette. */
          /* This is true for all paletted codecs implemented in FFmpeg. */
          if (priv->st->codec->extradata_size && (priv->st->codec->bits_per_coded_sample <= 8)) {
            int pal_size = (1 << priv->st->codec->bits_per_coded_sample) << 2;
            const uint8_t *pal_src;

            pal_size = FFMIN(pal_size, priv->st->codec->extradata_size);
            pal_src = priv->st->codec->extradata + priv->st->codec->extradata_size - pal_size;
#if HAVE_BIGENDIAN
            for (i = 0; i < pal_size/4; i++)
              ast->pal[i] = get_le32int((uint8_t *)(((uint32_t *)(pal_src+4*i))));
#else
            memcpy(ast->pal, pal_src, pal_size);
#endif
            ast->has_pal = 1;
          }

          print_tag("video", tag1, 0);

          vidst=priv->st;

          priv->st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
          priv->st->codec->codec_tag = tag1;
          priv->st->codec->codec_id = ff_codec_get_id((const AVCodecTag *)(codec_bmp_tags), tag1);

          // This is needed to get the pict type which is necessary for generating correct pts.
          priv->st->need_parsing = AVSTREAM_PARSE_HEADERS;
          // Support "Resolution 1:1" for Avid AVI Codec
          if (tag1 == MKTAG('A', 'V', 'R', 'n') &&
              priv->st->codec->extradata_size >= 31 &&
              !memcmp(&priv->st->codec->extradata[28], "1:1", 3))
            priv->st->codec->codec_id = CODEC_ID_RAWVIDEO;

          if (priv->st->codec->codec_tag==0 && priv->st->codec->height > 0 && priv->st->codec->extradata_size < 1U<<30) {
            priv->st->codec->extradata_size+= 9;
            priv->st->codec->extradata= realloc(priv->st->codec->extradata, priv->st->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
            if (priv->st->codec->extradata)
              memcpy(priv->st->codec->extradata + priv->st->codec->extradata_size - 9, "BottomUp", 9);
          }
          priv->st->codec->height= FFABS(priv->st->codec->height);

          //                    avio_skip(pb, size - 5 * 4);
          break;

        case AVMEDIA_TYPE_AUDIO:
          priv->input_position+=size;
          lseek(priv->fd,priv->input_position,SEEK_SET);
          break;
        case AVMEDIA_TYPE_SUBTITLE:
          priv->input_position+=size;
          lseek(priv->fd,priv->input_position,SEEK_SET);
          priv->st->codec->codec_id= CODEC_ID_NONE;
          priv->st->codec->codec_tag= 0;
          //priv->st->codec->codec_type = AVMEDIA_TYPE_SUBTITLE;
          //priv->st->request_probe= 1;
          break;
        default:
          priv->st->codec->codec_type = AVMEDIA_TYPE_DATA;
          priv->st->codec->codec_id= CODEC_ID_NONE;
          priv->st->codec->codec_tag= 0;

          priv->input_position+=size;
          lseek(priv->fd,priv->input_position,SEEK_SET);
          break;
        }
      }
      break;

    case MKTAG('i', 'n', 'd', 'x'):
      // TODO ***
      //if(read_braindead_odml_indx(priv->s, 0) < 0 && error_recognition >= ER_EXPLODE)
      //goto fail;
      priv->input_position+=size;
      lseek(priv->fd,priv->input_position,SEEK_SET);
      break;

    case MKTAG('v', 'p', 'r', 'p'):
      if (stream_index < (unsigned)priv->s->nb_streams && size > 9*4) {
        AVRational active, active_aspect;

        priv->st = priv->s->streams[stream_index];

        priv->input_position+=20;
        lseek(priv->fd,priv->input_position,SEEK_SET);

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        active_aspect.den=get_le16int(buffer);

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        active_aspect.num=get_le16int(buffer);

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;

        active.num=get_le32int(buffer);

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "avi_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;

        active.den=get_le32int(buffer);



        priv->input_position+=4; //fieldsperframe
        lseek(priv->fd,priv->input_position,SEEK_SET);

        if (active_aspect.num && active_aspect.den && active.num && active.den) {
          priv->st->sample_aspect_ratio= av_div_q(active_aspect, active);
          //av_log(s, AV_LOG_ERROR, "vprp %d/%d %d/%d\n", active_aspect.num, active_aspect.den, active.num, active.den);
        }
        size -= 9*4;
      }

      priv->input_position+=size;
      lseek(priv->fd,priv->input_position,SEEK_SET);
      break;

    case MKTAG('s', 't', 'r', 'n'):
      if (priv->s->nb_streams) {
        priv->input_position+=size;
        lseek(priv->fd,priv->input_position,SEEK_SET);
        break;
      }
    default:
      if (size > 1000000) {
        fprintf(stderr, "avi_decoder: Something went wrong during header parsing, "
                "I will ignore it and try to continue anyway.\n");
        if (error_recognition >= ER_EXPLODE) goto fail;
        priv->avi->movi_list = priv->input_position - 4;
        priv->avi->movi_end  = priv->filesize;
        goto end_of_header;
      }
      /* skip tag */
      size += (size & 1);
      priv->input_position+=size;
      lseek(priv->fd,priv->input_position,SEEK_SET);
      break;
    }
  }
end_of_header:
  /* check stream number */
  if (stream_index != priv->s->nb_streams - 1) {
fail:
    return FALSE;
  }

  // TODO ***
  //if(!priv->avi->index_loaded)
  //avi_load_index(priv->s);

  priv->avi->index_loaded = 1;
  priv->avi->non_interleaved=1;//|= guess_ni_flag(priv->s); //| (priv->s->flags & AVFMT_FLAG_SORT_DTS);

  // TODO *** - crap out if we have no keyframes

  if (priv->avi->non_interleaved) {
    fprintf(stderr, "av_decoder: non-interleaved AVI\n");
    //clean_index(priv->s);
  }

  //ff_metadata_conv_ctx(s, NULL, ff_avi_metadata_conv);


  ////////////////////////////////////////////////////////

  cdata->fps=(double)ast->rate/(double)ast->scale;

  priv->data_start=priv->input_position;

  // re-scan with avcodec; priv->data_start holds video data start position
  priv->input_position=priv->data_start;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  switch (vidst->codec->codec_id) {
  case CODEC_ID_WMV1:
    snprintf(cdata->video_name,16,"wmv1");
    break;
  case CODEC_ID_WMV2:
    snprintf(cdata->video_name,16,"wmv2");
    break;
  case CODEC_ID_WMV3:
    snprintf(cdata->video_name,16,"wmv3");
    break;
  case CODEC_ID_DVVIDEO:
    snprintf(cdata->video_name,16,"dv");
    break;
  case CODEC_ID_MPEG4:
    snprintf(cdata->video_name,16,"mpeg4");
    break;
  case CODEC_ID_H264:
    snprintf(cdata->video_name,16,"h264");
    break;
  case CODEC_ID_MPEG1VIDEO:
    snprintf(cdata->video_name,16,"mpeg1");
    break;
  case CODEC_ID_MPEG2VIDEO:
    snprintf(cdata->video_name,16,"mpeg2");
    break;
  default:
    snprintf(cdata->video_name,16,"unknown");
    break;
    fprintf(stderr,"add video format %d\n",vidst->codec->codec_id);
    break;
  }

  if (vidst->codec) codec = avcodec_find_decoder(vidst->codec->codec_id);
#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"video type is %s %d x %d (%d x %d +%d +%d) %p\n",cdata->video_name,
          cdata->width,cdata->height,cdata->frame_width,cdata->frame_height,cdata->offs_x,cdata->offs_y,codec);
#endif


  if (!vidst->codec) {
    if (strlen(cdata->video_name)>0)
      fprintf(stderr, "avi_decoder: Could not find avcodec codec for video type %s\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }

  if ((retval=avcodec_open(vidst->codec, codec)) < 0) {
    fprintf(stderr, "avi_decoder: Could not open avcodec context (%d) %d\n",retval,vidst->codec->frame_number);
    detach_stream(cdata);
    return FALSE;
  }

  priv->picture = av_frame_alloc();

  priv->input_position=priv->data_start;

  // TODO ***
  //avi_reset_header(priv->s);

  priv->kframe=add_keyframe(cdata,priv->data_start,0,0);

  priv->offs=0;

  ctx->width=cdata->width;
  ctx->height=cdata->height;
  ctx->flags|= CODEC_FLAG_TRUNCATED;


  do {

    do {
      //if (priv->avpkt.data!=NULL) free(priv->avpkt.data);
      retval = get_next_video_packet(cdata,0,-1);
      if (retval==-2) {
        fprintf(stderr, "avi_decoder: No frames found.\n");
        detach_stream(cdata);
        return FALSE; // eof
      }
    } while (retval!=0);


#if LIBAVCODEC_VERSION_MAJOR >= 53
    len=avcodec_decode_video2(vidst->codec, priv->picture, &got_picture, &priv->avpkt);
#else
    len=avcodec_decode_video(vidst->codec, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif
    printf("got len %d\n",len);
    //priv->avpkt.size-=len;
  } while (!got_picture);

  if (!got_picture) {
    fprintf(stderr, "avi_decoder: Did not get picture.\n");
    detach_stream(cdata);
    return FALSE;
  }

  if (priv->avpkt.size!=0) {
    fprintf(stderr, "avi_decoder: Multi frame packets, not decoding.\n");
    detach_stream(cdata);
    return FALSE;
  }

  pts=priv->start_dts=priv->frame_dts;

#ifdef DEBUG
  printf("first pts is %ld\n",pts);
#endif

  if (pts==AV_NOPTS_VALUE) {
    fprintf(stderr, "avi_decoder: No timestamps for frames, not decoding.\n");
    detach_stream(cdata);
    return FALSE;
  }



  do {
    free(priv->avpkt.data);
    retval = get_next_video_packet(cdata,-1,-1);

    // try to get dts of second frame
    if (retval!=-2) { // eof
      if (retval==0) {

        if (priv->frame_dts==AV_NOPTS_VALUE) {
          fprintf(stderr, "avi_decoder: No timestamp for second frame, aborting.\n");
          detach_stream(cdata);
          return FALSE;
        }

        pts=priv->frame_dts-pts;

#ifdef DEBUG
        printf("delta pts is %ld\n",pts);
#endif

        if (pts>0) cdata->fps=1000./(double)pts;
      }
    }
  } while (retval<0&&retval!=-2);

  free(priv->avpkt.data);
  priv->avpkt.data=NULL;
  priv->avpkt.size=0;

  priv->ctx=ctx=vidst->codec;

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
    fprintf(stderr, "avi_decoder: Could not find a usable palette for (%d) %s\n",ctx->pix_fmt,cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  cdata->current_palette=cdata->palettes[0];

  // re-get fps, width, height, nframes - actually avcodec is pretty useless at getting this
  // so we fall back on the values we obtained ourselves

  if (cdata->width==0) cdata->width=ctx->width-cdata->offs_x*2;
  if (cdata->height==0) cdata->height=ctx->height-cdata->offs_y*2;

  if (cdata->width*cdata->height==0) {
    fprintf(stderr, "avi_decoder: invalid width and height (%d X %d)\n",cdata->width,cdata->height);
    detach_stream(cdata);
    return FALSE;
  }

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

  if (cdata->fps==0.&&ctx->time_base.num==0) {
    if (ctx->time_base.den==1) cdata->fps=25.;
  }

  if (cdata->fps==0.||cdata->fps==1000.) {
    fprintf(stderr, "avi_decoder: invalid framerate %.4f (%d / %d)\n",cdata->fps,ctx->time_base.den,ctx->time_base.num);
    detach_stream(cdata);
    return FALSE;
  }

  if (ctx->ticks_per_frame==2) {
    // TODO - needs checking
    cdata->fps/=2.;
    cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;
  }

  priv->last_frame=-1;
  priv->black_fill=FALSE;

  vidst->duration = get_last_video_dts(cdata);

  cdata->nframes = dts_to_frame(cdata,vidst->duration+priv->start_dts);

  return TRUE;
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  avcodec_register_all();
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata(void) {
  lives_avi_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->URI=NULL;

  cdata->priv=priv=malloc(sizeof(lives_avi_priv_t));

  cdata->seek_flag=0;

  priv->ctx=NULL;
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

  lives_avi_priv_t *priv;


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
    //cdata->current_palette=cdata->palettes[0];
    cdata->current_clip=0;
  }

  cdata->nclips=1;

  ///////////////////////////////////////////////////////////

  sprintf(cdata->container_name,"%s","avi");

  // cdata->height was set when we attached the stream

  cdata->interlace=LIVES_INTERLACE_NONE;

  cdata->frame_width=cdata->width+cdata->offs_x*2;
  cdata->frame_height=cdata->height+cdata->offs_y*2;

  priv=cdata->priv;

  if (priv->ctx->width==cdata->frame_width) cdata->offs_x=0;
  if (priv->ctx->height==cdata->frame_height) cdata->offs_y=0;

  ////////////////////////////////////////////////////////////////////

  cdata->asigned=TRUE;
  cdata->ainterleaf=TRUE;

  return cdata;
}

static size_t write_black_pixel(unsigned char *idst, int pal, int npixels, int y_black) {
  unsigned char *dst=idst;
  register int i;

  for (i=0; i<npixels; i++) {
    switch (pal) {
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
      dst[0]=dst[1]=dst[2]=0;
      dst[3]=255;
      dst+=4;
      break;
    case WEED_PALETTE_ARGB32:
      dst[1]=dst[2]=dst[3]=0;
      dst[0]=255;
      dst+=4;
      break;
    case WEED_PALETTE_UYVY8888:
      dst[1]=dst[3]=y_black;
      dst[0]=dst[2]=128;
      dst+=4;
      break;
    case WEED_PALETTE_YUYV8888:
      dst[0]=dst[2]=y_black;
      dst[1]=dst[3]=128;
      dst+=4;
      break;
    case WEED_PALETTE_YUV888:
      dst[0]=y_black;
      dst[1]=dst[2]=128;
      dst+=3;
      break;
    case WEED_PALETTE_YUVA8888:
      dst[0]=y_black;
      dst[1]=dst[2]=128;
      dst[3]=255;
      dst+=4;
      break;
    case WEED_PALETTE_YUV411:
      dst[0]=dst[3]=128;
      dst[1]=dst[2]=dst[4]=dst[5]=y_black;
      dst+=6;
    default:
      break;
    }
  }
  return idst-dst;
}


boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, int *rowstrides, int height, void **pixel_data) {
  // seek to frame,

  int64_t nextframe=0;
  int tfrag=0;
  lives_avi_priv_t *priv=cdata->priv;
  int64_t target_pts=frame_to_dts(cdata,tframe);
  int xheight=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;
  int rescan_limit=16;  // pick some arbitrary value
  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;
  boolean got_picture=FALSE;
  unsigned char *dst,*src;
  unsigned char black[4]= {0,0,0,255};
  register int i,p;

#ifdef DEBUG_KFRAMES
  fprintf(stderr,"vals %ld %ld\n",tframe,priv->last_frame);
#endif

  // calc frame width and height, including any border

  if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P) {
    nplanes=3;
    black[0]=y_black;
    black[1]=black[2]=128;
  } else if (pal==WEED_PALETTE_YUVA4444P) {
    nplanes=4;
    black[0]=y_black;
    black[1]=black[2]=128;
    black[3]=255;
  }

  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) psize=3;

  if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_UYVY8888||pal==WEED_PALETTE_YUYV8888||
      pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) psize=4;

  if (pal==WEED_PALETTE_YUV411) psize=6;

  if (pal==WEED_PALETTE_A1) dstwidth>>=3;

  dstwidth*=psize;

  if (cdata->frame_height > cdata->height && height == cdata->height) {
    // host ignores vertical border
    btop=0;
    xheight=cdata->height;
    bbot=xheight-1;
  }

  if (cdata->frame_width > cdata->width && rowstrides[0] < cdata->frame_width*psize) {
    // host ignores horizontal border
    bleft=bright=0;
  }

  ////////////////////////////////////////////////////////////////////

  if (tframe!=priv->last_frame) {

    if (priv->last_frame==-1 || (tframe<priv->last_frame) || (tframe - priv->last_frame > rescan_limit)) {
      if (priv->avpkt.data!=NULL) free(priv->avpkt.data);
      priv->avpkt.data=NULL;
      priv->avpkt.size=0;

      if ((priv->kframe=get_idx_for_pts(cdata,target_pts-priv->start_dts))!=NULL) {
        priv->input_position=priv->kframe->offs;
        nextframe=dts_to_frame(cdata,priv->kframe->dts+priv->start_dts);
        tfrag=priv->kframe->frag;
      } else priv->input_position=priv->data_start;

      // we are now at the kframe before or at target - parse packets until we hit target


#ifdef DEBUG_KFRAMES
      if (priv->kframe!=NULL) printf("got kframe %ld frag %d for frame %ld\n",dts_to_frame(cdata,priv->kframe->dts+priv->start_dts),
                                       priv->kframe->frag,tframe);
#endif

      avcodec_flush_buffers(priv->ctx);

      // TODO ***
      //avi_reset_header(priv->s);
      priv->black_fill=FALSE;
    } else {
      nextframe=priv->last_frame+1;
      tfrag=-1;
    }

    priv->ctx->skip_frame=AVDISCARD_NONREF;

    priv->last_frame=tframe;


    // do this until we reach target frame //////////////

    while (nextframe<=tframe) {
      int len,ret;

      if (nextframe==tframe) priv->ctx->skip_frame=AVDISCARD_DEFAULT;

      if (priv->avpkt.size==0) {
        ret = get_next_video_packet(cdata,tfrag,-1);
        if (ret<0) {
          free(priv->avpkt.data);
          priv->avpkt.data=NULL;
          priv->avpkt.size=0;
          if (ret==-2) {
            priv->black_fill=TRUE;
            break;
          }
          fprintf(stderr,"avi_decoder: got frame decode error %d at frame %ld\n",ret,nextframe+1);
          continue;
        }
      }

#if LIBAVCODEC_VERSION_MAJOR >= 52
      len=avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
      len=avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

      priv->avpkt.size-=len;

#ifdef DEBUG_FRAMES
      fprintf(stderr,"vals here %ld %ld %d\n",nextframe,tframe,priv->avpkt.size);
#endif

      if (priv->avpkt.size==0) {
        free(priv->avpkt.data);
        priv->avpkt.data=NULL;
      }
      tfrag=-1;
      nextframe++;
    }

    /////////////////////////////////////////////////////

  }

  if (priv->black_fill) btop=cdata->frame_height;

  for (p=0; p<nplanes; p++) {
    dst=pixel_data[p];
    src=priv->picture->data[p];

    for (i=0; i<xheight; i++) {
      if (i<btop||i>bbot) {
        // top or bottom border, copy black row
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
            pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
          memset(dst,black[p],dstwidth+(bleft+bright)*psize);
          dst+=dstwidth+(bleft+bright)*psize;
        } else dst+=write_black_pixel(dst,pal,dstwidth/psize+bleft+bright,y_black);
        continue;
      }

      if (bleft>0) {
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
            pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
          memset(dst,black[p],bleft*psize);
          dst+=bleft*psize;
        } else dst+=write_black_pixel(dst,pal,bleft,y_black);
      }

      memcpy(dst,src,dstwidth);
      dst+=dstwidth;

      if (bright>0) {
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
            pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
          memset(dst,black[p],bright*psize);
          dst+=bright*psize;
        } else dst+=write_black_pixel(dst,pal,bright,y_black);
      }

      src+=priv->picture->linesize[p];
    }
    if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P)) {
      dstwidth>>=1;
      bleft>>=1;
      bright>>=1;
    }
    if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P)) {
      xheight>>=1;
      btop>>=1;
      bbot>>=1;
    }
  }

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
  module_check_init();
  lives_clip_data_t *cdata=get_clip_data("foo.avi",NULL);
  if (cdata==NULL) printf("bang\n");
  return 1;
}
