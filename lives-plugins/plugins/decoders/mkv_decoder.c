// LiVES - mkv decoder plugin
// (c) G. Finch 2011 <salsaman@xs4all.nl>

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



/**
 * @file
 * Matroska file demuxer
 * by Ronald Bultje <rbultje@ronald.bitfreak.net>
 * with a little help from Moritz Bunkus <moritz@bunkus.org>
 * totally reworked by Aurelien Jacobs <aurel@gnuage.org>
 * Specs available on the Matroska project page: http://www.matroska.org/.
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

const char *plugin_version="LiVES mkv decoder version 1.0";

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

#define HAVE_AVCODEC

#ifdef HAVE_SYSTEM_WEED_COMPAT
#include "weed/weed-compat.h"
#else
#include "../../../libweed/weed-compat.h"
#endif

#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/mem.h>

#include "decplugin.h"

#include "mkv_decoder.h"

//#include "mkv_decoder.h"

#include "libavutil/intfloat_readwrite.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/avstring.h"
#include "libavutil/lzo.h"
#include "libavutil/dict.h"
#if CONFIG_ZLIB
#include <zlib.h>
#endif
#if CONFIG_BZLIB
#include <bzlib.h>
#endif

////////////////////////////////////////////////////////////////////////////


const uint8_t ff_log2_tab[256]={
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};



static enum CodecID ff_codec_get_id(const AVCodecTag *tags, unsigned int tag)
{
  int i;
  for(i=0; tags[i].id != CODEC_ID_NONE;i++) {
    if(tag == tags[i].tag)
      return tags[i].id;
  }
  for(i=0; tags[i].id != CODEC_ID_NONE; i++) {
    if(   toupper((tag >> 0)&0xFF) == toupper((tags[i].tag >> 0)&0xFF)
	  && toupper((tag >> 8)&0xFF) == toupper((tags[i].tag >> 8)&0xFF)
	  && toupper((tag >>16)&0xFF) == toupper((tags[i].tag >>16)&0xFF)
	  && toupper((tag >>24)&0xFF) == toupper((tags[i].tag >>24)&0xFF))
      return tags[i].id;
  }
  return CODEC_ID_NONE;
}


#define MKV_TAG_TYPE_VIDEO 1

boolean got_eof;
int errval;

static const char *matroska_doctypes[] = { "matroska", "webm" };



static boolean check_eof(const lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  if (priv->input_position>=priv->filesize) return TRUE;
  return FALSE;
}


/*
 * Return: Whether we reached the end of a level in the hierarchy or not.
 */
static int ebml_level_end(const lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  int64_t pos = priv->input_position;

  if (matroska->num_levels > 0) {
    MatroskaLevel *level = &matroska->levels[matroska->num_levels - 1];
    if (pos - level->start >= level->length || matroska->current_id) {
      matroska->num_levels--;
      printf("abc1\n");
      return 1;
    }
  }
  return 0;
}

/*
 * Read: an "EBML number", which is defined as a variable-length
 * array of bytes. The first byte indicates the length by giving a
 * number of 0-bits followed by a one. The position of the first
 * "one" bit inside the first byte indicates the length of this
 * number.
 * Returns: number of bytes read, < 0 on error
 */
static int ebml_read_num(const lives_clip_data_t *cdata, uint8_t * data,
                         int max_size, uint64_t *number) {
  lives_mkv_priv_t *priv=cdata->priv;

  int bread = 1, n = 1;
  uint64_t total = 0;
  unsigned char buffer[1];
  uint8_t val8;

  /* The first byte tells us the length in bytes - avio_r8() can normally
   * return 0, but since that's not a valid first ebmlID byte, we can
   * use it safely here to catch EOS. */
  
  if (data==NULL) {
    if (read (priv->fd, buffer, 1) < 1) {
      fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
      got_eof=TRUE;
      return 0;
    }
  
    priv->input_position+=1;
  
    total=*buffer;
  }
  else {
    total=data[0];
  }


  /* get the length of the EBML number */
  bread = 8 - ff_log2_tab[total];

  if (bread > max_size) {
    fprintf(stderr,    
	    "mkv_decoder: Invalid EBML number\n");
    errval=-1;
    return 0;
  }
  
  /* read out length */
  total ^= 1 << ff_log2_tab[total];


  while (n++ < bread) {

    if (data==NULL) {
      if (read (priv->fd, buffer, 1) < 1) {
	fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
	got_eof=TRUE;
	return 0;
      }
      
      priv->input_position+=1;
      
      val8=(uint8_t)*buffer;
    }
    else {
      val8=data[n-1];
    }

    total = (total << 8) | val8;
  }
  *number = total;

  return bread;
}

/**
 * Read a EBML length value.
 * This needs special handling for the "unknown length" case which has multiple
 * encodings.
 */
static int ebml_read_length(const lives_clip_data_t *cdata,
                            uint64_t *number) {
  int res = ebml_read_num(cdata, NULL, 8, number);
  if (res > 0 && *number + 1 == 1ULL << (7 * res))
    *number = 0xffffffffffffffULL;
  return res;
}

/*
 * Read the next element as an unsigned int.
 * 0 is success, < 0 is failure.
 */
static int ebml_read_uint(const lives_clip_data_t *cdata, int size, uint64_t *num) {
  lives_mkv_priv_t *priv=cdata->priv;
  int n = 0;
  uint8_t val8;
  uint8_t buffer[1];

  if (size > 8) {
    errval=-2; // TODO 
    return 0;
  }

  /* big-endian ordering; build up number */
  *num = 0;
  while (n++ < size) {
    if (read (priv->fd, buffer, 1) < 1) {
      fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
      got_eof=TRUE;
      return 0;
    }
    
    priv->input_position+=1;
    
    val8=(uint8_t)*buffer;
    
    *num = (*num << 8) | val8;
  }
  
  return 0;
}

/*
 * Read the next element as a float.
 * 0 is success, < 0 is failure.
 */
static int ebml_read_float(const lives_clip_data_t *cdata, int size, double *num) {
  lives_mkv_priv_t *priv=cdata->priv;
  uint8_t buffer[8];
  uint32_t val32;
  uint64_t val64;

  if (size == 0) {
    *num = 0;
  } else if (size == 4) {
    
    if (read(priv->fd,buffer,4)<4) {
      fprintf(stderr, "mkv_decoder: read error in %s\n",cdata->URI);
      return -10; // EOF TODO **
    }

    priv->input_position+=4;

    val32 = get_le32int(buffer);

    *num= av_int2flt(val32);
  } else if(size==8){

    if (read(priv->fd,buffer,8)<8) {
      fprintf(stderr, "mkv_decoder: read error in %s\n",cdata->URI);
      return -10;
    }

    priv->input_position+=8;

    val64 = get_le64int(buffer);

    *num= av_int2dbl(val64);
  } else
    return -7;//AVERROR_INVALIDDATA; // TODO ***
  
  return 0;
}

/*
 * Read the next element as an ASCII string.
 * 0 is success, < 0 is failure.
 */
static int ebml_read_ascii(const lives_clip_data_t *cdata, int size, char **str) {
  lives_mkv_priv_t *priv=cdata->priv;

  free(*str);
  /* EBML strings are usually not 0-terminated, so we allocate one
   * byte more, read the string and NULL-terminate it ourselves. */
  if (!(*str = malloc(size + 1)))
    return -1;// TODO ** AVERROR(ENOMEM);
  
  if (read (priv->fd, (uint8_t *) *str, size) < size) {
    fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
    av_freep(str);
    got_eof=TRUE;
    return 0;
  }
  
  priv->input_position+=size;

  (*str)[size] = '\0';

  return 0;
}


/*
 * Read the next element as binary data.
 * 0 is success, < 0 is failure.
 */
static int ebml_read_binary(const lives_clip_data_t *cdata, int length, EbmlBin *bin) {
  lives_mkv_priv_t *priv=cdata->priv;
  free(bin->data);

  if (!(bin->data = malloc(length))) {
    errval=-4; // TODO ***
    return 0;
  }
  
  bin->size = length;
  bin->pos  = priv->input_position;
  
  if (read (priv->fd, bin->data, length) < length) {
    fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
    av_freep(&bin->data);
    got_eof=TRUE;
    return 0;
  }

  priv->input_position+=length;
  
  return 0;
}

/*
 * Read the next element, but only the header. The contents
 * are supposed to be sub-elements which can be read separately.
 * 0 is success, < 0 is failure.
 */
