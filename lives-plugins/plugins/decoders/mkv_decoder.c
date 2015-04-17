// LiVES - mkv decoder plugin
// (c) G. Finch 2011 <salsaman@xs4all.nl,salsaman@gmail.com>

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
#include <ctype.h>
#include <sys/stat.h>
#include <pthread.h>

const char *plugin_version="LiVES mkv decoder version 1.2";

#ifdef HAVE_AV_CONFIG_H
#undef HAVE_AV_CONFIG_H
#endif

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
#include <libavutil/avstring.h>
#include <libavcodec/version.h>
#include <libavutil/mem.h>

#include "decplugin.h"

#include <libavutil/intreadwrite.h>
#include <libavutil/lzo.h>
#include <libavutil/dict.h>


#include "mkv_decoder.h"

#include "libav_helper.h"

#if CONFIG_ZLIB
#include <zlib.h>
#endif
#if CONFIG_BZLIB
#include <bzlib.h>
#endif


static index_container_t **indices;
static int nidxc;
static pthread_mutex_t indices_mutex;

////////////////////////////////////////////////////////////////////////////

static double lives_int2dbl(int64_t v) {
  if ((uint64_t)v+v > 0xFFEULL<<52)
    return NAN;
  return ldexp(((v&((1LL<<52)-1)) + (1LL<<52)) * (v>>63|1), (v>>52&0x7FF)-1075);
}

static float lives_int2flt(int32_t v) {
  if ((uint32_t)v+v > 0xFF000000U)
    return NAN;
  return ldexp(((v&0x7FFFFF) + (1<<23)) * (v>>31|1), (v>>23&0xFF)-150);
}

