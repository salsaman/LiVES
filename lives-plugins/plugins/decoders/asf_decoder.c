
// LiVES - asf decoder plugin
// (c) G. Finch 2011 - 2014 <salsaman@gmail.com>

/*
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * LiVES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LiVES; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

// based on code from libavformat

/*
 * ASF compatible demuxer
 * Copyright (c) 2000, 2001 Fabrice Bellard
 *
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#if !defined (IS_MINGW) && !defined (IS_SOLARIS) && !defined (__FreeBSD__)
#include <endian.h>
#endif
#include <sys/stat.h>
#include <pthread.h>

const char *plugin_version="LiVES asf/wmv decoder version 1.1";

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#ifndef HAVE_SYSTEM_WEED
#include "../../../libweed/weed-palettes.h"
#endif

#define HAVE_AVCODEC
#define HAVE_AVUTIL

#ifdef HAVE_SYSTEM_WEED_COMPAT
#include <weed/weed-compat.h>
#else
#include "../../../libweed/weed-compat.h"
#endif

#include <libavformat/avformat.h>
#include <libavcodec/version.h>
#include <libavutil/avstring.h>

#include "decplugin.h"
#include "libav_helper.h"

#include "asf_decoder.h"

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

static index_container_t **indices;
static int nidxc;
static pthread_mutex_t indices_mutex;
static pthread_mutexattr_t mattr;


////////////////////////////////////////////////////////////////////////////


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
  lives_asf_priv_t *priv=cdata->priv;
  return (int)((double)(frame)*1000./cdata->fps)+priv->start_dts;
}

static int64_t dts_to_frame(const lives_clip_data_t *cdata, int dts) {
  lives_asf_priv_t *priv=cdata->priv;
  return (int64_t)((double)(dts-priv->start_dts)/1000.*cdata->fps+.5);
}


////////////////////////////////////////////////////////////////

static boolean get_guid(int fd, lives_asf_guid *g) {
  if ((read(fd, g, sizeof(lives_asf_guid)))!=sizeof(lives_asf_guid)) return FALSE;
  return TRUE;
}

static boolean guidcmp(const void *v1, const void *v2) {

  int res = memcmp(v1, v2, sizeof(lives_asf_guid));

#ifdef DEBUG
  uint8_t *g1=(uint8_t *)v1;
  uint8_t *g2=(uint8_t *)v2;
  if (res==0) {
    printf("check %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X %0X\n",(uint8_t)g1[0],(uint8_t)g1[1],(uint8_t)g1[2],
           (uint8_t)g1[3],(uint8_t)g1[4],(uint8_t)g1[5],(uint8_t)g1[6],(uint8_t)g1[7],(uint8_t)g1[8],(uint8_t)g1[9],(uint8_t)g1[10],(uint8_t)g1[11],
           (uint8_t)g1[12],(uint8_t)g1[13],(uint8_t)g1[14],(uint8_t)g1[15]);
  }
#endif

  return res!=0;
}


static void get_str16_nolen(unsigned char *inbuf, int len, char *buf, int buf_size) {
  int i=0;
  char *q = buf;
  while (len > 1) {
    uint8_t tmp;
    uint32_t ch;

    GET_UTF16(ch, (len -= 2) >= 0 ? get_le16int(&inbuf[i]) : 0, break;)
    PUT_UTF8(ch, tmp, if (q - buf < buf_size - 1) *q++ = tmp;)
      i+=2;
  }
  *q = '\0';
}


//////////////////////////////////////////////////////////////////

static boolean check_eof(const lives_clip_data_t *cdata) {
  lives_asf_priv_t *priv=cdata->priv;
  if (priv->input_position>=priv->filesize) return TRUE;
  return FALSE;
}


static int get_value(const lives_clip_data_t *cdata, int type) {
  unsigned char buffer[4096];
  lives_asf_priv_t *priv=cdata->priv;
  switch (type) {
  case 2:
  case 3:
    if (read(priv->fd,buffer,4)<4) {
      fprintf(stderr, "asf_decoder: read error getting value\n");
      close(priv->fd);
      return INT_MIN;
    }
    priv->input_position+=4;
    return get_le32int(buffer);
  case 4:
    if (read(priv->fd,buffer,8)<8) {
      fprintf(stderr, "asf_decoder: read error getting value\n");
      close(priv->fd);
      return INT_MIN;
    }
    priv->input_position+=8;
    return get_le64int(buffer);
  case 5:
    if (read(priv->fd,buffer,2)<2) {
      fprintf(stderr, "asf_decoder: read error getting value\n");
      close(priv->fd);
      return INT_MIN;
    }
    priv->input_position+=2;
    return get_le16int(buffer);
  default:
    return INT_MIN;
  }
}



static boolean get_tag(const lives_clip_data_t *cdata, AVFormatContext *s, const char *key, int type, int len) {
  char *value;
  unsigned char buffer[4096];
  lives_asf_priv_t *priv=cdata->priv;

  if ((unsigned)len >= (UINT_MAX - 1)/2) return TRUE;
  value = malloc(2*len+1);
  if (!value) return TRUE;

  if (type == 0) {         // UTF16-LE
    if (read(priv->fd,buffer,len)<len) {
      fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
      close(priv->fd);
      return FALSE;
    }
    get_str16_nolen(buffer, len, value, 2*len + 1);
    priv->input_position+=len;
  } else if (type > 1 && type <= 5) {  // boolean or DWORD or QWORD or WORD
    uint64_t num = get_value(cdata, type);
    if (num==INT_MIN) return FALSE;
    snprintf(value, len, "%"PRIu64, num);
  } else {
    lseek(priv->fd,len,SEEK_CUR);
    priv->input_position+=len;
    free(value);
    fprintf(stderr, "asf_decoder: Unsupported value type %d len %d in tag %s.\n", type, len, key);
    return TRUE;
  }

  av_dict_set(&s->metadata, key, value, 0);

  free(value);
  return TRUE;
}


static void asf_reset_header(AVFormatContext *s) {
  ASFContext *asf = s->priv_data;
  ASFStream *asf_st;
  int i;

  asf->packet_nb_frames = 0;
  asf->packet_size_left = 0;
  asf->packet_segments = 0;
  asf->packet_flags = 0;
  asf->packet_property = 0;
  asf->packet_timestamp = 0;
  asf->packet_segsizetype = 0;
  asf->packet_segments = 0;
  asf->packet_seq = 0;
  asf->packet_replic_size = 0;
  asf->packet_key_frame = 0;
  asf->packet_padsize = 0;
  asf->packet_frag_offset = 0;
  asf->packet_frag_size = 0;
  asf->packet_frag_timestamp = 0;
  asf->packet_multi_size = 0;
  asf->packet_obj_size = 0;
  asf->packet_time_delta = 0;
  asf->packet_time_start = 0;

  for (i=0; i<s->nb_streams; i++) {
    asf_st= s->streams[i]->priv_data;
    av_free_packet(&asf_st->pkt);
    asf_st->frag_offset=0;
    asf_st->seq=0;
  }
  asf->asf_st= NULL;
}


static void get_sync(lives_asf_priv_t *priv) {
  unsigned char buffer;

  while (1) {
    if (read(priv->fd, &buffer, 1) < 1) return;
    priv->input_position++;
    if (buffer==0x82) goto got_82;
    else continue;

got_82:
    if (read(priv->fd, &buffer, 1) < 1) return;
    priv->input_position++;

    if (buffer==0x82) goto got_82;
    if (buffer==0) goto got_0;
    else continue;

got_0:
    if (read(priv->fd, &buffer, 1) < 1) return;
    priv->input_position++;
    if (buffer==0) return; // OK
    if (buffer==0x82) goto got_82;

  }
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
  lives_asf_priv_t *priv=cdata->priv;
  index_entry *idx,*lidx,*nidx;

  lidx=idx=priv->idxc->idx;

  while (idx!=NULL) {
    if (idx->dts==dts) return idx; // already indexed
    if (idx->dts>dts) {
      // insert before idx
      nidx=idx_alloc(offs,frag,dts);
      nidx->next=idx;
      if (idx==priv->idxc->idx) priv->idxc->idx=nidx;
      else lidx->next=nidx;
      return nidx;
    }
    lidx=idx;
    idx=idx->next;
  }

  // insert at tail

  nidx=idx_alloc(offs,frag,dts);

  if (lidx!=NULL) lidx->next=nidx;
  else priv->idxc->idx=nidx;

  nidx->next=NULL;

  return nidx;
}




static int get_next_video_packet(const lives_clip_data_t *cdata, int tfrag, int64_t tdts) {
  lives_asf_priv_t *priv=cdata->priv;
  uint32_t packet_length, padsize;
  unsigned char buffer[8];
  uint8_t num;
  //uint16_t duration;
  int rsize;
  int streamid;
  int pack_fill=0;
  int64_t ts0;
  int64_t frag_start;
  AVFormatContext *s=priv->s;
  ASFContext *asf=priv->asf;
  int16_t vidindex=priv->st->id;

  // create avpacket

  // if (nodata):
  // seek to input_pos
  // a:
  // sync to hdr start

  // b)
  // do until avpacket is full:
  // if no more fragments, set target fragment to 0, goto a)
  // get next vid fragment, decrement target fragment
  // if we reached or passed target fragment, fill avpacket with fragment data
  // if fragment_offset is 0, and we have data in our packet, packet is full so return


  // if (!nodata):
  // create avpacket
  // set target fragment to 0
  // goto b)

  // if we have target (tdts) we parse only, until we reach a video fragment with dts>=tdts


  if (tdts==-1) {
    priv->avpkt.size=priv->def_packet_size+FF_INPUT_BUFFER_PADDING_SIZE;
    priv->avpkt.data=malloc(priv->avpkt.size);
    memset(priv->avpkt.data,0,priv->avpkt.size);
  }

  while (1) {

    lseek(priv->fd,priv->input_position,SEEK_SET);

    if (tfrag >=0 || asf->packet_segments==0) {

      rsize=8;

#ifdef DEBUG
      printf("pos is %x\n",priv->input_position);
#endif
      get_sync(priv);

      priv->hdr_start=priv->input_position-3;

      if (read(priv->fd, buffer, 2)<2) return -2;

      priv->input_position+=2;

      asf->packet_flags    = buffer[0];
      asf->packet_property = buffer[1];

      rsize+=3;

      DO_2BITS(asf->packet_flags >> 5, packet_length, s->packet_size);
      DO_2BITS(asf->packet_flags >> 1, padsize, 0); // sequence ignored
      DO_2BITS(asf->packet_flags >> 3, padsize, 0); // padding length

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error getting value\n");
        return -5;
      }

      priv->input_position+=4;

      asf->packet_timestamp = get_le32int(buffer);  // send time

      //the following checks prevent overflows and infinite loops
      if (!packet_length || packet_length >= (1U<<29)) {
        fprintf(stderr, "invalid packet_length %d at:%"PRId64"\n", packet_length, priv->input_position);
        return -3;
      }
      if (padsize >= packet_length) {
        fprintf(stderr, "invalid padsize %d at:%"PRId64"\n", padsize, priv->input_position);
        return -4;
      }

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error getting value\n");
        return -2;
      }
      priv->input_position+=2;

      //duration = get_le16int(buffer);

      // rsize has at least 11 bytes which have to be present

      if (asf->packet_flags & 0x01) {
        // multi segments
        if (read(priv->fd,buffer,1)<1) {
          fprintf(stderr, "asf_decoder: read error getting value\n");
          return -2;
        }
        priv->input_position++;
        asf->packet_segsizetype = buffer[0];
        rsize++;
        asf->packet_segments = asf->packet_segsizetype & 0x3f;
      } else {
        // single segment
        asf->packet_segments = 1;
        asf->packet_segsizetype = 0x80;
      }

      asf->packet_size_left = packet_length - padsize - rsize;

      if (packet_length < asf->hdr.min_pktsize)
        padsize += asf->hdr.min_pktsize - packet_length;

      asf->packet_padsize = padsize;

      asf->packet_time_start = 0;

      priv->fragnum=0;
      // packet header parsed
    }

    if (asf->packet_size_left < FRAME_HEADER_SIZE || asf->packet_segments < 1) {
      fprintf(stderr, "asf_decoder: bad packet\n");
      return -8;
    }

    do {
      // read frame header for fragment to get stream

      lseek(priv->fd, priv->input_position, SEEK_SET);
      frag_start=priv->input_position;

      if (read(priv->fd,buffer,1)<1) {
        fprintf(stderr, "asf_decoder: read error getting value\n");
        return -2;
      }

      priv->input_position++;

      num=buffer[0];
      asf->packet_key_frame = num >> 7;

      streamid = num&0x7f;

#ifdef DEBUG
      printf("streamid is %d\n",streamid);
#endif
      rsize=1;

      if (asf->packet_time_start==0) {
        asf->asf_st = s->streams[asf->stream_index]->priv_data;

        // read rest of frame header

        DO_2BITS(asf->packet_property >> 4, asf->packet_seq, 0);
        DO_2BITS(asf->packet_property >> 2, asf->packet_frag_offset, 0);
        DO_2BITS(asf->packet_property, asf->packet_replic_size, 0);

        if (asf->packet_replic_size >= 8) {

          if (read(priv->fd,buffer,4)<4) {
            fprintf(stderr, "asf_decoder: read error getting value\n");
            return -2;
          }
          priv->input_position+=4;
          asf->packet_obj_size = get_le32int(buffer);

          if (asf->packet_obj_size >= (1<<24) || asf->packet_obj_size <= 0) {
            if (streamid==vidindex) {
              fprintf(stderr, "asf_decoder: packet_obj_size invalid\n");
              return -9;
            }
          }

          if (read(priv->fd,buffer,4)<4) {
            fprintf(stderr, "asf_decoder: read error getting value\n");
            return -2;
          }
          priv->input_position+=4;

          asf->packet_frag_timestamp = get_le32int(buffer); // timestamp

          if (asf->packet_replic_size >= 8+38+4) {

            priv->input_position+=10;
            lseek(priv->fd,10,SEEK_CUR);

            if (read(priv->fd,buffer,8)<8) {
              fprintf(stderr, "asf_decoder: read error getting value\n");
              return -2;
            }
            priv->input_position+=8;
            ts0= get_le64int(buffer);

            priv->input_position+=asf->packet_replic_size + 16 - 38 - 4;
            lseek(priv->fd,asf->packet_replic_size + 16 - 38 - 4,SEEK_CUR);

            if (ts0!= -1) asf->packet_frag_timestamp= ts0/10000;
            else         asf->packet_frag_timestamp= AV_NOPTS_VALUE;
          } else {
            priv->input_position+=asf->packet_replic_size - 8;
            lseek(priv->fd,asf->packet_replic_size - 8,SEEK_CUR);
          }
          rsize += asf->packet_replic_size; // FIXME - check validity
        } else if (asf->packet_replic_size==1) {
          // single packet ? - frag_offset is beginning timestamp
          asf->packet_time_start = asf->packet_frag_offset;
          asf->packet_frag_offset = 0;
          asf->packet_frag_timestamp = asf->packet_timestamp;

          if (read(priv->fd,buffer,1)<1) {
            fprintf(stderr, "asf_decoder: read error getting value\n");
            return -2;
          }
          priv->input_position+=1;

          asf->packet_time_delta = buffer[0];

          rsize++;
        } else if (asf->packet_replic_size!=0) {
          if (check_eof(cdata)) {
            // could also be EOF in DO_2_BITS
            fprintf(stderr, "asf_decoder: EOF");
            return -2;
          }
          fprintf(stderr, "unexpected packet_replic_size of %d\n", asf->packet_replic_size);
          return -1;
        }

        if (asf->packet_flags & 0x01) {
          // multi fragments
          DO_2BITS(asf->packet_segsizetype >> 6, asf->packet_frag_size, 0); // 0 is illegal
          if (asf->packet_frag_size > asf->packet_size_left - rsize) {
            fprintf(stderr,"asf_decoder: packet_frag_size is invalid (%d > %d) %d %d\n",asf->packet_frag_size,asf->packet_size_left - rsize,
                    asf->packet_padsize,rsize);

#ifdef DEBUG
            printf("skipping %d %ld\n",asf->packet_size_left + asf->packet_padsize - 2, asf->packet_padsize);
#endif

            lseek(priv->fd, asf->packet_size_left + asf->packet_padsize - 2 - rsize, SEEK_CUR);
            priv->input_position+=asf->packet_size_left + asf->packet_padsize - 2 - rsize;
            asf->packet_segments=0;
            asf->packet_size_left=0;

            return -1;
          }
#ifdef DEBUG
          printf("Fragsize %d nsegs %d\n", asf->packet_frag_size,asf->packet_segments);
#endif
        } else {
          asf->packet_frag_size = asf->packet_size_left - rsize;
#ifdef DEBUG
          printf("Using rest  %d %d %d\n", asf->packet_frag_size, asf->packet_size_left, rsize);
#endif
        }
      }

      if (streamid == vidindex) {
#ifdef DEBUG
        printf("got vid fragment in packet !\n");
#endif

        if (asf->packet_key_frame&&asf->packet_frag_offset==0&&priv->have_start_dts) {
          pthread_mutex_lock(&priv->idxc->mutex);
          priv->kframe=add_keyframe(cdata,priv->hdr_start,priv->fragnum,asf->packet_frag_timestamp-priv->start_dts);
          pthread_mutex_unlock(&priv->idxc->mutex);

#ifdef DEBUG
          printf("and is keyframe !\n");
#endif
        }

        if (tdts>=0 && (asf->packet_frag_timestamp>=tdts)) {
          // reached target dts
          return 0;
        }


        if (asf->packet_frag_offset==0) {
          if (pack_fill==0) priv->frame_dts=asf->packet_frag_timestamp;
          else if (tdts==-1) {
            // got complete AV video packet
            priv->input_position=frag_start;
            priv->avpkt.size=pack_fill;
            return 0;
          }
        }

        priv->fragnum++;

        if (tfrag<=0 && tdts==-1) {

          if (asf->packet_frag_offset!=0 && pack_fill==0) {
            // we reached our target, but it has a non-zero offset, which is not allowed
            // So skip this packet;
            // - this could happen if we have broken fragments at the start of the clip.
            priv->input_position+=asf->packet_frag_size;
            continue;
          }

          while (asf->packet_frag_offset+asf->packet_frag_size>priv->avpkt.size) {
            fprintf(stderr, "asf_decoder: buffer overflow reading vid packet (%d + %d > %d),\n increasing buffer size\n",
                    asf->packet_frag_offset,asf->packet_frag_size,priv->avpkt.size);

            priv->avpkt.data=realloc(priv->avpkt.data,priv->def_packet_size*2+FF_INPUT_BUFFER_PADDING_SIZE);
            memset(priv->avpkt.data+priv->avpkt.size,0,priv->def_packet_size);
            priv->def_packet_size*=2;
            priv->avpkt.size=priv->def_packet_size+FF_INPUT_BUFFER_PADDING_SIZE;
          }

          if (read(priv->fd,priv->avpkt.data+asf->packet_frag_offset,asf->packet_frag_size)
              <asf->packet_frag_size) {
            fprintf(stderr, "asf_decoder: EOF error reading vid packet\n");
            return -2;
          }
          pack_fill=asf->packet_frag_offset+asf->packet_frag_size;
        }

        priv->input_position+=asf->packet_frag_size;

        tfrag--;

      } else {
        // fragment is for another stream
        priv->input_position+=asf->packet_frag_size;
      }

      asf->packet_size_left -= asf->packet_frag_size + rsize;
      asf->packet_segments--;

    } while (asf->packet_segments>0);

    // no more fragments left, skip remainder of asf packet

#ifdef DEBUG
    //printf("skipping %d %ld\n",asf->packet_size_left + asf->packet_padsize - 2, asf->packet_padsize);
#endif

    priv->input_position+=asf->packet_size_left + asf->packet_padsize - 2;

  }

  // will never reach here
  return 0;
}


static index_entry *get_idx_for_pts(const lives_clip_data_t *cdata, int64_t pts) {
  lives_asf_priv_t *priv=cdata->priv;
  int64_t tdts=pts;
  index_entry *idx=priv->idxc->idx,*lidx=idx;
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
  lives_asf_priv_t *priv=cdata->priv;

  pthread_mutex_lock(&priv->idxc->mutex);
  priv->kframe=get_idx_for_pts(cdata,frame_to_dts(cdata,0)-priv->start_dts);
  pthread_mutex_unlock(&priv->idxc->mutex);
  // this will parse through the file, adding keyframes, until we reach EOF

  priv->input_position=priv->kframe->offs;
  asf_reset_header(priv->s);
  get_next_video_packet(cdata,priv->kframe->frag,INT_MAX);

  // priv->kframe will hold last kframe in the clip
  return priv->frame_dts-priv->start_dts;
}


//////////////////////////////////////////////

static index_container_t *idxc_for(lives_clip_data_t *cdata) {
  // check all idxc for string match with URI
  index_container_t *idxc;
  register int i;

  pthread_mutex_lock(&indices_mutex);

  for (i=0; i<nidxc; i++) {
    if (indices[i]->clients[0]->current_clip==cdata->current_clip&&
        !strcmp(indices[i]->clients[0]->URI,cdata->URI)) {
      idxc=indices[i];
      // append cdata to clients
      idxc->clients=(lives_clip_data_t **)realloc(idxc->clients,(idxc->nclients+1)*sizeof(lives_clip_data_t *));
      idxc->clients[idxc->nclients]=cdata;
      idxc->nclients++;
      //
      pthread_mutex_unlock(&indices_mutex);
      return idxc;
    }
  }

  indices=(index_container_t **)realloc(indices,(nidxc+1)*sizeof(index_container_t *));

  // match not found, create a new index container
  idxc=(index_container_t *)malloc(sizeof(index_container_t));

  idxc->idx=NULL;

  idxc->nclients=1;
  idxc->clients=(lives_clip_data_t **)malloc(sizeof(lives_clip_data_t *));
  idxc->clients[0]=cdata;

  pthread_mutex_init(&idxc->mutex,&mattr);

  indices[nidxc]=idxc;
  pthread_mutex_unlock(&indices_mutex);

  nidxc++;

  return idxc;
}


static void idxc_release(lives_clip_data_t *cdata) {
  lives_asf_priv_t *priv=cdata->priv;
  index_container_t *idxc=priv->idxc;
  register int i,j;

  if (idxc==NULL) return;

  pthread_mutex_lock(&indices_mutex);

  if (idxc->nclients==1) {
    // remove this index
    index_free(idxc->idx);
    free(idxc->clients);
    for (i=0; i<nidxc; i++) {
      if (indices[i]==idxc) {
        nidxc--;
        for (j=i; j<nidxc; j++) {
          indices[j]=indices[j+1];
        }
        free(idxc);
        if (nidxc==0) {
          free(indices);
          indices=NULL;
        } else indices=(index_container_t **)realloc(indices,nidxc*sizeof(index_container_t *));
        break;
      }
    }
  } else {
    // reduce client count by 1
    for (i=0; i<idxc->nclients; i++) {
      if (idxc->clients[i]==cdata) {
        // remove this entry
        idxc->nclients--;
        for (j=i; j<idxc->nclients; j++) {
          idxc->clients[j]=idxc->clients[j+1];
        }
        idxc->clients=(lives_clip_data_t **)realloc(idxc->clients,idxc->nclients*sizeof(lives_clip_data_t *));
        break;
      }
    }
  }

  pthread_mutex_unlock(&indices_mutex);

}


static void idxc_release_all(void) {
  register int i;

  for (i=0; i<nidxc; i++) {
    index_free(indices[i]->idx);
    free(indices[i]->clients);
    free(indices[i]);
  }
  nidxc=0;
}


static void detach_stream(lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_asf_priv_t *priv=cdata->priv;

  cdata->seek_flag=0;

  if (priv->avpkt.data!=NULL) free(priv->avpkt.data);
  priv->avpkt.data=NULL;
  priv->avpkt.size=0;

  if (priv->ctx!=NULL) {
    avcodec_close(priv->ctx);
    av_free(priv->ctx);
  }

  if (priv->picture!=NULL) av_frame_free(&priv->picture);

  priv->ctx=NULL;
  priv->picture=NULL;

  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;

  free(priv->asf);

  av_free(priv->s);
  //avformat_free_context(priv->s);

  if (priv->st!=NULL) {
    if (priv->st->codec->extradata_size!=0) free(priv->st->codec->extradata);
  }

  if (priv->asf_st!=NULL) free(priv->asf_st);

  close(priv->fd);
}




static boolean attach_stream(lives_clip_data_t *cdata, boolean isclone) {
  // open the file and get metadata
  lives_asf_priv_t *priv=cdata->priv;
  char header[16];
  unsigned char buffer[4096];
  unsigned char flags;

  int i,len,retval;
  int size;

  boolean got_vidst=FALSE;
  boolean got_picture=FALSE;
  boolean gotframe2=FALSE;
  boolean is_partial_clone=FALSE;

  double fps;

  AVCodec *codec=NULL;
  AVCodecContext *ctx;
  AVStream *vidst=NULL;

  int64_t pts=AV_NOPTS_VALUE,pts2=pts;
  int64_t gsize;
  int64_t ftime;

  lives_asf_guid g;

  AVRational dar[128];
  uint32_t bitrate[128];

  int16_t vidindex=-1;

  struct stat sb;

#ifdef DEBUG
  fprintf(stderr,"\n");
#endif

  if (isclone&&!priv->inited) {
    isclone=FALSE;
    if (cdata->fps>0.&&cdata->nframes>0)
      is_partial_clone=TRUE;
  }


  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "asf_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

#ifdef IS_MINGW
  setmode(priv->fd,O_BINARY);
#endif

  if (isclone) {
    lseek(priv->fd, ASF_PROBE_SIZE+14, SEEK_SET);
    priv->input_position=ASF_PROBE_SIZE+14;
    goto seek_skip;
  }

  if (read(priv->fd, header, ASF_PROBE_SIZE) < ASF_PROBE_SIZE) {
    // for example, might be a directory
#ifdef DEBUG
    fprintf(stderr, "asf_decoder: unable to read header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }

  priv->input_position=ASF_PROBE_SIZE;

  // test header
  if (guidcmp(header, &lives_asf_header)) {
#ifdef DEBUG
    fprintf(stderr, "asf_decoder: not asf header\n");
#endif
    close(priv->fd);
    return FALSE;
  }

  // skip 14 bytes
  lseek(priv->fd, 14, SEEK_CUR);

  priv->input_position+=14;

  cdata->par=1.;
  cdata->fps=0.;
  cdata->width=cdata->frame_width=cdata->height=cdata->frame_height=0;
  cdata->offs_x=cdata->offs_y=0;

  cdata->arate=0;
  cdata->achans=0;
  cdata->asamps=0;
  sprintf(cdata->audio_name,"%s","");

  cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;

  cdata->offs_x=0;
  cdata->offs_y=0;

  fstat(priv->fd,&sb);
  priv->filesize=sb.st_size;

seek_skip:

  priv->idxc=idxc_for(cdata);

  priv->asf=(ASFContext *)malloc(sizeof(ASFContext));
  memset(&priv->asf->asfid2avid, -1, sizeof(priv->asf->asfid2avid));
  priv->s = avformat_alloc_context();
  priv->s->priv_data=priv->asf;
  av_init_packet(&priv->avpkt);
  priv->avpkt.data=NULL;
  priv->avpkt.size=0;
  priv->st=NULL;
  priv->asf_st=NULL;
  priv->ctx=NULL;

  for (i=0; i<128; i++) dar[i].num=dar[i].den=0;

  for (;;) {
    int64_t gpos=priv->input_position;

    get_guid(priv->fd, &g);
    priv->input_position+=sizeof(lives_asf_guid);

    if (read(priv->fd,buffer,8)<8) {
      fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
      detach_stream(cdata);
      return FALSE;
    }

    priv->input_position+=8;

    gsize = get_le64int(buffer);

    if (!guidcmp(&g, &lives_asf_data_header)) {
      priv->asf->data_object_offset = priv->input_position;
      if (!(priv->asf->hdr.flags & 0x01) && gsize >= 100) {
        priv->asf->data_object_size = gsize - 24;
      } else {
        priv->asf->data_object_size = (uint64_t)-1;
      }
      break;
    }

    if (gsize < 24) {
      fprintf(stderr, "asf_decoder: guid too small in %s\n",cdata->URI);
      detach_stream(cdata);
      return FALSE;
    }

    if (!guidcmp(&g, &lives_asf_file_header)) {
      // tested OK

      get_guid(priv->fd, &priv->asf->hdr.guid);
      priv->input_position+=sizeof(lives_asf_guid);

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;
      priv->asf->hdr.file_size          = get_le64int(buffer);

#ifdef DEBUG
      fprintf(stderr,"hdr says size is %ld\n",priv->asf->hdr.file_size);
#endif

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;
      priv->asf->hdr.create_time        = get_le64int(buffer);

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;
      priv->asf->nb_packets             = get_le64int(buffer);

#ifdef DEBUG
      fprintf(stderr,"hdr says pax is %ld\n",priv->asf->nb_packets);
#endif

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;
      priv->asf->hdr.play_time          = get_le64int(buffer);

#ifdef DEBUG
      fprintf(stderr,"hdr says playtime is %ld\n",priv->asf->hdr.play_time);
#endif

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;
      // seems to be the duration
      priv->asf->hdr.send_time = get_le64int(buffer);

#ifdef DEBUG
      fprintf(stderr,"hdr says send time is %ld\n",priv->asf->hdr.send_time);
#endif

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;
      priv->asf->hdr.preroll            = get_le32int(buffer);

#ifdef DEBUG
      fprintf(stderr,"hdr says preroll is %d\n",priv->asf->hdr.preroll);
#endif

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;
      priv->asf->hdr.ignore             = get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;
      priv->asf->hdr.flags              = get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;
      priv->asf->hdr.min_pktsize        = get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;
      priv->asf->hdr.max_pktsize        = get_le32int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;
      priv->asf->hdr.max_bitrate        = get_le32int(buffer);

      priv->s->packet_size = priv->asf->hdr.max_pktsize;

#ifdef DEBUG
      fprintf(stderr,"hdr says maxbr is %d\n",priv->asf->hdr.max_bitrate);
#endif

    } else if (!guidcmp(&g, &lives_asf_stream_header)) {
      enum LiVESMediaType type;
      int sizeX;
      //uint64_t total_size;
      unsigned int tag1;
      int64_t pos1, pos2, start_time;
      int test_for_ext_stream_audio;

      pos1 = priv->input_position;

      priv->st = av_new_stream(priv->s, 0);

      if (!priv->st) {
        fprintf(stderr, "asf_decoder: Unable to create new stream for %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      av_set_pts_info(priv->st, 32, 1, 1000); /* 32 bit pts in ms */
      priv->st->codec->extradata_size = 0;

      priv->asf_st = malloc(sizeof(ASFStream));
      memset(priv->asf_st, 0, (sizeof(ASFStream)));

      if (!priv->asf_st) {
        fprintf(stderr, "asf_decoder: Unable to create asf stream for %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->st->priv_data = priv->asf_st;
      start_time = priv->asf->hdr.preroll;

      priv->asf_st->stream_language_index = 128; // invalid stream index means no language info

      if (!(priv->asf->hdr.flags & 0x01)) { // if we aren't streaming...
        priv->st->duration = priv->asf->hdr.play_time /
                             (10000000 / 1000) - start_time;
      }

      get_guid(priv->fd, &g);
      priv->input_position+=sizeof(lives_asf_guid);

      test_for_ext_stream_audio = 0;
      if (!guidcmp(&g, &lives_asf_audio_stream)) {
        type = LIVES_MEDIA_TYPE_AUDIO;
      } else if (!guidcmp(&g, &lives_asf_video_stream)) {
        type = LIVES_MEDIA_TYPE_VIDEO;
      } else if (!guidcmp(&g, &lives_asf_command_stream)) {
        type = LIVES_MEDIA_TYPE_DATA;
      } else if (!guidcmp(&g, &lives_asf_ext_stream_embed_stream_header)) {
        test_for_ext_stream_audio = 1;
        type = LIVES_MEDIA_TYPE_UNKNOWN;
      } else {
        return -1;
      }

      get_guid(priv->fd, &g);
      priv->input_position+=sizeof(lives_asf_guid);

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;

      //      total_size = get_le64int(buffer);

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      //type_specific_size = get_le32int(buffer);

      lseek(priv->fd, 4, SEEK_CUR);
      priv->input_position+=4;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      priv->st->id = get_le16int(buffer) & 0x7f; /* stream id */
      // mapping of asf ID to AV stream ID;
      priv->asf->asfid2avid[priv->st->id] = priv->s->nb_streams - 1;

      lseek(priv->fd, 4, SEEK_CUR);
      priv->input_position+=4;

      if (test_for_ext_stream_audio) {
        get_guid(priv->fd, &g);
        priv->input_position+=sizeof(lives_asf_guid);

        if (!guidcmp(&g, &lives_asf_ext_stream_audio_stream)) {
          type = LIVES_MEDIA_TYPE_AUDIO;
          //is_dvr_ms_audio=1;
          get_guid(priv->fd, &g);
          priv->input_position+=sizeof(lives_asf_guid);

          lseek(priv->fd, 12, SEEK_CUR);
          priv->input_position+=12;

          get_guid(priv->fd, &g);
          priv->input_position+=sizeof(lives_asf_guid);

          lseek(priv->fd, 4, SEEK_CUR);
          priv->input_position+=4;
        }
      }


      if (type == LIVES_MEDIA_TYPE_AUDIO) {
        priv->st->codec->codec_type = AVMEDIA_TYPE_AUDIO;

      } else if (type == LIVES_MEDIA_TYPE_VIDEO) {
        priv->st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
        if (vidindex!=-1&&vidindex!=priv->st->id) {
          fprintf(stderr, "asf_decoder: unhandled multiple vidstreams %d and %d in %s\n",vidindex,priv->st->id,cdata->URI);
          got_vidst=TRUE;
          detach_stream(cdata);
          return FALSE;
        } else {
          vidst=priv->st;
          vidindex=vidst->id;
          priv->asf->stream_index=priv->asf->asfid2avid[vidindex];
        }

        lseek(priv->fd, 9, SEEK_CUR);
        priv->input_position+=9;

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        size = get_le16int(buffer); /* size */

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;
        sizeX= get_le32int(buffer); /* size */

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;

        if (!got_vidst) cdata->width = priv->st->codec->width = get_le32int(buffer);

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;
        if (!got_vidst) cdata->height = priv->st->codec->height = get_le32int(buffer);

        /* not available for asf */
        lseek(priv->fd, 2, SEEK_CUR); // panes
        priv->input_position+=2;


        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        if (!got_vidst) priv->st->codec->bits_per_coded_sample = get_le16int(buffer); /* depth */

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;
        tag1 = get_le32int(buffer);

        lseek(priv->fd, 20, SEEK_CUR);
        priv->input_position+=20;

        size= sizeX;

        if (size > 40) {
          if (!got_vidst) {
            priv->st->codec->extradata_size = size - 40;

            priv->st->codec->extradata = malloc(priv->st->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
            memset(priv->st->codec->extradata, 0, priv->st->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);

            if (read(priv->fd,priv->st->codec->extradata,
                     priv->st->codec->extradata_size)<priv->st->codec->extradata_size) {
              fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
              detach_stream(cdata);
              return FALSE;
            }
          } else lseek(priv->fd,size - 40, SEEK_CUR);
          priv->input_position+=priv->st->codec->extradata_size;

        }

        /* Extract palette from extradata if bpp <= 8 */
        /* This code assumes that extradata contains only palette */
        /* This is true for all paletted codecs implemented in ffmpeg */

        if (!got_vidst) {
          if (priv->st->codec->extradata_size && (priv->st->codec->bits_per_coded_sample <= 8)) {


#if !defined (IS_MINGW) && !defined (IS_SOLARIS) && !defined (__FreeBSD__)
# if __BYTE_ORDER == __BIG_ENDIAN
            int i;
            for (i = 0; i < FFMIN(priv->st->codec->extradata_size, AVPALETTE_SIZE)/4; i++)
              priv->asf_st->palette[i] = av_bswap32(((uint32_t *)priv->st->codec->extradata)[i]);
#else
            memcpy(priv->asf_st->palette, priv->st->codec->extradata,
                   FFMIN(priv->st->codec->extradata_size, AVPALETTE_SIZE));
#endif
#else
            memcpy(priv->asf_st->palette, priv->st->codec->extradata,
                   FFMIN(priv->st->codec->extradata_size, AVPALETTE_SIZE));
#endif

            priv->asf_st->palette_changed = 1;
          }

          priv->st->codec->codec_tag = tag1;
          priv->st->codec->codec_id = ff_codec_get_id((const AVCodecTag *)(codec_bmp_tags), tag1);

          if (tag1 == MK_FOURCC('D', 'V', 'R', ' '))
            priv->st->need_parsing = AVSTREAM_PARSE_FULL;
        }
      }

      pos2 = priv->input_position;
      gsize -= (pos2 - pos1 + 24);
      lseek(priv->fd, gsize, SEEK_CUR);
      priv->input_position+=gsize;
      gsize += (pos2 - pos1 + 24);

    } else if (!guidcmp(&g, &lives_asf_comment_header)) {
      int len1, len2, len3, len4, len5;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      len1 = get_le16int(buffer);

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      len2 = get_le16int(buffer);

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      len3 = get_le16int(buffer);
      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      len4 = get_le16int(buffer);

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      len5 = get_le16int(buffer);

      /*    get_tag(s, "title"    , 0, len1);
      get_tag(s, "author"   , 0, len2);
      get_tag(s, "copyright", 0, len3);
      get_tag(s, "comment"  , 0, len4); */

      lseek(priv->fd, len1+len2+len3+len4+len5, SEEK_CUR);
      priv->input_position+=len1+len2+len3+len4+len5;

#ifdef DEBUG
      fprintf(stderr,"skipping fwd %d bytes\n",len1+len2+len3+len4+len5);
#endif

    } else if (!guidcmp(&g, &stream_bitrate_guid)) {
      int stream_count;
      int j;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      stream_count = get_le16int(buffer);

      for (j = 0; j < stream_count; j++) {
        int flags, bitrate, stream_id;

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        flags = get_le16int(buffer);

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;
        bitrate= get_le32int(buffer);
        stream_id= (flags & 0x7f);
        priv->asf->stream_bitrates[stream_id]= bitrate;
      }
    } else if (!guidcmp(&g, &lives_asf_language_guid)) {
      int j;
      int stream_count = get_le16int(buffer);
      for (j = 0; j < stream_count; j++) {
        char lang[6];
        unsigned int lang_len;

        if (read(priv->fd,buffer,1)<1) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=1;

        lang_len = buffer[0];

        if (read(priv->fd,buffer,lang_len)<lang_len) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        get_str16_nolen(buffer, lang_len, lang, sizeof(lang));
        priv->input_position+=lang_len;

        if (j < 128)
          av_strlcpy(priv->asf->stream_languages[j], lang, sizeof(*priv->asf->stream_languages));
      }
    } else if (!guidcmp(&g, &lives_asf_extended_content_header)) {
      int desc_count, i;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      desc_count = get_le16int(buffer);

      for (i=0; i<desc_count; i++) {
        int name_len,value_type,value_len;
        char name[1024];

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        name_len = get_le16int(buffer);

        if (name_len%2)     // must be even, broken lavf versions wrote len-1
          name_len += 1;

        if (read(priv->fd,buffer,name_len)<name_len) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        get_str16_nolen(buffer, name_len, name, sizeof(name));

        priv->input_position+=name_len;

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        value_type = get_le16int(buffer);

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        value_len  = get_le16int(buffer);
        if (!value_type && value_len%2)
          value_len += 1;
        if (!get_tag(cdata, priv->s, name, value_type, value_len)) return FALSE;
      }

    } else if (!guidcmp(&g, &lives_asf_metadata_header)) {
      int n, stream_num, name_len, value_len, value_num;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      n = get_le16int(buffer);

      for (i=0; i<n; i++) {
        char name[1024];

        lseek(priv->fd, 2, SEEK_CUR); // lang_list_index
        priv->input_position+=2;

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        stream_num= get_le16int(buffer);

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        name_len=   get_le16int(buffer);

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        //value_type= get_le16int(buffer);

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;

        value_len=  get_le32int(buffer);

        if (read(priv->fd,buffer,name_len)<name_len) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        get_str16_nolen(buffer, name_len, name, sizeof(name));

        priv->input_position+=name_len;

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        value_num= get_le16int(buffer);

        // check this
        priv->input_position+=value_len-2;
        lseek(priv->fd, value_len+2, SEEK_CUR);


        if (stream_num<128) {
          if (!strcmp(name, "AspectRatioX")) dar[stream_num].num=
              value_num;
          else if (!strcmp(name, "AspectRatioY")) dar[stream_num].den=
              value_num;

          // i think PAR == 1/DAR (gf)
          if (cdata->par==1.&&dar[stream_num].num!=0&&dar[stream_num].den!=0)
            cdata->par=(float)dar[stream_num].den/(float)dar[stream_num].num;

        }
      }
    } else if (!guidcmp(&g, &lives_asf_ext_stream_header)) {
      int ext_len, payload_ext_ct, stream_ct;
      uint32_t leak_rate, stream_num;
      unsigned int stream_languageid_index;

      lseek(priv->fd, 16, SEEK_CUR); // startime, endtime
      priv->input_position+=16;

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      leak_rate = get_le32int(buffer); // leak-datarate

      /*            get_le32int(buffer); // bucket-datasize
        get_le32int(buffer); // init-bucket-fullness
        get_le32int(buffer); // alt-leak-datarate
        get_le32int(buffer); // alt-bucket-datasize
        get_le32int(buffer); // alt-init-bucket-fullness
        get_le32int(buffer); // max-object-size
      */

      lseek(priv->fd, 20, SEEK_CUR);
      priv->input_position+=20;

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      flags = get_le32int(buffer); // flags (reliable,seekable,no_cleanpoints?,resend-live-cleanpoints, rest of bits reserved)

      if (!(flags&0x02)) {
#ifndef RELEASE
        fprintf(stderr, "asf_decoder: NON-SEEKABLE FILE, falling back to default open.\n");
#endif
        detach_stream(cdata);
        return FALSE;
      }

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      stream_num = get_le16int(buffer); // stream-num

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      stream_languageid_index = get_le16int(buffer); // stream-language-id-index
      if (stream_num < 128)
        priv->asf->streams[stream_num].stream_language_index = stream_languageid_index;

      if (read(priv->fd,buffer,8)<8) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=8;

      ftime = get_le64int(buffer); // avg frametime in 100ns units

      if (cdata->fps==0.) cdata->fps = (double)100000000./(double)ftime;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;
      stream_ct = get_le16int(buffer); //stream-name-count

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;
      payload_ext_ct = get_le16int(buffer); //payload-extension-system-count

      if (stream_num < 128)
        bitrate[stream_num] = leak_rate;

      for (i=0; i<stream_ct; i++) {
        lseek(priv->fd, 2, SEEK_CUR);
        priv->input_position+=2;

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;
        ext_len = get_le16int(buffer);

        lseek(priv->fd, ext_len, SEEK_CUR);
        priv->input_position+=ext_len;
      }

      for (i=0; i<payload_ext_ct; i++) {
        get_guid(priv->fd, &g);
        priv->input_position+=sizeof(lives_asf_guid);

        if (read(priv->fd,buffer,2)<2) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=2;

        //ext_d=get_le16int(buffer);

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;

        ext_len=get_le32int(buffer);

        lseek(priv->fd, ext_len, SEEK_CUR);
      }

      // there could be a optional stream properties object to follow
      // if so the next iteration will pick it up
    } else if (!guidcmp(&g, &lives_asf_head1_guid)) {
      get_guid(priv->fd, &g);
      priv->input_position+=sizeof(lives_asf_guid);
      lseek(priv->fd, 6, SEEK_CUR);
      priv->input_position+=6;
    } else if (!guidcmp(&g, &lives_asf_marker_header)) {
      int i, count, name_len;
      char name[1024];

#if 1
      fprintf(stderr, "asf_decoder: Chapter handling not yet implemented for %s\n",cdata->URI);
      detach_stream(cdata);
      return FALSE;
#endif

      lseek(priv->fd, 16, SEEK_CUR);
      priv->input_position+=16;

      if (read(priv->fd,buffer,4)<4) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=4;

      cdata->nclips = count = get_le32int(buffer);    // markers count

      lseek(priv->fd, 2, SEEK_CUR);
      priv->input_position+=2;

      if (read(priv->fd,buffer,2)<2) {
        fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
        detach_stream(cdata);
        return FALSE;
      }

      priv->input_position+=2;

      name_len = get_le16int(buffer); // name length
      lseek(priv->fd, name_len, SEEK_CUR);
      priv->input_position+=name_len;

      for (i=0; i<count; i++) {
        //	int64_t pres_time;
        int name_len;

        lseek(priv->fd, 8, SEEK_CUR);
        priv->input_position+=8;

        if (read(priv->fd,buffer,8)<8) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=8;

        //pres_time = get_le64int(buffer); // presentation time

        lseek(priv->fd, 10, SEEK_CUR);
        priv->input_position+=10;

        if (read(priv->fd,buffer,4)<4) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        priv->input_position+=4;

        name_len = get_le32int(buffer);  // name length

        if (read(priv->fd,buffer,name_len)<name_len) {
          fprintf(stderr, "asf_decoder: read error in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }

        get_str16_nolen(buffer, name_len * 2, name, sizeof(name));
        //ff_new_chapter(s, i, (AVRational){1, 10000000}, pres_time, AV_NOPTS_VALUE, name );
        priv->input_position+=name_len;
      }


    } else if (check_eof(cdata)) {
      fprintf(stderr, "asf_decoder: EOF in %s\n",cdata->URI);
      detach_stream(cdata);
      return FALSE;
    } else {
      if (!priv->s->keylen) {
        if (!guidcmp(&g, &lives_asf_content_encryption)||
            !guidcmp(&g, &lives_asf_ext_content_encryption) ||
            !guidcmp(&g, &lives_asf_digital_signature)) {
          fprintf(stderr, "asf_decoder: encrypted/signed content in %s\n",cdata->URI);
          detach_stream(cdata);
          return FALSE;
        }
      }
    }

    lseek(priv->fd, gpos + gsize, SEEK_SET);
    if (priv->input_position != gpos + gsize)
#ifdef DEBUG
      fprintf(stderr, "gpos mismatch our pos=%"PRIu64", end=%"PRIu64"\n", priv->input_position-gpos, gsize);
#endif
    priv->input_position=gpos+gsize;
  } // end for


  if (vidindex==-1) {
    fprintf(stderr, "asf_decoder: no video stream found in %s\n",cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  get_guid(priv->fd, &g);
  priv->input_position+=sizeof(lives_asf_guid);

  lseek(priv->fd, 10, SEEK_CUR);
  priv->input_position+=10;

  if (check_eof(cdata)) {
    fprintf(stderr, "asf_decoder: EOF in %s\n",cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  priv->data_start = priv->input_position;
  priv->asf->packet_size_left = 0;

  for (i=0; i<128; i++) {
    int stream_num= priv->asf->asfid2avid[i];
    if (stream_num>=0) {
      AVStream *st = priv->s->streams[stream_num];
      if (!st||!priv->st->codec) continue;
      if (!priv->st->codec->bit_rate)
        priv->st->codec->bit_rate = bitrate[i];
      if (dar[i].num > 0 && dar[i].den > 0)
        av_reduce(&priv->st->sample_aspect_ratio.num,
                  &priv->st->sample_aspect_ratio.den,
                  dar[i].num, dar[i].den, INT_MAX);

      // copy and convert language codes to the frontend
      /*      if (priv->asf->streams[i].stream_language_index < 128) {
        const char *rfc1766 = priv->asf->stream_languages[priv->asf->streams[i].stream_language_index];
        if (rfc1766 && strlen(rfc1766) > 1) {
        const char primary_tag[3] = { rfc1766[0], rfc1766[1], '\0' }; // ignore country code if any
        const char *iso6392 = av_convert_lang_to(primary_tag, AV_LANG_ISO639_2_BIBL);
        if (iso6392)
        av_dict_set(&priv->stmetadata, "language", iso6392, 0);
        }
        }*/
    }
  }


  priv->inited=TRUE;

  priv->data_start=priv->input_position;

  priv->start_dts=0; // will reset this later
  priv->have_start_dts=FALSE; // cannot add keyframes until we know offset of start_dts

  // re-scan with avcodec; priv->data_start holds video data start position
  priv->input_position=priv->data_start;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  priv->ctx=ctx=vidst->codec;

  if (vidst->codec) codec = avcodec_find_decoder(vidst->codec->codec_id);

  if (isclone) {
    retval=avcodec_open2(vidst->codec, codec, NULL);
    return TRUE;
  }

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


#ifdef DEBUG
  fprintf(stderr,"video type is %s %d x %d (%d x %d +%d +%d)\n",cdata->video_name,
          cdata->width,cdata->height,cdata->frame_width,cdata->frame_height,cdata->offs_x,cdata->offs_y);
#endif


  if (!vidst->codec) {
    if (strlen(cdata->video_name)>0)
      fprintf(stderr, "asf_decoder: Could not find avcodec codec for video type %s\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }


  if ((retval=avcodec_open2(vidst->codec, codec, NULL)) < 0) {
    fprintf(stderr, "asf_decoder: Could not open avcodec context (%d) %d\n",retval,vidst->codec->frame_number);
    detach_stream(cdata);
    return FALSE;
  }


  priv->input_position=priv->data_start;
  asf_reset_header(priv->s);

  pthread_mutex_lock(&priv->idxc->mutex);
  priv->kframe=add_keyframe(cdata,priv->data_start,0,0);
  pthread_mutex_unlock(&priv->idxc->mutex);
  priv->def_packet_size=priv->asf->hdr.max_pktsize*10;

  priv->picture = av_frame_alloc();

  do {
    if (priv->avpkt.data!=NULL) free(priv->avpkt.data);
    retval = get_next_video_packet(cdata,0,-1);
    if (retval==-2) {
      fprintf(stderr, "asf_decoder: No frames found.\n");
      detach_stream(cdata);
      return FALSE; // eof
    }
  } while (retval<0);

  if (priv->asf->packet_time_delta!=0) {
    fprintf(stderr, "asf_decoder: packet time delta of (%d) not understood\n",priv->asf->packet_time_delta);
    detach_stream(cdata);
    return FALSE;
  }

#if LIBAVCODEC_VERSION_MAJOR >= 52
  len=avcodec_decode_video2(vidst->codec, priv->picture, &got_picture, &priv->avpkt);
#else
  len=avcodec_decode_video(vidst->codec, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif
  priv->avpkt.size-=len;

  if (!got_picture) {
    fprintf(stderr, "asf_decoder: Did not get picture.\n");
    detach_stream(cdata);
    return FALSE;
  }

  if (priv->avpkt.size!=0) {
    fprintf(stderr, "asf_decoder: Multi frame packets, not decoding.\n");
    detach_stream(cdata);
    return FALSE;
  }

  pts=priv->start_dts=priv->frame_dts;
  priv->have_start_dts=TRUE;
  cdata->video_start_time=(double)pts/10000.;

  cdata->sync_hint=0;
  //#define DEBUG
#ifdef DEBUG
  printf("first pts is %ld\n",pts);
#endif

  if (pts==AV_NOPTS_VALUE) {
    fprintf(stderr, "asf_decoder: No timestamps for frames, not decoding.\n");
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
          fprintf(stderr, "asf_decoder: No timestamp for second frame, aborting.\n");
          detach_stream(cdata);
          return FALSE;
        }

        if (priv->asf->packet_time_delta!=0) {
          fprintf(stderr, "asf_decoder: packet time delta of (%d) not understood\n",priv->asf->packet_time_delta);
          detach_stream(cdata);
          return FALSE;
        }

        pts=priv->frame_dts-pts;
        pts2=priv->frame_dts;

#ifdef DEBUG
        printf("delta pts is %ld %ld\n",pts,priv->frame_dts);
#endif

        if (pts>0) cdata->fps=1000./(double)pts;
        gotframe2=TRUE;
      }
    }
  } while (retval<0&&retval!=-2);


  if (gotframe2) {
    do {
      free(priv->avpkt.data);
      retval = get_next_video_packet(cdata,-1,-1);

      // try to get dts of second frame
      if (retval!=-2) { // eof
        if (retval==0) {

          if (priv->frame_dts==AV_NOPTS_VALUE) {
            break;
          }

          if (priv->asf->packet_time_delta!=0) {
            fprintf(stderr, "asf_decoder: packet time delta of (%d) not understood\n",priv->asf->packet_time_delta);
            detach_stream(cdata);
            return FALSE;
          }

          if (priv->frame_dts-pts2!=pts) {
            // 2->3 delta can be more accurate than 1 - 2 delta
            pts=priv->frame_dts-pts2;
            if (pts>0) cdata->fps=1000./(double)pts;
            priv->start_dts=pts2-pts;
            priv->have_start_dts=TRUE;
            cdata->video_start_time=(double)priv->start_dts/10000.;
          }

#ifdef DEBUG
          printf("3delta pts is %ld %ld\n",pts,priv->frame_dts);
#endif

        }
      }
    } while (retval<0&&retval!=-2);
  }

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


  cdata->palettes[0]=avi_pix_fmt_to_weed_palette(ctx->pix_fmt,
                     &cdata->YUV_clamping);

  if (cdata->palettes[0]==WEED_PALETTE_END) {
    fprintf(stderr, "asf_decoder: Could not find a usable palette for (%d) %s\n",ctx->pix_fmt,cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  cdata->current_palette=cdata->palettes[0];

  // re-get fps, width, height, nframes - actually avcodec is pretty useless at getting this
  // so we fall back on the values we obtained ourselves

  if (cdata->width==0) cdata->width=ctx->width-cdata->offs_x*2;
  if (cdata->height==0) cdata->height=ctx->height-cdata->offs_y*2;

  if (cdata->width*cdata->height==0) {
    fprintf(stderr, "asf_decoder: invalid width and height (%d X %d)\n",cdata->width,cdata->height);
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


#ifndef IS_MINGW

  if (cdata->fps==0.||cdata->fps==1000.) {
    // use mplayer to get fps if we can...it seems to have some magical way
    char cmd[1024];
    char tmpfname[32];
    int ofd;

    sprintf(tmpfname,"/tmp/mkvdec=XXXXXX");
    ofd=mkstemp(tmpfname);
    if (ofd!=-1) {
      int res;
      snprintf(cmd,1024,"LANGUAGE=en LANG=en mplayer \"%s\" -identify -frames 0 2>/dev/null | grep ID_VIDEO_FPS > %s",cdata->URI,tmpfname);
      res=system(cmd);

      if (res) {
        snprintf(cmd,1024,"LANGUAGE=en LANG=en mplayer2 \"%s\" -identify -frames 0 2>/dev/null | grep ID_VIDEO_FPS > %s",cdata->URI,tmpfname);
        res=system(cmd);
      }

      if (!res) {
        char buffer[1024];
        ssize_t bytes=read(ofd,buffer,1024);
        memset(buffer+bytes,0,1);
        if (!(strncmp(buffer,"ID_VIDEO_FPS=",13))) {
          cdata->fps=strtod(buffer+13,NULL);
        }
      }

      close(ofd);
      unlink(tmpfname);
    }
  }

#endif

  if (cdata->fps==0.&&ctx->time_base.num==0) {
    if (ctx->time_base.den==1) cdata->fps=25.;
  }

  if (cdata->fps==0.||cdata->fps==1000.) {
    fprintf(stderr, "asf_decoder: invalid framerate %.4f (%d / %d)\n",cdata->fps,ctx->time_base.den,ctx->time_base.num);
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

  if (is_partial_clone) return TRUE;

  vidst->duration = get_last_video_dts(cdata);

  cdata->nframes = dts_to_frame(cdata,vidst->duration+priv->start_dts)+2;

  // double check, sometimes we can be out by one or two frames
  while (1) {
    if (get_frame(cdata,cdata->nframes-1,NULL,0,NULL)) break;
    cdata->nframes--;
  }

  if (cdata->width!=cdata->frame_width||cdata->height!=cdata->frame_height)
    fprintf(stderr,"asf_decoder: info frame size=%d x %d, pixel size=%d x %d\n",cdata->frame_width,cdata->frame_height,cdata->width,
            cdata->height);


  return TRUE;
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  avcodec_register_all();
  indices=NULL;
  nidxc=0;

  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&indices_mutex,NULL);
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata(void) {
  lives_asf_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->URI=NULL;

  cdata->priv=priv=malloc(sizeof(lives_asf_priv_t));

  cdata->seek_flag=0;

  priv->ctx=NULL;
  priv->picture=NULL;

  cdata->palettes=(int *)malloc(2*sizeof(int));
  cdata->palettes[1]=WEED_PALETTE_END;

  priv->idxc=NULL;

  priv->inited=FALSE;

  cdata->sync_hint=0;

  cdata->video_start_time=0.;

  memset(cdata->author,0,1);
  memset(cdata->title,0,1);
  memset(cdata->comment,0,1);

  return cdata;
}


static lives_clip_data_t *asf_clone(lives_clip_data_t *cdata) {
  lives_clip_data_t *clone=init_cdata();
  lives_asf_priv_t *dpriv,*spriv;

  // copy from cdata to clone, with a new context for clone
  clone->URI=strdup(cdata->URI);
  clone->nclips=cdata->nclips;
  snprintf(clone->container_name,512,"%s",cdata->container_name);
  clone->current_clip=cdata->current_clip;
  clone->width=cdata->width;
  clone->height=cdata->height;
  clone->nframes=cdata->nframes;
  clone->interlace=cdata->interlace;
  clone->offs_x=cdata->offs_x;
  clone->offs_y=cdata->offs_y;
  clone->frame_width=cdata->frame_width;
  clone->frame_height=cdata->frame_height;
  clone->par=cdata->par;
  clone->fps=cdata->fps;
  if (cdata->palettes!=NULL) clone->palettes[0]=cdata->palettes[0];
  clone->current_palette=cdata->current_palette;
  clone->YUV_sampling=cdata->YUV_sampling;
  clone->YUV_clamping=cdata->YUV_clamping;
  snprintf(clone->video_name,512,"%s",cdata->video_name);
  clone->arate=cdata->arate;
  clone->achans=cdata->achans;
  clone->asamps=cdata->asamps;
  clone->asigned=cdata->asigned;
  clone->ainterleaf=cdata->ainterleaf;
  snprintf(clone->audio_name,512,"%s",cdata->audio_name);
  clone->seek_flag=cdata->seek_flag;
  clone->sync_hint=cdata->sync_hint;

  snprintf(clone->author,256,"%s",cdata->author);
  snprintf(clone->title,256,"%s",cdata->title);
  snprintf(clone->comment,256,"%s",cdata->comment);

  // create "priv" elements
  dpriv=clone->priv;
  spriv=cdata->priv;

  if (spriv!=NULL) {
    dpriv->filesize=spriv->filesize;
    dpriv->inited=TRUE;
  }

  if (!attach_stream(clone,TRUE)) {
    free(clone->URI);
    clone->URI=NULL;
    clip_data_free(clone);
    return NULL;
  }

  asf_reset_header(dpriv->s);

  if (spriv!=NULL) {
    dpriv->def_packet_size=spriv->def_packet_size;
    dpriv->start_dts=spriv->start_dts;
    dpriv->have_start_dts=TRUE;
    dpriv->st->duration=spriv->st->duration;

    dpriv->last_frame=-1;
    dpriv->black_fill=FALSE;
  } else {
    clone->nclips=1;

    ///////////////////////////////////////////////////////////

    sprintf(clone->container_name,"%s","asf");

    // clone->height was set when we attached the stream

    clone->interlace=LIVES_INTERLACE_NONE;

    clone->frame_width=clone->width+clone->offs_x*2;
    clone->frame_height=clone->height+clone->offs_y*2;

    if (dpriv->ctx->width==clone->frame_width) clone->offs_x=0;
    if (dpriv->ctx->height==clone->frame_height) clone->offs_y=0;

    ////////////////////////////////////////////////////////////////////

    clone->asigned=TRUE;
    clone->ainterleaf=TRUE;

  }

  if (dpriv->picture!=NULL) av_frame_free(&dpriv->picture);
  dpriv->picture=NULL;

  return clone;
}



lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different current_clip, this must be called again with the same
  // cdata as the second parameter

  // value returned should be freed with clip_data_free() when no longer required

  // should be thread-safe

  lives_asf_priv_t *priv;

  if (URI==NULL&&cdata!=NULL) {
    // create a clone of cdata - we also need to be able to handle a "fake" clone with only URI, nframes and fps set (priv == NULL)
    return asf_clone(cdata);
  }

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
    if (!attach_stream(cdata,FALSE)) {
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

  sprintf(cdata->container_name,"%s","asf");

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

  if (priv->picture!=NULL) av_frame_free(&priv->picture);
  priv->picture=NULL;

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

  lives_asf_priv_t *priv=cdata->priv;

  int64_t nextframe=0;
  int64_t target_pts=frame_to_dts(cdata,tframe);

  unsigned char *dst,*src;
  unsigned char black[4]= {0,0,0,255};

  boolean got_picture=FALSE;
  boolean hit_target=FALSE;

  int tfrag=0;
  int xheight=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;
  int rescan_limit=16;  // pick some arbitrary value
  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;

  register int i,p;

#ifdef DEBUG_KFRAMES
  fprintf(stderr,"vals %ld %ld\n",tframe,priv->last_frame);
#endif

  if (pixel_data!=NULL) {

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

    if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_UYVY8888||
        pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) psize=4;

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
  }

  ////////////////////////////////////////////////////////////////////

  if (tframe!=priv->last_frame) {

    if (priv->picture!=NULL) av_frame_free(&priv->picture);
    priv->picture=NULL;

    if (priv->last_frame==-1 || (tframe<priv->last_frame) || (tframe - priv->last_frame > rescan_limit)) {

      if (priv->avpkt.data!=NULL) free(priv->avpkt.data);
      priv->avpkt.data=NULL;
      priv->avpkt.size=0;

      pthread_mutex_lock(&priv->idxc->mutex);
      if ((priv->kframe=get_idx_for_pts(cdata,target_pts-priv->start_dts))!=NULL) {
        priv->input_position=priv->kframe->offs;
        nextframe=dts_to_frame(cdata,priv->kframe->dts+priv->start_dts);
        tfrag=priv->kframe->frag;
      } else priv->input_position=priv->data_start;
      pthread_mutex_unlock(&priv->idxc->mutex);

      // we are now at the kframe before or at target - parse packets until we hit target

      //#define DEBUG_KFRAMES
#ifdef DEBUG_KFRAMES
      if (priv->kframe!=NULL) printf("got kframe %ld frag %d for frame %ld\n",
                                       dts_to_frame(cdata,priv->kframe->dts+priv->start_dts),priv->kframe->frag,tframe);
#endif

      avcodec_flush_buffers(priv->ctx);
      asf_reset_header(priv->s);
      priv->black_fill=FALSE;
    } else {
      nextframe=priv->last_frame+1;
      tfrag=-1;
    }

    //priv->ctx->skip_frame=AVDISCARD_NONREF;

    priv->last_frame=tframe;

    // do this until we reach target frame //////////////


    while (1) {
      int ret;

      if (priv->avpkt.size==0) {
        ret = get_next_video_packet(cdata,tfrag,-1);
        if (ret<0) {
          free(priv->avpkt.data);
          priv->avpkt.data=NULL;
          priv->avpkt.size=0;
          if (ret==-2) {
            // EOF
            if (pixel_data==NULL) return FALSE;
            priv->black_fill=TRUE;
            break;
          }
          fprintf(stderr,"asf_decoder: got frame decode error %d at frame %ld\n",ret,nextframe+1);
          continue;
        }
      }


      // decode any frames from this packet
      if (priv->picture==NULL) priv->picture=av_frame_alloc();

#if LIBAVCODEC_VERSION_MAJOR >= 53
      avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
      avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

      free(priv->avpkt.data);
      priv->avpkt.data=NULL;
      priv->avpkt.size=0;

      if (nextframe==tframe) hit_target=TRUE;

      if (hit_target&&got_picture) break;

      // otherwise discard this frame
      if (got_picture) {
        av_frame_free(&priv->picture);
        priv->picture=NULL;
        tfrag=-1;
        nextframe++;
      }
    }
  }


  if (priv->picture==NULL||pixel_data==NULL) return TRUE;

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
  lives_asf_priv_t *priv=cdata->priv;

  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;

  if (priv->idxc!=NULL)
    idxc_release(cdata);

  priv->idxc=NULL;

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  free(cdata->priv);
  free(cdata);
}


void module_unload(void) {
  idxc_release_all();
}