static int ebml_read_master(const lives_clip_data_t *cdata, uint64_t length) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  
  MatroskaLevel *level;
  
  if (matroska->num_levels >= EBML_MAX_DEPTH) {
    //av_log(matroska->ctx, AV_LOG_ERROR,
    //       "File moves beyond max. allowed depth (%d)\n", EBML_MAX_DEPTH);
    errval=-3; // TODO **
    return 0;
  }
  
  level = &matroska->levels[matroska->num_levels++];
  level->start = priv->input_position;
  level->length = length;
  
  return 0;
}

/*
 * Read signed/unsigned "EBML" numbers.
 * Return: number of bytes processed, < 0 on error
 */
static int matroska_ebmlnum_uint(const lives_clip_data_t *cdata,
                                 uint8_t *data, uint32_t size, uint64_t *num) {
  return ebml_read_num(cdata, data, FFMIN(size, 8), num);
}

/*
 * Same as above, but signed.
 */
static int matroska_ebmlnum_sint(const lives_clip_data_t *cdata,
                                 uint8_t *data, uint32_t size, int64_t *num) {
  uint64_t unum;
  int res;
  
  /* read as unsigned number first */
  if ((res = matroska_ebmlnum_uint(cdata, data, size, &unum)) < 0)
    return res;
  
  /* make signed (weird way) */
  *num = unum - ((1LL << (7*res - 1)) - 1);
  
  return res;
}

static int ebml_parse_id(const lives_clip_data_t *cdata, EbmlSyntax *syntax,
                         uint32_t id, void *data)
{
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  int reso;
  int i;

  for (i=0; syntax[i].id; i++) {
    if (id == syntax[i].id)
      break;
  }
  if (!syntax[i].id && id == MATROSKA_ID_CLUSTER &&
      matroska->num_levels > 0 &&
      matroska->levels[matroska->num_levels-1].length == 0xffffffffffffff)
    return 0;  // we reached the end of an unknown size cluster
  // if (!syntax[i].id && id != EBML_ID_VOID && id != EBML_ID_CRC32)
    //av_log(matroska->ctx, AV_LOG_INFO, "Unknown entry 0x%X\n", id);

  reso=ebml_parse_elem(cdata, &syntax[i], data);

  return reso;
}

static int ebml_parse(const lives_clip_data_t *cdata, EbmlSyntax *syntax,
                      void *data)
{
  int res;
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  if (!matroska->current_id) {
    uint64_t id;
    
    int res = ebml_read_num(cdata, NULL, 4, &id);

    if (res < 0)
      return res;
    
    matroska->current_id = id | 1 << 7*res;

    printf("got id %08x\n",matroska->current_id);

  }
  res=ebml_parse_id(cdata, syntax, matroska->current_id, data);
  
  return res;
}

static int ebml_parse_nest(const lives_clip_data_t *cdata, EbmlSyntax *syntax,
                           void *data)
{
  int i, res = 0;

  for (i=0; syntax[i].id; i++)
    switch (syntax[i].type) {
    case EBML_UINT:
      *(uint64_t *)((char *)data+syntax[i].data_offset) = syntax[i].def.u;
      break;
    case EBML_FLOAT:
      *(double   *)((char *)data+syntax[i].data_offset) = syntax[i].def.f;
      break;
    case EBML_STR:
    case EBML_UTF8:
      *(char    **)((char *)data+syntax[i].data_offset) = av_strdup(syntax[i].def.s);
      break;
    default:
      break;
    }

  while (!res && !ebml_level_end(cdata))
    res = ebml_parse(cdata, syntax, data);

  return res;
}

static int ebml_parse_elem(const lives_clip_data_t *cdata, EbmlSyntax *syntax, void *data)
{
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  static const uint64_t max_lengths[EBML_TYPE_COUNT] = {
    [EBML_UINT]  = 8,
    [EBML_FLOAT] = 8,
    // max. 16 MB for strings
    [EBML_STR]   = 0x1000000,
    [EBML_UTF8]  = 0x1000000,
    // max. 256 MB for binary data
    [EBML_BIN]   = 0x10000000,
    // no limits for anything else
  };
  uint32_t id = syntax->id;
  uint64_t length;
  int res;
  void *newelem;

  //printf("pt a11 %p %d %08x %08x\n",syntax,syntax->type,id,priv->input_position);

  data = (char *)data + syntax->data_offset;


  if (syntax->list_elem_size) {
    EbmlList *list = data;
    newelem = av_realloc(list->elem, (list->nb_elem+1)*syntax->list_elem_size);
    if (!newelem)
      return AVERROR(ENOMEM);
    list->elem = newelem;
    data = (char*)list->elem + list->nb_elem*syntax->list_elem_size;
    memset(data, 0, syntax->list_elem_size);
    list->nb_elem++;
  }
  if (syntax->type != EBML_PASS && syntax->type != EBML_STOP) {
    matroska->current_id = 0;
    if ((res = ebml_read_length(cdata, &length)) < 0)
      return res;
    if (max_lengths[syntax->type] && length > max_lengths[syntax->type]) {
      //av_log(matroska->ctx, AV_LOG_ERROR,
      //       "Invalid length 0x%"PRIx64" > 0x%"PRIx64" for syntax element %i\n",
      //       length, max_lengths[syntax->type], syntax->type);
      errval=-4; // TODO **
      return 0;//AVERROR_INVALIDDATA;
    }
  }

  switch (syntax->type) {
  case EBML_UINT:  res = ebml_read_uint  (cdata, length, data);  break;
  case EBML_FLOAT: res = ebml_read_float (cdata, length, data);  break;
  case EBML_STR:
  case EBML_UTF8:  res = ebml_read_ascii (cdata, length, data);  break;
  case EBML_BIN:   res = ebml_read_binary(cdata, length, data);  break;
  case EBML_NEST:  if ((res=ebml_read_master(cdata, length)) < 0)
      return res;
    if (id == MATROSKA_ID_SEGMENT)
      matroska->segment_start = priv->input_position;
    return ebml_parse_nest(cdata, syntax->def.n, data);
  case EBML_PASS:  return ebml_parse_id(cdata, syntax->def.n, id, data);
  case EBML_STOP:  return 1;
  default:        
    // skip length bytes
    priv->input_position+=length;
    lseek(priv->fd, priv->input_position, SEEK_SET);
    res=check_eof(cdata);
    return res;
  }

  if (res == -4) //AVERROR_INVALIDDATA)
  //av_log(matroska->ctx, AV_LOG_ERROR, "Invalid element\n");
    return res;
  if (res == -999) //AVERROR(EIO))
    //av_log(matroska->ctx, AV_LOG_ERROR, "Read error\n");
    return res;

  return res;
}