const uint8_t ff_log2_tab[256]= {
  0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
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


#define MKV_TAG_TYPE_VIDEO 1

boolean got_eof;
int errval;

static const char *matroska_doctypes[] = { "matroska", "webm" };




static void lives_dynarray_add(void *tab_ptr, int *nb_ptr, void *elem) {
  int nb, nb_alloc;
  intptr_t *tab;

  nb = *nb_ptr;
  tab = *(intptr_t **)tab_ptr;
  if ((nb & (nb - 1)) == 0) {
    if (nb == 0)
      nb_alloc = 1;
    else
      nb_alloc = nb * 2;
    tab = av_realloc(tab, nb_alloc * sizeof(intptr_t));
    *(intptr_t **)tab_ptr = tab;
  }
  tab[nb++] = (intptr_t)elem;
  *nb_ptr = nb;
}


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
static int ebml_read_num(const lives_clip_data_t *cdata, uint8_t *data,
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
    if (read(priv->fd, buffer, 1) < 1) {
      if (!priv->expect_eof)
        fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
      got_eof=TRUE;
      return 0;
    }

    priv->input_position+=1;

    total=*buffer;
  } else {
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
      if (read(priv->fd, buffer, 1) < 1) {
        if (!priv->expect_eof)
          fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
        got_eof=TRUE;
        return 0;
      }

      priv->input_position+=1;

      val8=(uint8_t)*buffer;
    } else {
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
    errval=ERR_INVALID_DATA;
    return -errval;
  }

  /* big-endian ordering; build up number */
  *num = 0;
  while (n++ < size) {
    if (read(priv->fd, buffer, 1) < 1) {
      if (!priv->expect_eof)
        fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
      got_eof=TRUE;
      return -ERR_EOF;
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
      if (!priv->expect_eof)
        fprintf(stderr, "mkv_decoder: read error in %s\n",cdata->URI);
      got_eof=TRUE;
      return -ERR_EOF;
    }

    priv->input_position+=4;

    val32 = AV_RB32(buffer);

    *num= lives_int2flt(val32);

  } else if (size==8) {

    if (read(priv->fd,buffer,8)<8) {
      if (!priv->expect_eof)
        fprintf(stderr, "mkv_decoder: read error in %s\n",cdata->URI);
      got_eof=TRUE;
      return -ERR_EOF;
    }

    priv->input_position+=8;

    val64 = AV_RB64(buffer);

    *num= lives_int2dbl(val64);
  } else {
    errval=ERR_INVALID_DATA;
    return -errval;
  }

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
  if (!(*str = malloc(size + 1))) {
    errval=ERR_NOMEM;
    return -errval;
  }

  if (read(priv->fd, (uint8_t *) *str, size) < size) {
    if (!priv->expect_eof)
      fprintf(stderr, "mkv_decoder: error in stream header for %s\n",cdata->URI);
    av_freep(str);
    got_eof=TRUE;
    return -ERR_EOF;
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
    errval=ERR_NOMEM;
    return -errval;
  }

  bin->size = length;
  bin->pos  = priv->input_position;

  if (read(priv->fd, bin->data, length) < length) {
    if (!priv->expect_eof)
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
    errval=ERR_MAX_DEPTH;
    return -errval;
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
                         uint32_t id, void *data) {
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
                      void *data) {
  int res;
  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  if (!matroska->current_id) {
    uint64_t id;

    int res = ebml_read_num(cdata, NULL, 4, &id);

    if (res < 0)
      return res;

    matroska->current_id = id | 1 << 7*res;

    if (matroska->current_id!=163) {
      //printf("got id %08x\n",matroska->current_id);
    }

  }
  res=ebml_parse_id(cdata, syntax, matroska->current_id, data);

  return res;
}

static int ebml_parse_nest(const lives_clip_data_t *cdata, EbmlSyntax *syntax,
                           void *data) {
  int i, res = 0;

  for (i=0; syntax[i].id; i++)
    switch (syntax[i].type) {
    case EBML_UINT:
      *(uint64_t *)((char *)data+syntax[i].data_offset) = syntax[i].def.u;
      break;
    case EBML_FLOAT:
      *(double *)((char *)data+syntax[i].data_offset) = syntax[i].def.f;
      break;
    case EBML_STR:
    case EBML_UTF8:
      *(char **)((char *)data+syntax[i].data_offset) = av_strdup(syntax[i].def.s);
      break;
    default:
      break;
    }

  while (!res && !ebml_level_end(cdata))
    res = ebml_parse(cdata, syntax, data);

  return res;
}

static int ebml_parse_elem(const lives_clip_data_t *cdata, EbmlSyntax *syntax, void *data) {
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

  data = (char *)data + syntax->data_offset;

  if (syntax->list_elem_size) {
    EbmlList *list = data;
    newelem = av_realloc(list->elem, (list->nb_elem+1)*syntax->list_elem_size);
    if (!newelem) {
      fprintf(stderr,"mkv_decoder: out of memory !\n");
      errval=ERR_NOMEM;
      return -errval;
    }
    list->elem = newelem;
    data = (char *)list->elem + list->nb_elem*syntax->list_elem_size;
    memset(data, 0, syntax->list_elem_size);
    list->nb_elem++;
  }
  if (syntax->type != EBML_PASS && syntax->type != EBML_STOP) {
    matroska->current_id = 0;
    if ((res = ebml_read_length(cdata, &length)) < 0)
      return res;
    if (max_lengths[syntax->type] && length > max_lengths[syntax->type]) {
      fprintf(stderr,"mkv_decoder: invalid data in clip\n");
      errval=ERR_INVALID_DATA;
      return -errval;
    }
  }

  switch (syntax->type) {
  case EBML_UINT:
    res = ebml_read_uint(cdata, length, data);
    break;
  case EBML_FLOAT:
    res = ebml_read_float(cdata, length, data);
    break;
  case EBML_STR:
  case EBML_UTF8:
    res = ebml_read_ascii(cdata, length, data);
    break;
  case EBML_BIN:
    res = ebml_read_binary(cdata, length, data);
    break;
  case EBML_NEST:
    if ((res=ebml_read_master(cdata, length)) < 0)
      return res;
    if (id == MATROSKA_ID_SEGMENT)
      matroska->segment_start = priv->input_position;
    return ebml_parse_nest(cdata, syntax->def.n, data);
  case EBML_PASS:
    return ebml_parse_id(cdata, syntax->def.n, id, data);
  case EBML_STOP:
    return 1;
  default:
    // skip length bytes
    priv->input_position+=length;
    lseek(priv->fd, priv->input_position, SEEK_SET);
    res=check_eof(cdata);
    return res;
  }

  if (res == -ERR_INVALID_DATA) {
    fprintf(stderr,"mkv_decoder: invalid data in clip\n");
    return res;
  }

  return res;
}


static void ebml_free(EbmlSyntax *syntax, void *data) {
  int i, j;
  for (i=0; syntax[i].id; i++) {
    void *data_off = (char *)data + syntax[i].data_offset;
    switch (syntax[i].type) {
    case EBML_STR:
    case EBML_UTF8:
      av_freep(data_off);
      break;
    case EBML_BIN:
      av_freep(&((EbmlBin *)data_off)->data);
      break;
    case EBML_NEST:
      if (syntax[i].list_elem_size) {
        EbmlList *list = data_off;
        char *ptr = list->elem;
        for (j=0; j<list->nb_elem; j++, ptr+=syntax[i].list_elem_size)
          ebml_free(syntax[i].def.n, ptr);
        free(list->elem);
      } else
        ebml_free(syntax[i].def.n, data_off);
    default:
      break;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/*
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
*/

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
    int num) {
  MatroskaTrack *tracks = matroska->tracks.elem;
  int i;

  for (i=0; i < matroska->tracks.nb_elem; i++) {
    if (tracks[i].num == num)
      return &tracks[i];
  }
  //av_log(matroska->ctx, AV_LOG_ERROR, "Invalid track number %d\n", num);
  return NULL;
}




static int matroska_decode_buffer(uint8_t **buf, int *buf_size,
                                  MatroskaTrack *track) {
  MatroskaTrackEncoding *encodings = track->encodings.elem;
  uint8_t *data = *buf;
  int isize = *buf_size;
  uint8_t *pkt_data = NULL;
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
                                    AVPacket *pkt, uint64_t display_duration) {
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
    sh = sc/360000;
    sc -= 360000*sh;
    sm = sc/  6000;
    sc -=   6000*sm;
    ss = sc/   100;
    sc -=    100*ss;
    eh = ec/360000;
    ec -= 360000*eh;
    em = ec/  6000;
    ec -=   6000*em;
    es = ec/   100;
    ec -=    100*es;
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

static int matroska_merge_packets(AVPacket *out, AVPacket *in) {
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
    fprintf(stderr,"mkv_decoder: max ebml depth breached in clip\n");
    errval=-11;
    ret = 0;
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
        pthread_mutex_lock(&priv->idxc->mutex);
        lives_add_idx(cdata, pos[j].pos + matroska->segment_start, (uint32_t)(index[i].time/index_scale));
        pthread_mutex_unlock(&priv->idxc->mutex);
        //printf("ADD INDEX %ld %ld\n", pos[j].pos + matroska->segment_start, index[i].time/index_scale);
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


static int get_last_video_dts(lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  uint32_t ldts;
  uint64_t frames=0;
  boolean got_picture=FALSE;

  pthread_mutex_lock(&priv->idxc->mutex);
  if (priv->idxc->idxht==NULL) {
    pthread_mutex_unlock(&priv->idxc->mutex);
    return 0;
  }
  // jump to last dts in keyframe index
  ldts=priv->idxc->idxht->dts;

  // never trust the given duration in a video clip.
  //
  // seek to the last frame in our index, then keep reading frames until we reach eof.
  // crude but it should work.

  cdata->nframes=1000000000; // allow seeking to end

  matroska_read_seek(cdata,ldts);
  pthread_mutex_unlock(&priv->idxc->mutex);

  frames=dts_to_frame(cdata,ldts);

  av_log_set_level(AV_LOG_FATAL);

  // stop error reporting EOF
  priv->expect_eof=TRUE;

  while (1) {
    //read frames until we hit EOF

    if (priv->avpkt.data!=NULL) {
      free(priv->avpkt.data);
      priv->avpkt.data=NULL;
      priv->avpkt.size=0;
    }

    matroska_read_packet(cdata,&priv->avpkt);
    if (got_eof) {
      got_eof=FALSE;
      break;
    }

#if LIBAVCODEC_VERSION_MAJOR >= 52
    avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

    if (got_picture) {
      frames++;
    }
  }

  priv->expect_eof=FALSE;

  return frame_to_dts(cdata,frames);

}




static uint32_t calc_dts_delta(const lives_clip_data_t *cdata) {
  // try real hard to get the fps.
  // if all else has failed,
  // count the number of frames between the first two entries in our index
  // then, since we know the dts of each we can work out the dts delta per frame
  // from this we can calculate the fps

  lives_mkv_priv_t *priv=cdata->priv;

  uint32_t deltadts,origdts,idxdts;
  index_entry *idx;
  int frames=0;
  boolean got_picture=FALSE;

  pthread_mutex_lock(&priv->idxc->mutex);
  if (priv->idxc->idxhh==NULL) {
    pthread_mutex_unlock(&priv->idxc->mutex);
    return 0;
  }
  // seek to 0

  idx=matroska_read_seek(cdata,0);
  origdts=idx->dts;

  idx=idx->next;

  if (idx==NULL) {
    pthread_mutex_unlock(&priv->idxc->mutex);
    return 0;
  }
  idxdts=idx->dts;
  pthread_mutex_unlock(&priv->idxc->mutex);

  got_eof=FALSE;

  while (1) {
    //read frames until we hit the second seek frame

    if (priv->avpkt.data!=NULL) {
      free(priv->avpkt.data);
      priv->avpkt.data=NULL;
      priv->avpkt.size=0;
    }

    matroska_read_packet(cdata,&priv->avpkt);
    if (got_eof) {
      got_eof=FALSE;
      return 0;
    }

    if (priv->avpkt.pos>=idx->offs) break;

#if LIBAVCODEC_VERSION_MAJOR >= 52
    avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

    if (got_picture) {
      frames++;
    }
  }

  // divide 2nd dts by nframes, this gives delta dts

  deltadts=((double)(idxdts-origdts)/(double)(frames)+.5);

  return deltadts;

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


static index_entry *index_walk(index_entry *idx, uint32_t pts) {
  index_entry *xidx=idx;
  while (xidx!=NULL) {
    if (xidx->next==NULL || (pts>=xidx->dts && pts<xidx->next->dts)) return xidx;
    xidx=xidx->next;
  }
  /// oops. something went wrong
  return NULL;
}


index_entry *lives_add_idx(const lives_clip_data_t *cdata, uint64_t offset, uint32_t pts) {
  lives_mkv_priv_t *priv=cdata->priv;
  index_entry *nidx;
  index_entry *nentry;

  nidx=priv->idxc->idxht;

  nentry=malloc(sizeof(index_entry));

  nentry->dts=pts;
  nentry->offs=offset;
  nentry->next=NULL;

  if (nidx==NULL) {
    // first entry in list
    priv->idxc->idxhh=priv->idxc->idxht=nentry;
    return nentry;
  }

  if (nidx->dts < pts) {
    // last entry in list
    nidx->next=nentry;
    priv->idxc->idxht=nentry;
    return nentry;
  }

  if (priv->idxc->idxhh->dts>pts) {
    // before head
    nentry->next=priv->idxc->idxhh;
    priv->idxc->idxhh=nentry;
    return nentry;
  }

  nidx=index_walk(priv->idxc->idxhh,pts);

  // after nidx in list

  nentry->next=nidx->next;
  nidx->next=nentry;

  return nentry;
}



static index_entry *get_idx_for_pts(const lives_clip_data_t *cdata, uint32_t pts) {
  lives_mkv_priv_t *priv=cdata->priv;
  return index_walk(priv->idxc->idxhh,pts);
}


//////////////////////////////////////////////



static int lives_mkv_read_header(lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;

  MatroskaDemuxContext *matroska = s->priv_data;
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

      if (priv->has_video) {
#ifdef DEBUG
        fprintf(stderr,"mkv_decoder: duplicate video streams found\n");
#endif
        //continue;
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

      cdata->frame_width=track->video.display_width;
      cdata->frame_height=track->video.display_height;

      if (cdata->width!=cdata->frame_width||cdata->height!=cdata->frame_height)
        fprintf(stderr,"mkv_decoder: info frame size=%d x %d, pixel size=%d x %d\n",cdata->frame_width,cdata->frame_height,cdata->width,
                cdata->height);

      if (track->video.flag_interlaced) cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;

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

    for (j=0; ff_mkv_codec_tags[j].id != CODEC_ID_NONE; j++) {
      if (!strncmp(ff_mkv_codec_tags[j].str, track->codec_id,
                   strlen(ff_mkv_codec_tags[j].str))) {
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
      if (extradata) {
        st->codec->extradata = extradata;
        st->codec->extradata_size = extradata_size;
      } else if (track->codec_priv.data && track->codec_priv.size > 0) {
        st->codec->extradata = calloc(track->codec_priv.size +
                                      FF_INPUT_BUFFER_PADDING_SIZE,1);
        if (st->codec->extradata == NULL) {
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
      av_reduce(&st->sample_aspect_ratio.num,
                &st->sample_aspect_ratio.den,
                st->codec->height * track->video.display_width,
                st->codec-> width * track->video.display_height,
                255);
      if (st->codec->codec_id != CODEC_ID_H264)
        st->need_parsing = AVSTREAM_PARSE_HEADERS;
      if (track->default_duration)
        st->avg_frame_rate = av_d2q(1000000000.0/track->default_duration, INT_MAX);

      if (cdata->fps==0.) cdata->fps=(float)st->avg_frame_rate.num/(float)st->avg_frame_rate.den;

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
    } else {
      avcodec_close(st->codec);
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

  if (priv->idxc->idxhh==NULL) {
    fprintf(stderr,"mkv_decoder: no seek info found\n");
    return -6;
  }

  switch (priv->vidst->codec->codec_id) {
  case CODEC_ID_VP8  :
    sprintf(cdata->video_name,"%s","vp8");
    break;
  case CODEC_ID_THEORA  :
    sprintf(cdata->video_name,"%s","theora");
    break;

#if FF_API_SNOW
  case CODEC_ID_SNOW  :
    sprintf(cdata->video_name,"%s","snow");
    break;
#endif

  case CODEC_ID_DIRAC  :
    sprintf(cdata->video_name,"%s","dirac");
    break;
  case CODEC_ID_MJPEG  :
    sprintf(cdata->video_name,"%s","mjpeg");
    break;
  case CODEC_ID_MPEG1VIDEO  :
    sprintf(cdata->video_name,"%s","mpeg1");
    break;
  case CODEC_ID_MPEG2VIDEO  :
    sprintf(cdata->video_name,"%s","mpeg2");
    break;
  case CODEC_ID_MPEG4  :
    sprintf(cdata->video_name,"%s","mpeg4");
    break;
  case CODEC_ID_H264  :
    sprintf(cdata->video_name,"%s","h264");
    break;
  case CODEC_ID_MSMPEG4V3  :
    sprintf(cdata->video_name,"%s","msmpeg4");
    break;
  case CODEC_ID_RV10  :
    sprintf(cdata->video_name,"%s","rv10");
    break;
  case CODEC_ID_RV20  :
    sprintf(cdata->video_name,"%s","rv20");
    break;
  case CODEC_ID_RV30  :
    sprintf(cdata->video_name,"%s","rv30");
    break;
  case CODEC_ID_RV40  :
    sprintf(cdata->video_name,"%s","rv40");
    break;
  case CODEC_ID_RAWVIDEO  :
    sprintf(cdata->video_name,"%s","raw");
    break;
  default  :
    sprintf(cdata->video_name,"%s","unknown");
    break;
  }


  return 0;
}





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

  idxc->idxhh=NULL;
  idxc->idxht=NULL;

  idxc->nclients=1;
  idxc->clients=(lives_clip_data_t **)malloc(sizeof(lives_clip_data_t *));
  idxc->clients[0]=cdata;
  pthread_mutex_init(&idxc->mutex,NULL);

  indices[nidxc]=idxc;
  pthread_mutex_unlock(&indices_mutex);

  nidxc++;

  return idxc;
}


static void idxc_release(lives_clip_data_t *cdata) {
  lives_mkv_priv_t *priv=cdata->priv;
  index_container_t *idxc=priv->idxc;
  register int i,j;

  if (idxc==NULL) return;

  pthread_mutex_lock(&indices_mutex);

  if (idxc->nclients==1) {
    // remove this index
    index_free(idxc->idxhh);
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
    index_free(indices[i]->idxhh);
    free(indices[i]->clients);
    free(indices[i]);
  }
  nidxc=0;
}


static void detach_stream(lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_mkv_priv_t *priv=cdata->priv;

  cdata->seek_flag=0;

  if (priv->s) matroska_read_close(cdata);

  if (priv->ctx!=NULL) {
    avcodec_close(priv->ctx);
    av_free(priv->ctx);
  }

  avcodec_close(priv->vidst->codec);

  if (priv->picture!=NULL) av_frame_free(&priv->picture);

  priv->ctx=NULL;
  priv->codec=NULL;
  priv->picture=NULL;

  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;

  if (priv->avpkt.data!=NULL) {
    free(priv->avpkt.data);
    priv->avpkt.data=NULL;
    priv->avpkt.size=0;
  }

  matroska_clear_queue(&priv->matroska);

  close(priv->fd);
}




#define MKV_PROBE_SIZE 5
#define MKV_META_SIZE 1024

static boolean attach_stream(lives_clip_data_t *cdata, boolean isclone) {
  // open the file and get a handle
  lives_mkv_priv_t *priv=cdata->priv;
  unsigned char header[MKV_PROBE_SIZE];
  int64_t ldts,dts,pts;
  double fps,duration=0.;

  AVCodec *codec=NULL;
  AVCodecContext *ctx;

  int err;

  boolean got_picture=FALSE;
  boolean is_partial_clone=FALSE;

  struct stat sb;
  //#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"\n\n\n\nDEBUG MKV");
#endif

  if (isclone&&!priv->inited) {
    isclone=FALSE;
    if (cdata->fps>0.&&cdata->nframes>0)
      is_partial_clone=TRUE;
  }

  priv->has_audio=priv->has_video=FALSE;
  priv->vidst=NULL;
  priv->vididx=-1;

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "mkv_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

#ifdef IS_MINGW
  setmode(priv->fd,O_BINARY);
#endif

  if (isclone) goto skip_probe;

  if ((err=read(priv->fd, header, MKV_PROBE_SIZE)) < MKV_PROBE_SIZE) {
    // for example, might be a directory
#ifdef DEBUG
    fprintf(stderr, "mkv_decoder: unable to read header %d %d for %s\n",err,MKV_PROBE_SIZE,cdata->URI);
    if (err==0)  fprintf(stderr,"err was %s %d\n",strerror(errno),priv->filesize);
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

  if (!is_partial_clone) cdata->fps=0.;
  cdata->width=cdata->frame_width=cdata->height=cdata->frame_height=0;
  cdata->offs_x=cdata->offs_y=0;

  cdata->arate=0;
  cdata->achans=0;
  cdata->asamps=16;

  fstat(priv->fd,&sb);
  priv->filesize=sb.st_size;

  sprintf(cdata->audio_name,"%s","");

skip_probe:

  priv->idxc=idxc_for(cdata);
  priv->inited=TRUE;

  priv->input_position=0;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  priv->s = avformat_alloc_context();

  memset(&priv->matroska,0,sizeof(priv->matroska));
  priv->matroska.current_id = 0;
  priv->s->priv_data = &priv->matroska;

  av_init_packet(&priv->avpkt);
  priv->avpkt.data=NULL;
  priv->avpkt.size=0;
  priv->ctx=NULL;

  if (lives_mkv_read_header(cdata)) {
    close(priv->fd);
    return FALSE;
  }

  cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;

  cdata->offs_x=0;
  cdata->offs_y=0;

  priv->data_start=priv->input_position;

  if (priv->matroska.duration!=0. && priv->matroska.time_scale!=.0)
    duration = priv->matroska.duration / priv->matroska.time_scale * 1000.;

  //#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"video type is %f %s %d x %d (%d x %d +%d +%d)\n",duration,cdata->video_name,
          cdata->width,cdata->height,cdata->frame_width,cdata->frame_height,cdata->offs_x,cdata->offs_y);
#endif

  codec = avcodec_find_decoder(priv->vidst->codec->codec_id);

  if (!codec) {
    if (strlen(cdata->video_name)>0)
      fprintf(stderr, "mkv_decoder: Could not find avcodec codec for video type %s\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }

  priv->ctx = ctx = avcodec_alloc_context3(codec);

  if (avcodec_open2(ctx, codec, NULL) < 0) {
    fprintf(stderr, "mkv_decoder: Could not open avcodec context for codec\n");
    detach_stream(cdata);
    return FALSE;
  }

  priv->codec=codec;

  if (isclone) return TRUE;

  // re-scan with avcodec; priv->data_start holds video data start position

  av_init_packet(&priv->avpkt);

  priv->picture = av_frame_alloc();

  pthread_mutex_lock(&priv->idxc->mutex);
  matroska_read_seek(cdata,0);
  pthread_mutex_unlock(&priv->idxc->mutex);

  while (!got_picture&&!got_eof) {
    matroska_read_packet(cdata,&priv->avpkt);

    if (got_eof) break;

#if LIBAVCODEC_VERSION_MAJOR >= 52
    avcodec_decode_video2(ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    avcodec_decode_video(ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif
  }

  if (!got_picture) {
    fprintf(stderr,"mkv_decoder: could not get picture.\n PLEASE SEND A PATCH FOR %s FORMAT.\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }

  pts=priv->avpkt.pts;
  dts=priv->avpkt.dts;

  if (priv->avpkt.data!=NULL) {
    free(priv->avpkt.data);
    priv->avpkt.data=NULL;
    priv->avpkt.size=0;
  }

  matroska_read_packet(cdata,&priv->avpkt);

  if (!got_eof) {
#if LIBAVCODEC_VERSION_MAJOR >= 52
    avcodec_decode_video2(ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    avcodec_decode_video(ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif
  }

  if (got_picture) {
    pts=priv->avpkt.pts-pts;
    dts=priv->avpkt.dts-dts;
  } else pts=dts=0;

  if (priv->avpkt.data!=NULL) {
    free(priv->avpkt.data);
    priv->avpkt.data=NULL;
    priv->avpkt.size=0;
  }

  cdata->YUV_clamping=WEED_YUV_CLAMPING_UNCLAMPED;
  if (ctx->color_range==AVCOL_RANGE_MPEG) cdata->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;

  cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT;
  if (ctx->chroma_sample_location!=AVCHROMA_LOC_LEFT) cdata->YUV_sampling=WEED_YUV_SAMPLING_MPEG;

  cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
  if (ctx->colorspace==AVCOL_SPC_BT709) cdata->YUV_subspace=WEED_YUV_SUBSPACE_BT709;


  cdata->palettes[0]=avi_pix_fmt_to_weed_palette(ctx->pix_fmt,
                     &cdata->YUV_clamping);

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

  if (!is_partial_clone&&ctx->time_base.den>0&&ctx->time_base.num>0) {
    fps=(double)ctx->time_base.den/(double)ctx->time_base.num;
    if (fps!=1000.) cdata->fps=fps;
  }

  if (cdata->fps==0.) {
    if (pts!=0) cdata->fps=1000./(double)pts;
    else if (dts!=0) cdata->fps=1000./(double)dts;
  }

  if (cdata->fps==0.||cdata->fps==1000.) {
    // use mplayer to get fps if we can...it seems to have some magical way
    char cmd[1024];
    char tmpfname[32];
    int res;

    sprintf(tmpfname,"mkvdec=XXXXXX");
#ifndef IS_MINGW
    snprintf(cmd,1024,"LANGUAGE=en LANG=en mplayer \"%s\" -identify -frames 0 2>/dev/null | grep ID_VIDEO_FPS > %s",cdata->URI,tmpfname);
#else
    snprintf(cmd,1024,"mplayer.exe \"%s\" -identify -frames 0 2>NUL | grep.exe ID_VIDEO_FPS > %s",cdata->URI,tmpfname);
#endif
    res=system(cmd);

    if (!res) {
      char buffer[1024];
      ssize_t bytes;
      int ofd=open(tmpfname,O_RDONLY);
      if (ofd>-1) {

#ifdef IS_MINGW
        setmode(ofd,O_BINARY);
#endif
        bytes=read(ofd,buffer,1024);
        memset(buffer+bytes,0,1);
        if (!(strncmp(buffer,"ID_VIDEO_FPS=",13))) {
          cdata->fps=strtod(buffer+13,NULL);
        }
        close(ofd);
      }
    }
    unlink(tmpfname);
  }

  if (cdata->fps==0.||cdata->fps==1000.) {
    // if mplayer fails, count the frames between index entries
    dts=calc_dts_delta(cdata);
    if (dts!=0) cdata->fps=1000./(double)dts;
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

  if (is_partial_clone) return TRUE;

  ldts=get_last_video_dts(cdata);

  if (ldts==0) ldts=duration*1000.;

  if (ldts==-1) {
    fprintf(stderr, "mkv_decoder: could not read last dts\n");
    detach_stream(cdata);
    return FALSE;
  }

  cdata->nframes=dts_to_frame(cdata,ldts)+2;

  // double check, sometimes we can be out by one or two frames
  while (1) {
    priv->expect_eof=TRUE;
    got_eof=FALSE;
    get_frame(cdata,cdata->nframes-1,NULL,0,NULL);
    if (!got_eof) break;
    cdata->nframes--;
  }
  priv->expect_eof=FALSE;


#ifdef DEBUG
  fprintf(stderr,"fps is %.4f %ld\n",cdata->fps,cdata->nframes);
#endif

  return TRUE;
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  avcodec_register_all();
  av_log_set_level(AV_LOG_ERROR);
  indices=NULL;
  nidxc=0;
  pthread_mutex_init(&indices_mutex,NULL);
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata(void) {
  lives_mkv_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->URI=NULL;

  cdata->priv=priv=malloc(sizeof(lives_mkv_priv_t));

  cdata->seek_flag=0;

  priv->ctx=NULL;
  priv->codec=NULL;
  priv->picture=NULL;
  priv->inited=FALSE;

  priv->expect_eof=FALSE;

  cdata->palettes=(int *)malloc(2*sizeof(int));
  cdata->palettes[1]=WEED_PALETTE_END;

  cdata->interlace=LIVES_INTERLACE_NONE;

  cdata->nframes=0;

  cdata->sync_hint=0;

  cdata->video_start_time=0.;

  priv->idxc=NULL;

  got_eof=FALSE;
  errval=0;

  memset(cdata->author,0,1);
  memset(cdata->title,0,1);
  memset(cdata->comment,0,1);

  return cdata;
}


static lives_clip_data_t *mkv_clone(lives_clip_data_t *cdata) {
  lives_clip_data_t *clone=init_cdata();
  lives_mkv_priv_t *dpriv,*spriv;

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

  if (spriv==NULL) {
    clone->nclips=1;

    ///////////////////////////////////////////////////////////

    sprintf(clone->container_name,"%s","mkv");

    // clone->height was set when we attached the stream

    if (clone->frame_width==0||clone->frame_width<clone->width) clone->frame_width=clone->width;
    else {
      clone->offs_x=(clone->frame_width-clone->width)/2;
    }

    if (clone->frame_height==0||clone->frame_height<clone->height) clone->frame_height=clone->height;
    else {
      clone->offs_y=(clone->frame_height-clone->height)/2;
    }

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

  dpriv->last_frame=-1;
  dpriv->expect_eof=FALSE;

  return clone;
}




lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different current_clip, this must be called again with the same
  // cdata as the second parameter

  // value returned should be freed with clip_data_free() when no longer required

  // should be thread-safe

  lives_mkv_priv_t *priv;

  if (URI==NULL&&cdata!=NULL) {
    // create a clone of cdata - we also need to be able to handle a "fake" clone with only URI, nframes and fps set (priv == NULL)
    return mkv_clone(cdata);
  }

  got_eof=FALSE;
  errval=0;

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
    cdata->current_palette=cdata->palettes[0];
    cdata->current_clip=0;
  }

  cdata->nclips=1;

  ///////////////////////////////////////////////////////////

  sprintf(cdata->container_name,"%s","mkv");

  // cdata->height was set when we attached the stream

  if (cdata->frame_width==0||cdata->frame_width<cdata->width) cdata->frame_width=cdata->width;
  else {
    cdata->offs_x=(cdata->frame_width-cdata->width)/2;
  }

  if (cdata->frame_height==0||cdata->frame_height<cdata->height) cdata->frame_height=cdata->height;
  else {
    cdata->offs_y=(cdata->frame_height-cdata->height)/2;
  }

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
static void matroska_clear_queue(MatroskaDemuxContext *matroska) {
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
                                int64_t cluster_pos) {

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
    case 0x1: { /* Xiph lacing */
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

    case 0x3: { /* EBML lacing */
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
          memcpy(pkt->data, encodings->compression.settings.data, offset);

        memcpy(pkt->data+offset, pkt_data, pkt_size);

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
          lives_dynarray_add(&matroska->packets,&matroska->num_packets,pkt);
          matroska->prev_pkt = pkt;
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


static int matroska_parse_cluster(const lives_clip_data_t *cdata) {

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




static index_entry *matroska_read_seek(const lives_clip_data_t *cdata,
                                       uint32_t timestamp) {

  lives_mkv_priv_t *priv=cdata->priv;
  MatroskaDemuxContext *matroska=&priv->matroska;
  //AVFormatContext *s=priv->s;

  //AVStream *st = priv->vidst;

  index_entry *idx;

  // lock idxc_mutex before calling

  if (!priv->idxc->idxhh) {
    return NULL;
  }

  if (timestamp!=0) {
    timestamp = FFMIN(timestamp, frame_to_dts(cdata,cdata->nframes));
    timestamp = FFMAX(timestamp, priv->idxc->idxhh->dts);
  }

  idx=get_idx_for_pts(cdata,timestamp);

  matroska_clear_queue(matroska);

  priv->input_position=idx->offs;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  if (priv->avpkt.data!=NULL) {
    free(priv->avpkt.data);
    priv->avpkt.data=NULL;
    priv->avpkt.size=0;
  }

  //printf("2seeking to %ld\n",priv->input_position);

  matroska->current_id = 0;
  matroska->skip_to_keyframe = 1;
  matroska->skip_to_timecode = idx->dts;
  matroska->done = 0;

  //ff_update_cur_dts(s, st, idx->dts);

  return idx;
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
  int xheight=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;
  int rescan_limit=16;  // pick some arbitrary value
  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;
  boolean got_picture=FALSE;
  unsigned char *dst,*src;
  unsigned char black[4]= {0,0,0,255};
  index_entry *idx;
  register int i,p;

  got_eof=FALSE;

#ifdef DEBUG_KFRAMES
  fprintf(stderr,"vals %ld %ld\n",tframe,priv->last_frame);
#endif

  // calc frame width and height, including any border

  if (pixel_data!=NULL) {
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

    if (priv->last_frame==-1 || (tframe<priv->last_frame) || (tframe - priv->last_frame > rescan_limit)) {
      pthread_mutex_lock(&priv->idxc->mutex);
      idx=matroska_read_seek(cdata,target_pts);
      pthread_mutex_unlock(&priv->idxc->mutex);
      nextframe=dts_to_frame(cdata,idx->dts);
      if (got_eof) return FALSE;
      avcodec_flush_buffers(priv->ctx);
#ifdef DEBUG_KFRAMES
      if (idx!=NULL) printf("got kframe %ld for frame %ld\n",dts_to_frame(cdata,idx->dts),tframe);
#endif
    } else {
      nextframe=priv->last_frame+1;
    }

    //priv->ctx->skip_frame=AVDISCARD_NONREF;

    priv->last_frame=tframe;
    if (priv->picture==NULL) priv->picture = av_frame_alloc();

    // do this until we reach target frame //////////////


    do {

      got_picture=FALSE;

      while (!got_picture) {

        if (priv->avpkt.data!=NULL) {
          free(priv->avpkt.data);
          priv->avpkt.data=NULL;
          priv->avpkt.size=0;
        }

        matroska_read_packet(cdata,&priv->avpkt);

        if (got_eof) return FALSE;

#if LIBAVCODEC_VERSION_MAJOR >= 52
        avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
        avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif
      }

      nextframe++;

      if (nextframe>cdata->nframes) return FALSE;
    } while (nextframe<=tframe);

    /////////////////////////////////////////////////////

  }

  if (priv->picture==NULL||pixel_data==NULL) return TRUE;

  for (p=0; p<nplanes; p++) {
    dst=pixel_data[p];
    src=priv->picture->data[p];

    for (i=0; i<xheight; i++) {
      if (i<btop||i>bbot) {
        // top or bottom border, copy black row
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||
            pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
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
  lives_mkv_priv_t *priv=cdata->priv;

  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;

  if (priv->idxc!=NULL) idxc_release(cdata);
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