static void ebml_free(EbmlSyntax *syntax, void *data)
{
  int i, j;
  for (i=0; syntax[i].id; i++) {
    void *data_off = (char *)data + syntax[i].data_offset;
    switch (syntax[i].type) {
    case EBML_STR:
    case EBML_UTF8: av_freep(data_off);                      break;
    case EBML_BIN:  av_freep(&((EbmlBin *)data_off)->data);  break;
    case EBML_NEST:
      if (syntax[i].list_elem_size) {
	EbmlList *list = data_off;
	char *ptr = list->elem;
	for (j=0; j<list->nb_elem; j++, ptr+=syntax[i].list_elem_size)
	  ebml_free(syntax[i].def.n, ptr);
	free(list->elem);
      } else
	ebml_free(syntax[i].def.n, data_off);
    default:  break;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////

void ff_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp)
{
  int i;

  for(i = 0; i < s->nb_streams; i++) {
    AVStream *st = s->streams[i];

    st->cur_dts = av_rescale(timestamp,
			     st->time_base.den * (int64_t)ref_st->time_base.num,
			     st->time_base.num * (int64_t)ref_st->time_base.den);
  }
}


/*
 * Autodetecting...
 */
static boolean lives_mkv_probe(const lives_clip_data_t *cdata, unsigned char *p) {
  lives_mkv_priv_t *priv=cdata->priv;

  uint64_t total = 0;
  int len_mask = 0x80, size = 1, n = 1, i;
  unsigned char nextbytes[1024];
  
  /* EBML header? */
  if (AV_RB32(p) != EBML_ID_HEADER) {
    return FALSE;
  }
  
  /* length of header */
  total = p[4];
  
  while (size <= 8 && !(total & len_mask)) {
    size++;
    len_mask >>= 1;
  }
  
  total &= (len_mask - 1);
  
  while (n < size) {
    if (read(priv->fd,nextbytes,1)!=1) {
      return FALSE;
      
    }
    total = (total << 8) | nextbytes[0]; // nextbyte is after p[4]
    n++;
    priv->input_position++;
  }
  
  // read "total" bytes
  
  if (read(priv->fd,nextbytes,total)!=total) {
    return FALSE;
  }
  
  priv->input_position+=total;
  
  /* The header should contain a known document type. For now,
   * we don't parse the whole header but simply check for the
   * availability of that array of characters inside the header.
   * Not fully fool-proof, but good enough. */
  
  for (i = 0; i < 2; i++) {
    int probelen = strlen(matroska_doctypes[i]);
    if (total < probelen)
      continue;
    for (n = 0; n <= total-probelen; n++)
      if (!memcmp(nextbytes + n, matroska_doctypes[i], probelen))
	return TRUE;
  }
  
  // probably valid EBML header but no recognized doctype
  return FALSE;
}



static MatroskaTrack *matroska_find_track_by_num(MatroskaDemuxContext *matroska,
                                                 int num)
{
  MatroskaTrack *tracks = matroska->tracks.elem;
  int i;

  for (i=0; i < matroska->tracks.nb_elem; i++) {
    if (tracks[i].num == num) 
      return &tracks[i];
  }
  //av_log(matroska->ctx, AV_LOG_ERROR, "Invalid track number %d\n", num);
  return NULL;
}




static int matroska_decode_buffer(uint8_t** buf, int* buf_size,
                                  MatroskaTrack *track)
{
  MatroskaTrackEncoding *encodings = track->encodings.elem;
  uint8_t* data = *buf;
  int isize = *buf_size;
  uint8_t* pkt_data = NULL;
  int pkt_size = isize;
  int result = 0;
  int olen;

  if (pkt_size >= 10000000)
    return -1;

  switch (encodings[0].compression.algo) {
  case MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP:
    return encodings[0].compression.settings.size;
  case MATROSKA_TRACK_ENCODING_COMP_LZO:
    do {
      olen = pkt_size *= 3;
      pkt_data = av_realloc(pkt_data, pkt_size+AV_LZO_OUTPUT_PADDING);
      result = av_lzo1x_decode(pkt_data, &olen, data, &isize);
    } while (result==AV_LZO_OUTPUT_FULL && pkt_size<10000000);
    if (result)
      goto failed;
    pkt_size -= olen;
    break;
#if CONFIG_ZLIB
  case MATROSKA_TRACK_ENCODING_COMP_ZLIB: {
    z_stream zstream = {0};
    if (inflateInit(&zstream) != Z_OK)
      return -1;
    zstream.next_in = data;
    zstream.avail_in = isize;
    do {
      pkt_size *= 3;
      newpktdata = av_realloc(pkt_data, pkt_size);
      if (!newpktdata) {
	inflateEnd(&zstream);
	goto failed;
      }
      pkt_data = newpktdata;
      zstream.avail_out = pkt_size - zstream.total_out;
      zstream.next_out = pkt_data + zstream.total_out;
      if (pkt_data) {
	result = inflate(&zstream, Z_NO_FLUSH);
      } else
	result = Z_MEM_ERROR;
    } while (result==Z_OK && pkt_size<10000000);
    pkt_size = zstream.total_out;
    inflateEnd(&zstream);
    if (result != Z_STREAM_END)
      goto failed;
    break;
  }
#endif
#if CONFIG_BZLIB
  case MATROSKA_TRACK_ENCODING_COMP_BZLIB: {
    bz_stream bzstream = {0};
    if (BZ2_bzDecompressInit(&bzstream, 0, 0) != BZ_OK)
      return -1;
    bzstream.next_in = data;
    bzstream.avail_in = isize;
    do {
      pkt_size *= 3;
      newpktdata = av_realloc(pkt_data, pkt_size);
      if (!newpktdata) {
	BZ2_bzDecompressEnd(&bzstream);
	goto failed;
      }
      pkt_data = newpktdata;
      bzstream.avail_out = pkt_size - bzstream.total_out_lo32;
      bzstream.next_out = pkt_data + bzstream.total_out_lo32;
      if (pkt_data) {
	result = BZ2_bzDecompress(&bzstream);
      } else
	result = BZ_MEM_ERROR;
    } while (result==BZ_OK && pkt_size<10000000);
    pkt_size = bzstream.total_out_lo32;
    BZ2_bzDecompressEnd(&bzstream);
    if (result != BZ_STREAM_END)
      goto failed;
    break;
  }
#endif
  default:
    return -1;
  }

  *buf = pkt_data;
  *buf_size = pkt_size;
  return 0;
 failed:
  av_free(pkt_data);
  return -1;
}




static void matroska_fix_ass_packet(MatroskaDemuxContext *matroska,
                                    AVPacket *pkt, uint64_t display_duration)
{
  char *line;
  unsigned char *layer;
  unsigned char *ptr = pkt->data;
  unsigned char *end = ptr+pkt->size;
  for (; *ptr!=',' && ptr<end-1; ptr++);
  if (*ptr == ',')
    ptr++;
  layer = ptr;
  for (; *ptr!=',' && ptr<end-1; ptr++);
  if (*ptr == ',') {
    int64_t end_pts = pkt->pts + display_duration;
    int sc = matroska->time_scale * pkt->pts / 10000000;
    int ec = matroska->time_scale * end_pts  / 10000000;
    int sh, sm, ss, eh, em, es, len;
    sh = sc/360000;  sc -= 360000*sh;
    sm = sc/  6000;  sc -=   6000*sm;
    ss = sc/   100;  sc -=    100*ss;
    eh = ec/360000;  ec -= 360000*eh;
    em = ec/  6000;  ec -=   6000*em;
    es = ec/   100;  ec -=    100*es;
    *ptr++ = '\0';
    len = 50 + end-ptr + FF_INPUT_BUFFER_PADDING_SIZE;
    if (!(line = malloc(len)))
      return;
    snprintf(line,len,"Dialogue: %s,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,%s\r\n",
	     layer, sh, sm, ss, sc, eh, em, es, ec, ptr);
    av_free(pkt->data);
    pkt->data = (unsigned char *)line;
    pkt->size = strlen(line);
  }
}

static int matroska_merge_packets(AVPacket *out, AVPacket *in)
{
  void *newdata = av_realloc(out->data, out->size+in->size);
  if (!newdata)
    return AVERROR(ENOMEM);
  out->data = newdata;
  memcpy(out->data+out->size, in->data, in->size);
  out->size += in->size;
  av_destruct_packet(in);
  av_free(in);
  return 0;
}


static int matroska_parse_seekhead_entry(const lives_clip_data_t *cdata, int idx) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  EbmlList *seekhead_list = &matroska->seekhead;
  MatroskaSeekhead *seekhead = seekhead_list->elem;
  uint32_t level_up = matroska->level_up;
  int64_t before_pos = priv->input_position;
  uint32_t saved_id = matroska->current_id;
  MatroskaLevel level;
  int64_t offset;
  int ret = 0;
  
  if (idx >= seekhead_list->nb_elem
      || seekhead[idx].id == MATROSKA_ID_SEEKHEAD
      || seekhead[idx].id == MATROSKA_ID_CLUSTER)
    return 0;

  /* seek */
  offset = seekhead[idx].pos + matroska->segment_start;

  if (offset>priv->filesize) {
    got_eof=TRUE;
    return 0;
  }

  priv->input_position=offset;
  lseek(priv->fd,priv->input_position,SEEK_SET);
  
  /* We don't want to lose our seekhead level, so we add
   * a dummy. This is a crude hack. */
  if (matroska->num_levels == EBML_MAX_DEPTH) {
    //    av_log(matroska->ctx, AV_LOG_INFO,
    //	   "Max EBML element depth (%d) reached, "
    //	   "cannot parse further.\n", EBML_MAX_DEPTH);
    errval=-11;
    ret = 0; // TODO ** AVERROR_INVALIDDATA;
  } else {
    level.start = 0;
    level.length = (uint64_t)-1;
    matroska->levels[matroska->num_levels] = level;
    matroska->num_levels++;
    matroska->current_id = 0;
    
    ret = ebml_parse(cdata, matroska_segment, matroska);
    
    /* remove dummy level */
    while (matroska->num_levels) {
      uint64_t length = matroska->levels[--matroska->num_levels].length;
      if (length == (uint64_t)-1)
	break;
    }
  }

  /* seek back */
  priv->input_position=before_pos;
  lseek(priv->fd, priv->input_position, SEEK_SET);
  
  matroska->level_up = level_up;
  matroska->current_id = saved_id;
  
  return ret;
}




static void matroska_add_index_entries(const lives_clip_data_t *cdata) {

  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  EbmlList *index_list;
  MatroskaIndex *index;
  int index_scale = 1;
  int i, j;

  index_list = &matroska->index;
  index = index_list->elem;
  printf("adding indices %d\n",index_list->nb_elem);

  if (index_list->nb_elem
      && index[0].time > 1E14/matroska->time_scale) {
    //av_log(matroska->ctx, AV_LOG_WARNING, "Working around broken index.\n");
    index_scale = matroska->time_scale;
  }

  for (i = 0; i < index_list->nb_elem; i++) {
    EbmlList *pos_list = &index[i].pos;
    MatroskaIndexPos *pos = pos_list->elem;
    for (j = 0; j < pos_list->nb_elem; j++) {
      MatroskaTrack *track = matroska_find_track_by_num(matroska, pos[j].track);
      if (track && track->stream && track->stream==priv->vidst) {
	lives_add_idx(cdata, pos[j].pos + matroska->segment_start, (uint32_t)(index[i].time/index_scale));

	// TODO - ******
	av_add_index_entry(track->stream, pos[j].pos + matroska->segment_start,
			   index[i].time/index_scale, 0, 0, AVINDEX_KEYFRAME);



	printf("ADD INDEX %ld %ld\n", pos[j].pos + matroska->segment_start, index[i].time/index_scale);


      }
      
      
    }
  }
}


static void matroska_parse_cues(const lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  EbmlList *seekhead_list = &matroska->seekhead;
  MatroskaSeekhead *seekhead = seekhead_list->elem;
  int i;
  
  for (i = 0; i < seekhead_list->nb_elem; i++)
    if (seekhead[i].id == MATROSKA_ID_CUES)
      break;
  //assert(i <= seekhead_list->nb_elem);
  
  matroska_parse_seekhead_entry(cdata, i);
  matroska_add_index_entries(cdata);
}



static void matroska_execute_seekhead(const lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  EbmlList *seekhead_list = &matroska->seekhead;
  MatroskaSeekhead *seekhead = seekhead_list->elem;
  int64_t before_pos = priv->data_start;
  int i;
  
  /*  // we should not do any seeking in the streaming case
      if (!matroska->ctx->pb->seekable ||
      (matroska->ctx->flags & AVFMT_FLAG_IGNIDX))
      return;*/

  for (i = 0; i < seekhead_list->nb_elem; i++) {
    if (seekhead[i].pos <= before_pos)
      continue;

    // defer cues parsing until we actually need cue data.
    if (seekhead[i].id == MATROSKA_ID_CUES) {
      matroska->cues_parsing_deferred = 1;
      continue;
    }

    if (matroska_parse_seekhead_entry(cdata, i) < 0)
      break;
  }

}


//////////////////////////////////////////////

static int frame_to_dts(const lives_clip_data_t *cdata, int64_t frame) {
  return (int)((double)(frame)*1000./cdata->fps);
}

static int64_t dts_to_frame(const lives_clip_data_t *cdata, int dts) {
  return (int64_t)((double)dts/1000.*cdata->fps+.5);
}


//////////////////////////////////////////////////////////////////



static off_t get_last_packet_pos(const lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  int32_t tagsize;
  off_t offs=lseek(priv->fd,-4,SEEK_END);

  tagsize=get_last_tagsize(cdata);

  return offs-tagsize;
}



static int get_last_video_dts(const lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  lives_mkv_pack_t pack;
  int delta,tagsize;
  unsigned char flags;
  off_t offs;

  return priv->idxht->dts;

  priv->input_position=get_last_packet_pos(cdata);

  while (1) {
    if (!lives_mkv_parse_pack(cdata,&pack)) return -1;
    delta=-15;
    if (pack.type==MKV_TAG_TYPE_VIDEO) {
      if (read (priv->fd, &flags, 1) < 1) return -1;
      if (!((flags & 0xF0)==0x50)) break;
      delta-=1;
    }
    // rewind to previous packet
    offs=lseek(priv->fd,delta,SEEK_CUR);
    if (offs<=0) return -1;
    tagsize=get_last_tagsize(cdata);
    priv->input_position-=15+tagsize;
  }
  return pack.dts;
}


static boolean is_keyframe(int fd) {
  unsigned char data[2];

  // read 6 bytes
  if (read (fd, data, 2) < 2) return FALSE;

  if ((data[0]&0xF0)==0x10) return TRUE;

  if ((data[0]&0xF0)==0x50 && data[1]==0) return TRUE; // TODO *** - h264 ? may need to seek to next video frame

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



/// here we assume that pts of interframes > pts of previous keyframe
// should be true for most formats (except eg. dirac)

// we further assume that pts == dts for all frames


static index_entry *index_walk(index_entry *idx, int pts) {
  index_entry *xidx=idx;
  while(xidx!=NULL) {
    if (xidx->next==NULL || (pts>=xidx->dts && pts<idx->next->dts)) return xidx;
    xidx=xidx->next;
  }
  /// oops. something went wrong
  return NULL; 
}


index_entry *lives_add_idx(const lives_clip_data_t *cdata, uint64_t offset, uint32_t pts) {
  lives_mkv_priv_t *priv=cdata->priv;
  index_entry *nidx=priv->idxht;
  index_entry *nentry;

  nentry=malloc(sizeof(index_entry));

  nentry->dts=pts;
  nentry->offs=offset;
  nentry->prev=nentry->next=NULL;

  if (nidx==NULL) {
    // first entry in list
    priv->idxhh=priv->idxht=nentry;
    return nentry;
  }

  printf("vl %ld cf %ld\n",nidx->dts,pts);

  if (nidx->dts < pts) {
    // last entry in list
    nentry->prev=nidx;
    nidx->next=nentry;
    priv->idxht=nentry;
    return nentry;
  }

  if (priv->idxhh->dts>pts) {
    // before head
    nentry->next=priv->idxhh;
    priv->idxhh->prev=nentry;
    priv->idxhh=nentry;
    return nentry;
  }

  nidx=index_walk(priv->idxhh,pts);

  // after nidx in list
  nidx->next->prev=nentry;
  nentry->prev=nidx;

  nentry->next=nidx->next;
  nidx->next=nentry;

  return nentry;
}



static index_entry *get_idx_for_pts(const lives_clip_data_t *cdata, int64_t pts) {
  lives_mkv_priv_t *priv=cdata->priv;
  return index_walk(priv->idxhh,pts);
}


//////////////////////////////////////////////



static int lives_mkv_read_header(lives_clip_data_t *cdata, lives_mkv_pack_t *pack) {
  lives_mkv_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;

  MatroskaDemuxContext *matroska = s->priv_data;
  EbmlList *attachements_list = &matroska->attachments;
  MatroskaAttachement *attachements;
  //EbmlList *chapters_list = &matroska->chapters;
  //MatroskaChapter *chapters;
  MatroskaTrack *tracks;
  //uint64_t max_start = 0;
  Ebml ebml = { 0 };
  AVStream *st;
  int i, j, k, res;

  matroska->ctx = s;

  /* First read the EBML header. */
  if ((res=ebml_parse(cdata, ebml_syntax, &ebml))
      || ebml.version > EBML_VERSION       || ebml.max_size > sizeof(uint64_t)
      || ebml.id_length > sizeof(uint32_t) || ebml.doctype_version > 3) {
    printf(
	   "EBML %d header using unsupported features\n"
	   "(EBML version %"PRIu64", doctype %s, doc version %"PRIu64")\n", res,
	   ebml.version, ebml.doctype, ebml.doctype_version);
    ebml_free(ebml_syntax, &ebml);
    return -19;// AVERROR_PATCHWELCOME;
  } else if (ebml.doctype_version == 3) {
    /*        av_log(matroska->ctx, AV_LOG_WARNING,
	      "EBML header using unsupported features\n"
	      "(EBML version %"PRIu64", doctype %s, doc version %"PRIu64")\n",
	      ebml.version, ebml.doctype, ebml.doctype_version);*/
  }

  for (i = 0; i < FF_ARRAY_ELEMS(matroska_doctypes); i++)
    if (ebml.doctype==NULL || !strcmp(ebml.doctype, matroska_doctypes[i]))
      break;

  if (i >= FF_ARRAY_ELEMS(matroska_doctypes)) {
    //av_log(s, AV_LOG_WARNING, "Unknown EBML doctype '%s'\n", ebml.doctype);
  }

  ebml_free(ebml_syntax, &ebml);

  priv->data_start=priv->input_position;

  /* The next thing is a segment. */
  if ((res = ebml_parse(cdata, matroska_segments, matroska)) < 0) {
    return res;
  }

  matroska_execute_seekhead(cdata);

  if (!matroska->time_scale)
    matroska->time_scale = 1000000;
  if (matroska->duration)
    matroska->ctx->duration = matroska->duration * matroska->time_scale
      * 1000 / AV_TIME_BASE;
  av_dict_set(&s->metadata, "title", matroska->title, 0);

  tracks = matroska->tracks.elem;

  for (i=0; i < matroska->tracks.nb_elem; i++) {

    MatroskaTrack *track = &tracks[i];
    enum CodecID codec_id = CODEC_ID_NONE;
    EbmlList *encodings_list = &track->encodings;
    MatroskaTrackEncoding *encodings = encodings_list->elem;
    uint8_t *extradata = NULL;
    int extradata_size = 0;
    int extradata_offset = 0;
    uint32_t fourcc = 0;

    /* Apply some sanity checks. */
    if (track->type != MATROSKA_TRACK_TYPE_VIDEO &&
	track->type != MATROSKA_TRACK_TYPE_AUDIO &&
	track->type != MATROSKA_TRACK_TYPE_SUBTITLE) {
      continue;
    }
    if (track->codec_id == NULL)
      continue;

    if (track->type == MATROSKA_TRACK_TYPE_VIDEO) {
      printf("got a video track !\n");

      if (priv->has_video) {
	fprintf(stderr,"mkv_decoder: duplicate video streams found\n");
	continue;
	return -6;
      }

      priv->vididx=i;

      priv->has_video=TRUE;

      if (!track->default_duration)
	track->default_duration = 1000000000/track->video.frame_rate;

      if (cdata->fps==0.) cdata->fps=track->video.frame_rate;

      if (!track->video.display_width)
	track->video.display_width = track->video.pixel_width;

      cdata->width=track->video.pixel_width;

      if (!track->video.display_height)
	track->video.display_height = track->video.pixel_height;
      
      cdata->height=track->video.pixel_height;

      if (track->video.color_space.size == 4)
	fourcc = AV_RL32(track->video.color_space.data);



    } else if (track->type == MATROSKA_TRACK_TYPE_AUDIO) {
      priv->has_audio=TRUE;
    }
    if (encodings_list->nb_elem > 1) {
      fprintf(stderr,"mkv_decoder: Multiple combined encodings not supported\n");
      return -1;
    }
 
    if (encodings_list->nb_elem == 1) {
      if (encodings[0].type ||
	  (encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP &&
#if CONFIG_ZLIB
	   encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_ZLIB &&
#endif
#if CONFIG_BZLIB
	   encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_BZLIB &&
#endif
	   encodings[0].compression.algo != MATROSKA_TRACK_ENCODING_COMP_LZO)) {
	encodings[0].scope = 0;


	fprintf(stderr,"mkv_decoder: Unsupported encoding type\n");
	return -2;

      } else if (track->codec_priv.size && encodings[0].scope&2) {
	uint8_t *codec_priv = track->codec_priv.data;
	int offset = matroska_decode_buffer(&track->codec_priv.data,
					    &track->codec_priv.size,
					    track);
	if (offset < 0) {
	  track->codec_priv.data = NULL;
	  track->codec_priv.size = 0;


	fprintf(stderr,
		 "mkv_decoder: Failed to decode codec private data\n");
	return -3;


	} else if (offset > 0) {
	  track->codec_priv.data = malloc(track->codec_priv.size + offset);
	  memcpy(track->codec_priv.data,
		 encodings[0].compression.settings.data, offset);
	  memcpy(track->codec_priv.data+offset, codec_priv,
		 track->codec_priv.size);
	  track->codec_priv.size += offset;
	}
	if (codec_priv != track->codec_priv.data)
	  free(codec_priv);
      }
    }

    for(j=0; ff_mkv_codec_tags[j].id != CODEC_ID_NONE; j++){
      if(!strncmp(ff_mkv_codec_tags[j].str, track->codec_id,
		  strlen(ff_mkv_codec_tags[j].str))){
	codec_id= ff_mkv_codec_tags[j].id;
	break;
      }
    }

    st = track->stream = av_new_stream(s, 0);
    if (st == NULL) {
	fprintf(stderr,
		"mkv_decoder: Out of memory\n");
	return -4;
    }


    if (!strcmp(track->codec_id, "V_MS/VFW/FOURCC")
	&& track->codec_priv.size >= 40
	&& track->codec_priv.data != NULL) {
      track->ms_compat = 1;
      fourcc = AV_RL32(track->codec_priv.data + 16);
      codec_id = ff_codec_get_id(codec_bmp_tags, fourcc);
      extradata_offset = 40;
    } else if (!strcmp(track->codec_id, "V_QUICKTIME")
	       && (track->codec_priv.size >= 86)
	       && (track->codec_priv.data != NULL)) {
      fourcc = AV_RL32(track->codec_priv.data);
      codec_id = ff_codec_get_id(codec_movvideo_tags, fourcc);
    } else if (codec_id == CODEC_ID_RV10 || codec_id == CODEC_ID_RV20 ||
	       codec_id == CODEC_ID_RV30 || codec_id == CODEC_ID_RV40) {
      extradata_offset = 26;
    }


    track->codec_priv.size -= extradata_offset;

    if (codec_id == CODEC_ID_NONE) {
	fprintf(stderr,
		"mkv_decoder: Unknown video codec\n");
	return -42;
    }

    if (track->time_scale < 0.01)
      track->time_scale = 1.0;

    av_set_pts_info(st, 64, matroska->time_scale*track->time_scale, 1000*1000*1000); /* 64 bit pts in ns */

    st->codec->codec_id = codec_id;
    st->start_time = 0;


    /*    if (strcmp(track->language, "und"))
      av_dict_set(&st->metadata, "language", track->language, 0);
    av_dict_set(&st->metadata, "title", track->name, 0);
    */

    if (track->flag_default)
      st->disposition |= AV_DISPOSITION_DEFAULT;
    if (track->flag_forced)
      st->disposition |= AV_DISPOSITION_FORCED;

    //if (track->default_duration)
    //av_reduce(&st->codec->time_base.num, &st->codec->time_base.den,
    //		track->default_duration, 1000000000, 30000);

    if (!st->codec->extradata) {
      if(extradata){
	st->codec->extradata = extradata;
	st->codec->extradata_size = extradata_size;
      } else if(track->codec_priv.data && track->codec_priv.size > 0){
	st->codec->extradata = calloc(track->codec_priv.size +
				      FF_INPUT_BUFFER_PADDING_SIZE,1);
	if(st->codec->extradata == NULL){
	  fprintf(stderr,
		  "mkv_decoder: Out of memory\n");
	  return -4;
	}

	st->codec->extradata_size = track->codec_priv.size;
	memcpy(st->codec->extradata,
	       track->codec_priv.data + extradata_offset,
	       track->codec_priv.size);
      }
    }

    if (track->type == MATROSKA_TRACK_TYPE_VIDEO) {
      MatroskaTrackPlane *planes = track->operation.combine_planes.elem;
      if (priv->vidst!=NULL) continue;
      priv->vidst=st;
      st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
      st->codec->codec_tag  = fourcc;
      st->codec->width  = track->video.pixel_width;
      st->codec->height = track->video.pixel_height;
      /*av_reduce(&st->sample_aspect_ratio.num,
		&st->sample_aspect_ratio.den,
		st->codec->height * track->video.display_width,
		st->codec-> width * track->video.display_height,
		255);*/
      if (st->codec->codec_id != CODEC_ID_H264)
	st->need_parsing = AVSTREAM_PARSE_HEADERS;
      if (track->default_duration)
	st->avg_frame_rate = av_d2q(1000000000.0/track->default_duration, INT_MAX);

      if (cdata->fps==0.) cdata->fps=st->avg_frame_rate.num/st->avg_frame_rate.den;

      /*
      if (track->video.stereo_mode && track->video.stereo_mode < MATROSKA_VIDEO_STEREO_MODE_COUNT)
	av_dict_set(&st->metadata, "stereo_mode", matroska_video_stereo_mode[track->video.stereo_mode], 0);
      */

      /* if we have virtual track, mark the real tracks */
      for (j=0; j < track->operation.combine_planes.nb_elem; j++) {
	char buf[32];
	if (planes[j].type >= MATROSKA_VIDEO_STEREO_PLANE_COUNT)
	  continue;
	snprintf(buf, sizeof(buf), "%s_%d",
		 matroska_video_stereo_plane[planes[j].type], i);
	for (k=0; k < matroska->tracks.nb_elem; k++)
	  if (planes[j].uid == tracks[k].uid) {
	    av_dict_set(&s->streams[k]->metadata,
			"stereo_mode", buf, 0);
	    break;
	  }
      }
    }
  }

  if (priv->vidst==NULL) {
    fprintf(stderr,"mkv_decoder: no video stream found\n");
    return -5;
  }


  /* Parse the CUES now since we need the index data to seek. */
  if (priv->matroska.cues_parsing_deferred) {
    matroska_parse_cues(cdata);
  }

  if (!priv->vidst->nb_index_entries) {
    fprintf(stderr,"mkv_decoder: no seek info found\n");
    return -6;
  }


  switch (priv->vidst->codec->codec_id) {
  case CODEC_ID_VP8  : sprintf(cdata->video_name,"%s","vp8"); break;
  case CODEC_ID_THEORA  : sprintf(cdata->video_name,"%s","theora"); break;
  case CODEC_ID_SNOW  : sprintf(cdata->video_name,"%s","snow"); break;
  case CODEC_ID_DIRAC  : sprintf(cdata->video_name,"%s","dirac"); break;
  case CODEC_ID_MJPEG  : sprintf(cdata->video_name,"%s","mjpeg"); break;
  case CODEC_ID_MPEG1VIDEO  : sprintf(cdata->video_name,"%s","mpeg1"); break;
  case CODEC_ID_MPEG2VIDEO  : sprintf(cdata->video_name,"%s","mpeg2"); break;
  case CODEC_ID_MPEG4  : sprintf(cdata->video_name,"%s","mpeg4"); break;
  case CODEC_ID_H264  : sprintf(cdata->video_name,"%s","avc"); break;
  case CODEC_ID_MSMPEG4V3  : sprintf(cdata->video_name,"%s","msmpeg4"); break;
  case CODEC_ID_RV10  : sprintf(cdata->video_name,"%s","rv10"); break;
  case CODEC_ID_RV20  : sprintf(cdata->video_name,"%s","rv20"); break;
  case CODEC_ID_RV30  : sprintf(cdata->video_name,"%s","rv30"); break;
  case CODEC_ID_RV40  : sprintf(cdata->video_name,"%s","rv40"); break;
  case CODEC_ID_RAWVIDEO  : sprintf(cdata->video_name,"%s","raw"); break;
  default  : sprintf(cdata->video_name,"%s","unknown"); break;
  }

  return 0;
}




static void detach_stream (lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_mkv_priv_t *priv=cdata->priv;

  cdata->seek_flag=0;

  if (priv->s) matroska_read_close(cdata);

  if (priv->ctx!=NULL) {
    avcodec_close(priv->ctx);
    av_free(priv->ctx);
  }

  if (priv->picture!=NULL) av_free(priv->picture);

  priv->ctx=NULL;
  priv->codec=NULL;
  priv->picture=NULL;

  if (priv->idxhh!=NULL) index_free(priv->idxhh);

  priv->idxhh=NULL;
  priv->idxht=NULL;
  priv->idxth=NULL;

  if (cdata->palettes!=NULL) free(cdata->palettes);

  close(priv->fd);
}




#define MKV_PROBE_SIZE 5
#define MKV_META_SIZE 1024

static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get a handle
  lives_mkv_priv_t *priv=cdata->priv;
  lives_mkv_pack_t pack;
  unsigned char header[MKV_PROBE_SIZE];
  boolean got_astream;
  int64_t ldts;
  double fps;

  AVCodec *codec=NULL;
  AVCodecContext *ctx;

  boolean got_picture=FALSE;

  int len;
  
  struct stat sb;

#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"\n");
#endif

  priv->has_audio=priv->has_video=FALSE;
  priv->vidst=NULL;
  priv->vididx=-1;

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "mkv_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

  if (read (priv->fd, header, MKV_PROBE_SIZE) < MKV_PROBE_SIZE) {
    // for example, might be a directory
#ifdef DEBUG
    fprintf(stderr, "mkv_decoder: unable to read header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }

  priv->input_position+=MKV_PROBE_SIZE;

  if (!lives_mkv_probe(cdata, header)) {
#ifdef DEBUG
    fprintf(stderr, "mkv_decoder: unable to parse header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }

  priv->input_position=0;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  cdata->fps=0.;
  cdata->width=cdata->frame_width=cdata->height=cdata->frame_height=0;
  cdata->offs_x=cdata->offs_y=0;

  cdata->arate=0;
  cdata->achans=0;
  cdata->asamps=16;

  priv->idxhh=NULL;
  priv->idxht=NULL;
  priv->idxth=NULL;

  priv->s = avformat_alloc_context();

  memset(&priv->matroska,0,sizeof(priv->matroska));
  priv->s->priv_data = &priv->matroska;
  av_init_packet(&priv->avpkt);
  priv->avpkt.data=NULL;
  priv->st=NULL;
  priv->ctx=NULL;

  fstat(priv->fd,&sb);
  priv->filesize=sb.st_size;

  if (lives_mkv_read_header(cdata,&pack)) {
    close(priv->fd);
    return FALSE;
  }

  cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;

  cdata->offs_x=0;
  cdata->offs_y=0;

  priv->data_start=priv->input_position;

  if (!priv->has_audio) got_astream=TRUE;

#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"video type is %s %d x %d (%d x %d +%d +%d)\n",cdata->video_name,
	  cdata->width,cdata->height,cdata->frame_width,cdata->frame_height,cdata->offs_x,cdata->offs_y);
#endif

  codec = avcodec_find_decoder(priv->vidst->codec->codec_id);

  if (!codec) {
    if (strlen(cdata->video_name)>0) 
      fprintf(stderr, "mkv_decoder: Could not find avcodec codec for video type %s\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }

  priv->ctx = ctx = avcodec_alloc_context();
  sprintf(cdata->audio_name,"none");

  if (avcodec_open(ctx, codec) < 0) {
    fprintf(stderr, "mkv_decoder: Could not open avcodec context\n");
    detach_stream(cdata);
    return FALSE;
  }

  priv->codec=codec;

  // re-scan with avcodec; priv->data_start holds video data start position

  av_init_packet(&priv->avpkt);

  priv->picture = avcodec_alloc_frame();

  matroska_read_seek(cdata,0,0);

  matroska_read_packet(cdata,&priv->avpkt);

#if LIBAVCODEC_VERSION_MAJOR >= 52
  len=avcodec_decode_video2(ctx, priv->picture, &got_picture, &priv->avpkt );
#else 
  len=avcodec_decode_video(ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size );
#endif
  
  if (!got_picture) {
    fprintf(stderr,"mkv_decoder: could not get picture.\n");
    detach_stream(cdata);
    return FALSE;
  }

  cdata->YUV_clamping=WEED_YUV_CLAMPING_UNCLAMPED;
  if (ctx->color_range==AVCOL_RANGE_MPEG) cdata->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;

  cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT;
  if (ctx->chroma_sample_location!=AVCHROMA_LOC_LEFT) cdata->YUV_sampling=WEED_YUV_SAMPLING_MPEG;

  cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
  if (ctx->colorspace==AVCOL_SPC_BT709) cdata->YUV_subspace=WEED_YUV_SUBSPACE_BT709;

  cdata->palettes=(int *)malloc(2*sizeof(int));

  cdata->palettes[0]=avi_pix_fmt_to_weed_palette(ctx->pix_fmt,
						 &cdata->YUV_clamping);
  cdata->palettes[1]=WEED_PALETTE_END;

  if (cdata->palettes[0]==WEED_PALETTE_END) {
    fprintf(stderr, "mkv_decoder: Could not find a usable palette for (%d) %s\n",ctx->pix_fmt,cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  cdata->current_palette=cdata->palettes[0];

  // re-get fps, width, height, nframes - actually avcodec is pretty useless at getting this
  // so we fall back on the values we obtained ourselves

  if (cdata->width==0) cdata->width=ctx->width-cdata->offs_x*2;
  if (cdata->height==0) cdata->height=ctx->height-cdata->offs_y*2;
  
  if (cdata->width*cdata->height==0) {
    fprintf(stderr, "mkv_decoder: invalid width and height (%d X %d)\n",cdata->width,cdata->height);
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
    // not sure about this
    if (ctx->time_base.den==1) cdata->fps=12.;
  }


  if (cdata->fps==0.||cdata->fps==1000.) {
    fprintf(stderr, "mkv_decoder: invalid framerate %.4f (%d / %d)\n",cdata->fps,ctx->time_base.den,ctx->time_base.num);
    detach_stream(cdata);
    return FALSE;
  }
  
  if (ctx->ticks_per_frame==2) {
    // TODO - needs checking
    cdata->fps/=2.;
    cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;
  }

  priv->last_frame=-1;

  ldts=get_last_video_dts(cdata);
    
  if (ldts==-1) {
    fprintf(stderr, "mkv_decoder: could not read last dts\n");
    detach_stream(cdata);
    return FALSE;
  }
  
  cdata->nframes=dts_to_frame(cdata,ldts)+1;

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
  lives_mkv_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->URI=NULL;
  
  cdata->priv=priv=malloc(sizeof(lives_mkv_priv_t));

  cdata->seek_flag=0;

  priv->ctx=NULL;
  priv->codec=NULL;
  priv->picture=NULL;

  cdata->palettes=NULL;

  got_eof=FALSE;
  errval=0;
  
  return cdata;
}






lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different current_clip, this must be called again with the same
  // cdata as the second parameter

  // value returned should be freed with clip_data_free() when no longer required

  // should be thread-safe

  lives_mkv_priv_t *priv;


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

  sprintf(cdata->container_name,"%s","mkv");

  // cdata->height was set when we attached the stream

  cdata->interlace=LIVES_INTERLACE_NONE;

  cdata->frame_width=cdata->width+cdata->offs_x*2;
  cdata->frame_height=cdata->height+cdata->offs_y*2;

  priv=cdata->priv;
  // TODO - check this = spec suggests we should cut right and bottom
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

  for (i=0;i<npixels;i++) {
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
    default: break;
    }
  }
  return idst-dst;
}



/*
 * Put one packet in an application-supplied AVPacket struct.
 * Returns 0 on success or -1 on failure.
 */
static int matroska_deliver_packet(const lives_clip_data_t *cdata, AVPacket *pkt) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska = &priv->matroska;

  if (matroska->num_packets > 0) {

    memcpy(pkt, matroska->packets[0], sizeof(AVPacket));
    free(matroska->packets[0]);
    if (matroska->num_packets > 1) {
      void *newpackets;
      memmove(&matroska->packets[0], &matroska->packets[1],
	      (matroska->num_packets - 1) * sizeof(AVPacket *));
      newpackets = av_realloc(matroska->packets,
			      (matroska->num_packets - 1) * sizeof(AVPacket *));
      if (newpackets)
	matroska->packets = newpackets;
    } else {
      av_freep(&matroska->packets);
    }
    matroska->num_packets--;
    return 0;
  }

  return -1;
}




/*
 * Free all packets in our internal queue.
 */
static void matroska_clear_queue(MatroskaDemuxContext *matroska)
{
  if (matroska->packets) {
    int n;
    for (n = 0; n < matroska->num_packets; n++) {
      av_free_packet(matroska->packets[n]);
      free(matroska->packets[n]);
    }
    av_freep(&matroska->packets);
    matroska->num_packets = 0;
  }
}

static int matroska_parse_block(const lives_clip_data_t *cdata, uint8_t *data,
                                int size, int64_t pos, uint64_t cluster_time,
                                uint64_t duration, int is_keyframe,
                                int64_t cluster_pos)
{

  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  uint64_t timecode = AV_NOPTS_VALUE;
  MatroskaTrack *track;
  int res = 0;
  AVStream *st;
  AVPacket *pkt;
  int16_t block_time;
  uint32_t *lace_size = NULL;
  int n, flags, laces = 0;
  uint64_t num;
  
  unsigned char buffer[2];

  //  printf("PARSING BLOCK\n");

  if ((n = matroska_ebmlnum_uint(cdata, data, size, &num)) < 0) {
    //av_log(matroska->ctx, AV_LOG_ERROR, "EBML block data error\n");
    return res;
  }
  data += n;
  size -= n;
  
  track = matroska_find_track_by_num(matroska, num);

  if (!track || !track->stream) {
    //av_log(matroska->ctx, AV_LOG_INFO,
    //       "Invalid stream %"PRIu64" or size %u\n", num, size);
    return res;
  } else if (size <= 3) {
    return 0;
  }
  st = track->stream;
  if (st!=priv->vidst) {
    return res;
  }
  if (st->discard >= AVDISCARD_ALL) {
    return res;
  }
  if (!duration)
    duration = track->default_duration / matroska->time_scale;

  block_time = AV_RB16(data);

  data += 2;
  flags = *data++;
  size -= 3;
  if (is_keyframe == -1)
    is_keyframe = flags & 0x80 ? AV_PKT_FLAG_KEY : 0;

  if (cluster_time != (uint64_t)-1
      && (block_time >= 0 || cluster_time >= -block_time)) {
    timecode = cluster_time + block_time;
    if (track->type == MATROSKA_TRACK_TYPE_SUBTITLE
	&& timecode < track->end_timecode)
      is_keyframe = 0;  /* overlapping subtitles are not key frame */

    // TODO **** 
    if (is_keyframe) {
      av_add_index_entry(st, cluster_pos, timecode, 0,0,AVINDEX_KEYFRAME);
    }

    track->end_timecode = FFMAX(track->end_timecode, timecode+duration);
  }
  
  if (matroska->skip_to_keyframe && track->type != MATROSKA_TRACK_TYPE_SUBTITLE) {
    if (!is_keyframe || timecode < matroska->skip_to_timecode) {
      return res;
    }
    matroska->skip_to_keyframe = 0;
  }

  switch ((flags & 0x06) >> 1) {
  case 0x0: /* no lacing */
    laces = 1;
    lace_size = calloc(sizeof(int),1);
    lace_size[0] = size;
    break;

  case 0x1: /* Xiph lacing */
  case 0x2: /* fixed-size lacing */
  case 0x3: /* EBML lacing */
    //assert(size>0); // size <=3 is checked before size-=3 above

    laces = (*data) + 1;
    data += 1;
    size -= 1;
    lace_size = calloc(laces,sizeof(int));

    switch ((flags & 0x06) >> 1) {
    case 0x1: /* Xiph lacing */ {
      uint8_t temp;
      uint32_t total = 0;
      for (n = 0; res == 0 && n < laces - 1; n++) {
	while (1) {
	  if (size == 0) {
	    res = -1;
	    break;
	  }
	  temp = *data;
	  lace_size[n] += temp;
	  data += 1;
	  size -= 1;
	  if (temp != 0xff)
	    break;
	}
	total += lace_size[n];
      }
      lace_size[n] = size - total;
      break;
    }

    case 0x2: /* fixed-size lacing */
      for (n = 0; n < laces; n++)
	lace_size[n] = size / laces;
      break;

    case 0x3: /* EBML lacing */ {
      uint32_t total;
      n = matroska_ebmlnum_uint(cdata, data, size, &num);
      if (n < 0) {
	//  av_log(matroska->ctx, AV_LOG_INFO,
	//         "EBML block data error\n");
	break;
      }
      data += n;
      size -= n;
      total = lace_size[0] = num;
      for (n = 1; res == 0 && n < laces - 1; n++) {
	int64_t snum;
	int r;
	r = matroska_ebmlnum_sint(cdata, data, size, &snum);
	if (r < 0) {
	  // av_log(matroska->ctx, AV_LOG_INFO,
	  //        "EBML block data error\n");
	  break;
	}
	data += r;
	size -= r;
	lace_size[n] = lace_size[n - 1] + snum;
	total += lace_size[n];
      }
      lace_size[laces - 1] = size - total;
      break;
    }
    }
    break;
  }

  if (res == 0) {
    for (n = 0; n < laces; n++) {

      {
	MatroskaTrackEncoding *encodings = track->encodings.elem;
	int offset = 0, pkt_size = lace_size[n];
	uint8_t *pkt_data = data;

	if (pkt_size > size) {
	  //av_log(matroska->ctx, AV_LOG_ERROR, "Invalid packet size\n");
	  break;
	}

	if (encodings && encodings->scope & 1) {
	  offset = matroska_decode_buffer(&pkt_data,&pkt_size, track);
	  if (offset < 0)
	    continue;
	}

	pkt = calloc(sizeof(AVPacket),1);
	/* XXX: prevent data copy... */
	if (av_new_packet(pkt, pkt_size+offset) < 0) {
	  free(pkt);
	  res = AVERROR(ENOMEM);
	  break;
	}
	if (offset)
	  memcpy (pkt->data, encodings->compression.settings.data, offset);
	memcpy (pkt->data+offset, pkt_data, pkt_size);

	if (pkt_data != data)
	  free(pkt_data);

	if (n == 0)
	  pkt->flags = is_keyframe;
	pkt->stream_index = st->index;

	if (track->ms_compat)
	  pkt->dts = timecode;
	else
	  pkt->pts = timecode;
	pkt->pos = pos;

	if (st->codec->codec_id == CODEC_ID_TEXT)
	  pkt->convergence_duration = duration;

	else if (track->type != MATROSKA_TRACK_TYPE_SUBTITLE)
	  pkt->duration = duration;

	if (st->codec->codec_id == CODEC_ID_SSA)
	  matroska_fix_ass_packet(matroska, pkt, duration);

	if (matroska->prev_pkt &&
	    timecode != AV_NOPTS_VALUE &&
	    matroska->prev_pkt->pts == timecode &&
	    matroska->prev_pkt->stream_index == st->index &&
	    st->codec->codec_id == CODEC_ID_SSA)
	  matroska_merge_packets(matroska->prev_pkt, pkt);
	else {
	  av_dynarray_add(&matroska->packets,&matroska->num_packets,pkt);
	  matroska->prev_pkt = pkt;
	  //printf("DYNARRAY ADD %d\n",matroska->num_packets);
	}

      }

      if (timecode != AV_NOPTS_VALUE)
	timecode = duration ? timecode + duration : AV_NOPTS_VALUE;
      data += lace_size[n];
      size -= lace_size[n];
    }
  }

  free(lace_size);
  return res;
}


static int matroska_parse_cluster(const lives_clip_data_t *cdata)
{

  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  MatroskaCluster cluster = { 0 };
  EbmlList *blocks_list;
  MatroskaBlock *blocks;
  int i, res;

  int64_t pos = priv->input_position;
  matroska->prev_pkt = NULL;
  if (matroska->current_id)
    pos -= 4;  /* sizeof the ID which was already read */

  res = ebml_parse(cdata, matroska_clusters, &cluster);

  blocks_list = &cluster.blocks;
  blocks = blocks_list->elem;
  for (i=0; blocks!=NULL && i<blocks_list->nb_elem; i++) {
    if (blocks[i].bin.size > 0 && blocks[i].bin.data) {
      int is_keyframe = blocks[i].non_simple ? !blocks[i].reference : -1;
      res=matroska_parse_block(cdata,
			       blocks[i].bin.data, blocks[i].bin.size,
			       blocks[i].bin.pos,  cluster.timecode,
			       blocks[i].duration, is_keyframe,
			       pos);
    }
  }
  ebml_free(matroska_cluster, &cluster);
  if (res < 0)  matroska->done = 1;
  return res;
}




static boolean matroska_read_packet(const lives_clip_data_t *cdata, AVPacket *pkt) {
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska = &priv->matroska;

  while (matroska_deliver_packet(cdata, pkt)) {
    if (matroska->done||got_eof) return FALSE;
    matroska_parse_cluster(cdata);
  }
  
  return TRUE;
}




static int matroska_read_seek(const lives_clip_data_t *cdata,
                              int64_t timestamp, int flags)
{

  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  AVFormatContext *s=priv->s;

  MatroskaTrack *tracks = matroska->tracks.elem;
  AVStream *st = priv->vidst;
  int i, index, index_sub, index_min;


  if (!st->nb_index_entries) return 0;

  timestamp = FFMAX(timestamp, st->index_entries[0].timestamp);

  if ((index = av_index_search_timestamp(st, timestamp, flags)) < 0) {
    priv->input_position=st->index_entries[st->nb_index_entries-1].pos;
    lseek(priv->fd,priv->input_position,SEEK_SET);
    printf("seeking to %ld\n",priv->input_position);
    matroska->current_id = 0;
    while ((index = av_index_search_timestamp(st, timestamp, flags)) < 0) {
      matroska_clear_queue(matroska);
      if (matroska_parse_cluster(cdata) < 0)
	break;
    }
  }

  matroska_clear_queue(matroska);
  if (index < 0) return 0;

  index_min = index;
  
  priv->input_position=st->index_entries[index_min].pos;
  lseek(priv->fd,priv->input_position,SEEK_SET);
  printf("2seeking to %ld\n",priv->input_position);

  matroska->current_id = 0;
  matroska->skip_to_keyframe = !(flags & AVSEEK_FLAG_ANY);
  matroska->skip_to_timecode = st->index_entries[index].timestamp;
  matroska->done = 0;
  ff_update_cur_dts(s, st, st->index_entries[index].timestamp);

  return 0;
}



static int matroska_read_close(const lives_clip_data_t *cdata) {

  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;

  MatroskaTrack *tracks = matroska->tracks.elem;
  int n;

  matroska_clear_queue(matroska);

  for (n=0; n < matroska->tracks.nb_elem; n++)
    if (tracks[n].type == MATROSKA_TRACK_TYPE_AUDIO)
      av_free(tracks[n].audio.buf);
  ebml_free(matroska_segment, matroska);

  return 0;
}


boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, int *rowstrides, int height, void **pixel_data) {
  // seek to frame,

  int64_t target_pts=frame_to_dts(cdata,tframe);
  int64_t nextframe=0;
  lives_mkv_priv_t *priv=cdata->priv;
  lives_mkv_pack_t pack;
  int xheight=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;
  int rescan_limit=16;  // pick some arbitrary value
  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;
  boolean got_picture=FALSE;
  unsigned char *dst,*src;//,flags;
  unsigned char black[4]={0,0,0,255};
  //index_entry *idx;
  register int i,p;

#ifdef DEBUG_KFRAMES
  fprintf(stderr,"vals %ld %ld\n",tframe,priv->last_frame);
#endif

  // calc frame width and height, including any border

  if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P) {
    nplanes=3;
    black[0]=y_black;
    black[1]=black[2]=128;
  }
  else if (pal==WEED_PALETTE_YUVA4444P) {
    nplanes=4;
    black[0]=y_black;
    black[1]=black[2]=128;
    black[3]=255;
  }
  
  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) psize=3;

  if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_UYVY8888||pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) psize=4;

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

      matroska_read_seek(cdata,target_pts,0);
      avcodec_flush_buffers (priv->ctx);

      /*      if ((idx=get_idx_for_pts(cdata,target_pts))!=NULL) {
	priv->input_position=idx->offs;
	nextframe=dts_to_frame(cdata,idx->dts);
      }
      else priv->input_position=priv->data_start;
      
      // we are now at the kframe before or at target - parse packets until we hit target
      

#ifdef DEBUG_KFRAMES
      if (idx!=NULL) printf("got kframe %ld for frame %ld\n",dts_to_frame(cdata,idx->dts),tframe);
#endif


    }
    else {
      nextframe=priv->last_frame+1;
    }

    priv->ctx->skip_frame=AVDISCARD_NONREF;
    */
    }
    priv->last_frame=tframe;

    nextframe=tframe;

    // do this until we reach target frame //////////////

    do {

      got_picture=FALSE;

      while (!got_picture) {
	int len;

#if LIBAVCODEC_VERSION_MAJOR >= 52
	len=avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt );
#else 
	len=avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size );
#endif
	printf("len is %d\n",len);

	matroska_read_packet(cdata,&priv->avpkt);

      }

      //free(pack.data);
      nextframe++;
    } while (nextframe<=tframe);

    /////////////////////////////////////////////////////

  }
  
  for (p=0;p<nplanes;p++) {
    dst=pixel_data[p];
    src=priv->picture->data[p];

    for (i=0;i<xheight;i++) {
      if (i<btop||i>bbot) {
	// top or bottom border, copy black row
	if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
	  memset(dst,black[p],dstwidth+(bleft+bright)*psize);
	  dst+=dstwidth+(bleft+bright)*psize;
	}
	else dst+=write_black_pixel(dst,pal,dstwidth/psize+bleft+bright,y_black);
	continue;
      }

      if (bleft>0) {
	if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
	  memset(dst,black[p],bleft*psize);
	  dst+=bleft*psize;
	}
	else dst+=write_black_pixel(dst,pal,bleft,y_black);
      }

      memcpy(dst,src,dstwidth);
      dst+=dstwidth;

      if (bright>0) {
	if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
	  memset(dst,black[p],bright*psize);
	  dst+=bright*psize;
	}
	else dst+=write_black_pixel(dst,pal,bright,y_black);
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
