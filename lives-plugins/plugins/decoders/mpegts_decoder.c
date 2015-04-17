// LiVES - mpegts decoder plugin
// (c) G. Finch 2012 - 2014 <salsaman@gmail.com>

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

/* based on code from libavformat
 * Copyright (c) 2002-2003 Fabrice Bellard
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#if !defined (IS_MINGW) && !defined (IS_SOLARIS) && !defined (__FreeBSD__)
#include <endian.h>
#endif

const char *plugin_version="LiVES mpegts decoder version 1.2a";

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


#include "decplugin.h"


#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/crc.h>
#include <libavutil/avstring.h>
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>

#include <pthread.h>

#include "libav_helper.h"

#include "mpegts_decoder.h"

static index_container_t **indices;
static int nidxc;
static pthread_mutex_t indices_mutex;

static void mpegts_save_index(lives_clip_data_t *);

/**
 * Read 1-25 bits.
 */
static inline unsigned int get_bits(GetBitContext *s, int n) {
  register int tmp;
  OPEN_READER(re, s);
  UPDATE_CACHE(re, s);
  tmp = SHOW_UBITS(re, s, n);
  LAST_SKIP_BITS(re, s, n);
  CLOSE_READER(re, s);
  return tmp;
}



static inline unsigned int get_bits1(GetBitContext *s) {
#ifdef ALT_BITSTREAM_READER
  unsigned int index = s->index;
  uint8_t result = s->buffer[index>>3];
#ifdef ALT_BITSTREAM_READER_LE
  result >>= index & 7;
  result &= 1;
#else
  result <<= index & 7;
  result >>= 8 - 1;
#endif
  index++;
  s->index = index;

  return result;
#else
  return get_bits(s, 1);
#endif
}



static inline void skip_bits(GetBitContext *s, int n) {
  //Note gcc seems to optimize this to s->index+=n for the ALT_READER :))
  OPEN_READER(re, s);
  UPDATE_CACHE(re, s);
  LAST_SKIP_BITS(re, s, n);
  CLOSE_READER(re, s);
}


/**
 * init GetBitContext.
 * @param buffer bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE bytes larger than the actual read bits
 * because some optimized bitstream readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 *
 * While GetBitContext stores the buffer size, for performance reasons you are
 * responsible for checking for the buffer end yourself (take advantage of the padding)!
 */
static inline void init_get_bits(GetBitContext *s,
                                 const uint8_t *buffer, int bit_size) {
  int buffer_size = (bit_size+7)>>3;
  if (buffer_size < 0 || bit_size < 0) {
    buffer_size = bit_size = 0;
    buffer = NULL;
  }

  s->buffer       = buffer;
  s->size_in_bits = bit_size;
  s->buffer_end   = buffer + buffer_size;
#ifdef ALT_BITSTREAM_READER
  s->index        = 0;
#elif defined A32_BITSTREAM_READER
  s->buffer_ptr   = (uint32_t *)((intptr_t)buffer & ~3);
  s->bit_count    = 32 +     8*((intptr_t)buffer &  3);
  skip_bits_long(s, 0);
#endif
}



static void index_free(index_entry *idx) {
  index_entry *cidx=idx,*next;

  while (cidx!=NULL) {
    next=cidx->next;
    free(cidx);
    cidx=next;
  }
}

//////////////////////////////////////////////////////////////////////////

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


index_entry *lives_add_idx(const lives_clip_data_t *cdata, uint64_t offset, int64_t pts) {
  lives_mpegts_priv_t *priv=cdata->priv;
  index_entry *nidx=priv->idxc->idxht;
  index_entry *nentry;

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



static index_entry *get_idx_for_pts(const lives_clip_data_t *cdata, int64_t pts) {
  lives_mpegts_priv_t *priv=cdata->priv;
  return index_walk(priv->idxc->idxhh,pts);
}


//////////////////////////////////////////////


static boolean check_for_eof(lives_clip_data_t *cdata) {
  lives_mpegts_priv_t *priv=cdata->priv;
  if (priv->input_position>priv->filesize) {
    priv->got_eof=TRUE;
    return TRUE;
  }
  return FALSE;
}


/* maximum size in which we look for synchronisation if
   synchronisation is lost */
#define MAX_RESYNC_SIZE 65536

#define MAX_PES_PAYLOAD 200*1024

#define MAX_MP4_DESCR_COUNT 16

enum MpegTSFilterType {
  MPEGTS_PES,
  MPEGTS_SECTION,
};

typedef struct MpegTSFilter MpegTSFilter;

typedef int PESCallback(lives_clip_data_t *cdata, MpegTSFilter *f, const uint8_t *buf, int len, int is_start, int64_t pos);

typedef struct MpegTSPESFilter {
  PESCallback *pes_cb;
  void *opaque;
} MpegTSPESFilter;

typedef void SectionCallback(lives_clip_data_t *cdata, MpegTSFilter *f, const uint8_t *buf, int len);

typedef void SetServiceCallback(void *opaque, int ret);

typedef struct MpegTSSectionFilter {
  int section_index;
  int section_h_size;
  uint8_t *section_buf;
  unsigned int check_crc:1;
  unsigned int end_of_section_reached:1;
  SectionCallback *section_cb;
  void *opaque;
} MpegTSSectionFilter;

struct MpegTSFilter {
  int pid;
  int es_id;
  int last_cc; /* last cc code (-1 if first packet) */
  enum MpegTSFilterType type;
  union {
    MpegTSPESFilter pes_filter;
    MpegTSSectionFilter section_filter;
  } u;
};

#define MAX_PIDS_PER_PROGRAM 64
struct Program {
  unsigned int id; //program id/service id
  unsigned int nb_pids;
  unsigned int pids[MAX_PIDS_PER_PROGRAM];
};

struct MpegTSContext {
  const AVClass *class;
  /* user data */
  AVFormatContext *stream;
  /** raw packet size, including FEC if present            */
  int raw_packet_size;

  int pos47;

  /** if true, all pids are analyzed to find streams       */
  int auto_guess;

  /** compute exact PCR for each transport stream packet   */
  int mpeg2ts_compute_pcr;

  int64_t cur_pcr;    /**< used to estimate the exact PCR  */
  int pcr_incr;       /**< used to estimate the exact PCR  */

  /* data needed to handle file based ts */
  /** stop parsing loop                                    */
  int stop_parse;
  /** packet containing Audio/Video data                   */
  AVPacket *pkt;
  /** to detect seek                                       */
  int64_t last_pos;

  /******************************************/
  /* private mpegts data */
  /* scan context */
  /** structure to keep track of Program->pids mapping     */
  unsigned int nb_prg;
  struct Program *prg;


  /** filters for various streams specified by PMT + for the PAT and PMT */
  MpegTSFilter *pids[NB_PID_MAX];
};

static const AVOption options[] = {
  {
    "compute_pcr", "Compute exact PCR for each transport stream packet.", offsetof(MpegTSContext, mpeg2ts_compute_pcr), AV_OPT_TYPE_INT,
    {.dbl = 0}, 0, 1, AV_OPT_FLAG_DECODING_PARAM
  },
  { NULL },
};

/* TS stream handling */

enum MpegTSState {
  MPEGTS_HEADER = 0,
  MPEGTS_PESHEADER,
  MPEGTS_PESHEADER_FILL,
  MPEGTS_PAYLOAD,
  MPEGTS_SKIP,
};

/* enough for PES header + length */
#define PES_START_SIZE  6
#define PES_HEADER_SIZE 9
#define MAX_PES_HEADER_SIZE (9 + 255)

typedef struct PESContext {
  int pid;
  int pcr_pid; /**< if -1 then all packets containing PCR are considered */
  int stream_type;
  MpegTSContext *ts;
  AVFormatContext *stream;
  AVStream *st;
  AVStream *sub_st; /**< stream for the embedded AC3 stream in HDMV TrueHD */
  enum MpegTSState state;
  /* used to get the format */
  int data_index;
  int flags; /**< copied to the AVPacket flags */
  int total_size;
  int pes_header_size;
  int extended_stream_id;
  int64_t pts, dts;
  int64_t ts_packet_pos; /**< position of first TS packet of this PES packet */
  uint8_t header[MAX_PES_HEADER_SIZE];
  uint8_t *buffer;
  SLConfigDescr sl;
} PESContext;

//extern AVInputFormat ff_mpegts_demuxer;

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


static boolean lives_seek(lives_clip_data_t *cdata, int fd, off_t pos) {
  // seek

  lives_mpegts_priv_t *priv=cdata->priv;
  if (fd==priv->fd) {
    priv->input_position = pos;
    check_for_eof(cdata);
    if (lseek(priv->fd, priv->input_position, SEEK_SET)==-1) {
      return FALSE;
    }
  } else {
    struct stat sb;
    fstat(fd,&sb);
    if (pos>sb.st_size) priv->got_eof=TRUE;
    if (lseek(fd, pos, SEEK_SET)==-1) return FALSE;
  }
  return TRUE;
}


static boolean lives_skip(lives_clip_data_t *cdata, int fd, off_t offs) {
  // skip

  lives_mpegts_priv_t *priv=cdata->priv;
  if (fd==priv->fd) {
    priv->input_position += offs;
    check_for_eof(cdata);
    if (lseek(priv->fd, priv->input_position, SEEK_SET)==-1) {
      return FALSE;
    }
  } else {
    off_t pos=lseek(fd,0,SEEK_CUR)+offs;
    return lives_seek(cdata,fd,pos);
  }
  return TRUE;
}



ssize_t lives_read(lives_clip_data_t *cdata, int fd, void *data, size_t len) {
  // read fd

  lives_mpegts_priv_t *priv=cdata->priv;
  ssize_t bytes=read(fd,data,len);
  if (bytes>=0 && fd==priv->fd) priv->input_position+=len;
  check_for_eof(cdata);
  return bytes;
}


static inline uint16_t lives_r8(const uint8_t *x) {
  return x[0];
}


static uint8_t lives_rf8(lives_clip_data_t *cdata, int fd) {
  uint8_t buf;
  lives_read(cdata,fd,&buf,1);
  return buf;
}



static inline uint16_t lives_rb16(const uint8_t *x) {
  return (x[0]<<8) + x[1];
}

static uint16_t lives_rbf16(lives_clip_data_t *cdata, int fd) {
  uint8_t buf[2];
  lives_read(cdata,fd,&buf,2);
  return lives_rb16(buf);
}


static inline uint16_t lives_rb24(const uint8_t *x) {
  return (((x[0]<<8) + x[1])<<8) + x[2];
}


static inline uint16_t lives_rb32(const uint8_t *x) {
  return (((((x[0]<<8) + x[1])<<8) + x[2])<<8) + x[3];
}


static uint16_t lives_rbf32(lives_clip_data_t *cdata, int fd) {
  uint8_t buf[4];
  lives_read(cdata,fd,&buf,4);
  return lives_rb32(buf);
}


static inline uint16_t lives_rl32(const char *x) {
  return (((((x[3]<<8) + x[2])<<8) + x[1])<<8) + x[0];
}




/**
 * Parse MPEG-PES five-byte timestamp
 */
static inline int64_t ff_parse_pes_pts(const uint8_t *buf) {
  return (int64_t)(*buf & 0x0e) << 29 |
         (lives_rb16(buf+1) >> 1) << 15 |
         lives_rb16(buf+3) >> 1;
}


int ff_find_stream_index(AVFormatContext *s, int id) {
  int i;
  for (i = 0; i < s->nb_streams; i++) {
    if (s->streams[i]->id == id)
      return i;
  }
  return -1;
}


void ff_mp4_parse_es_descr(lives_clip_data_t *cdata, int fd, int *es_id) {
  lives_mpegts_priv_t *priv=cdata->priv;
  int flags;

  if (es_id) *es_id = lives_rbf16(cdata,fd);
  else                lives_rbf16(cdata,fd);
  flags = lives_rf8(cdata,fd);
  if (flags & 0x80) //streamDependenceFlag
    lives_rbf16(cdata,fd);
  if (flags & 0x40) { //URL_Flag
    int len = lives_rf8(cdata,fd);
    lives_skip(cdata, fd, len);
  }
  if (flags & 0x20) //OCRstreamFlag
    lives_rbf16(cdata,priv->fd);
}


static void clear_program(MpegTSContext *ts, unsigned int programid) {
  int i;

  for (i=0; i<ts->nb_prg; i++)
    if (ts->prg[i].id == programid)
      ts->prg[i].nb_pids = 0;
}

static void clear_programs(MpegTSContext *ts) {
  av_freep(&ts->prg);
  ts->nb_prg=0;
}

static void add_pat_entry(MpegTSContext *ts, unsigned int programid) {
  struct Program *p;
  void *tmp = av_realloc(ts->prg, (ts->nb_prg+1)*sizeof(struct Program));
  if (!tmp)
    return;
  ts->prg = tmp;
  p = &ts->prg[ts->nb_prg];
  p->id = programid;
  p->nb_pids = 0;
  ts->nb_prg++;
}

static void add_pid_to_pmt(MpegTSContext *ts, unsigned int programid, unsigned int pid) {
  int i;
  struct Program *p = NULL;
  for (i=0; i<ts->nb_prg; i++) {
    if (ts->prg[i].id == programid) {
      p = &ts->prg[i];
      break;
    }
  }
  if (!p)
    return;

  if (p->nb_pids >= MAX_PIDS_PER_PROGRAM)
    return;

  //fprintf(stderr,"adding pid %d\n",pid);
  p->pids[p->nb_pids++] = pid;
}

static void set_pcr_pid(AVFormatContext *s, unsigned int programid, unsigned int pid) {
  int i;
  for (i=0; i<s->nb_programs; i++) {
    if (s->programs[i]->id == programid) {
      //s->programs[i]->pcr_pid = pid;
      break;
    }
  }
}

/**
 * @brief discard_pid() decides if the pid is to be discarded according
 *                      to caller's programs selection
 * @param ts    : - TS context
 * @param pid   : - pid
 * @return 1 if the pid is only comprised in programs that have .discard=AVDISCARD_ALL
 *         0 otherwise
 */
static int discard_pid(MpegTSContext *ts, unsigned int pid) {
  int i, j, k;
  int used = 0, discarded = 0;
  struct Program *p;
  for (i=0; i<ts->nb_prg; i++) {
    p = &ts->prg[i];
    for (j=0; j<p->nb_pids; j++) {
      if (p->pids[j] != pid)
        continue;
      //is program with id p->id set to be discarded?
      for (k=0; k<ts->stream->nb_programs; k++) {
        if (ts->stream->programs[k]->id == p->id) {
          if (ts->stream->programs[k]->discard == AVDISCARD_ALL)
            discarded++;
          else
            used++;
        }
      }
    }
  }

  return !used && discarded;
}

/**
 *  Assemble PES packets out of TS packets, and then call the "section_cb"
 *  function when they are complete.
 */
static void write_section_data(lives_clip_data_t *cdata, AVFormatContext *s, MpegTSFilter *tss1,
                               const uint8_t *buf, int buf_size, int is_start) {
  MpegTSSectionFilter *tss = &tss1->u.section_filter;
  int len;

  //fprintf(stderr,"pt a123\n");

  if (is_start) {
    memcpy(tss->section_buf, buf, buf_size);
    tss->section_index = buf_size;
    tss->section_h_size = -1;
    tss->end_of_section_reached = 0;
  } else {
    if (tss->end_of_section_reached)
      return;
    len = 4096 - tss->section_index;
    if (buf_size < len)
      len = buf_size;
    memcpy(tss->section_buf + tss->section_index, buf, len);
    tss->section_index += len;
  }

  /* compute section length if possible */
  if (tss->section_h_size == -1 && tss->section_index >= 3) {
    len = (lives_rb16(tss->section_buf + 1) & 0xfff) + 3;
    if (len > 4096)
      return;
    tss->section_h_size = len;
  }

  if (tss->section_h_size != -1 && tss->section_index >= tss->section_h_size) {
    tss->end_of_section_reached = 1;
    if (!tss->check_crc ||
        av_crc(av_crc_get_table(AV_CRC_32_IEEE), -1,
               tss->section_buf, tss->section_h_size) == 0)
      tss->section_cb(cdata, tss1, tss->section_buf, tss->section_h_size);
  }
}

static MpegTSFilter *mpegts_open_section_filter(MpegTSContext *ts, unsigned int pid,
    SectionCallback *section_cb, void *opaque,
    int check_crc)

{
  MpegTSFilter *filter;
  MpegTSSectionFilter *sec;

  if (pid >= NB_PID_MAX || ts->pids[pid])
    return NULL;
  filter = av_mallocz(sizeof(MpegTSFilter));
  if (!filter)
    return NULL;

  //fprintf(stderr,"adding filter %d %p\n",pid,filter);
  ts->pids[pid] = filter;
  filter->type = MPEGTS_SECTION;
  filter->pid = pid;
  filter->es_id = -1;
  filter->last_cc = -1;
  sec = &filter->u.section_filter;
  sec->section_cb = section_cb;
  sec->opaque = opaque;
  sec->section_buf = av_malloc(MAX_SECTION_SIZE);
  sec->check_crc = check_crc;
  if (!sec->section_buf) {
    av_free(filter);
    return NULL;
  }
  return filter;
}

static MpegTSFilter *mpegts_open_pes_filter(MpegTSContext *ts, unsigned int pid,
    PESCallback *pes_cb,
    void *opaque) {
  MpegTSFilter *filter;
  MpegTSPESFilter *pes;

  if (pid >= NB_PID_MAX || ts->pids[pid])
    return NULL;
  filter = av_mallocz(sizeof(MpegTSFilter));
  if (!filter)
    return NULL;
  //fprintf(stderr,"2adding filter %p\n",filter);
  ts->pids[pid] = filter;
  filter->type = MPEGTS_PES;
  filter->pid = pid;
  filter->es_id = -1;
  filter->last_cc = -1;
  pes = &filter->u.pes_filter;
  pes->pes_cb = pes_cb;
  pes->opaque = opaque;
  return filter;
}

static void mpegts_close_filter(MpegTSContext *ts, MpegTSFilter *filter) {
  int pid;

  pid = filter->pid;
  if (filter->type == MPEGTS_SECTION)
    av_freep(&filter->u.section_filter.section_buf);
  else if (filter->type == MPEGTS_PES) {
    PESContext *pes = filter->u.pes_filter.opaque;
    av_freep(&pes->buffer);
    /* referenced private data will be freed later in
     * av_close_input_stream */
    if (!((PESContext *)filter->u.pes_filter.opaque)->st) {
      av_freep(&filter->u.pes_filter.opaque);
    }
  }

  av_free(filter);
  ts->pids[pid] = NULL;
}

static int analyze(const uint8_t *buf, int size, int packet_size, int *index) {
  int stat[TS_MAX_PACKET_SIZE];
  int i;
  int x=0;
  int best_score=0;

  memset(stat, 0, packet_size*sizeof(int));

  for (x=i=0; i<size-3; i++) {
    if (buf[i] == 0x47 && !(buf[i+1] & 0x80) && buf[i+3] != 0x47) {
      stat[x]++;
      if (stat[x] > best_score) {
        best_score= stat[x];
        if (index) *index= x;
      }
    }

    x++;
    if (x == packet_size) x= 0;
  }

  return best_score;
}

/* autodetect fec presence. Must have at least 1024 bytes  */
static int get_packet_size(const uint8_t *buf, int size) {
  int score, fec_score, dvhs_score;

  if (size < (TS_FEC_PACKET_SIZE * 5 + 1))
    return -1;

  score    = analyze(buf, size, TS_PACKET_SIZE, NULL);
  dvhs_score    = analyze(buf, size, TS_DVHS_PACKET_SIZE, NULL);
  fec_score= analyze(buf, size, TS_FEC_PACKET_SIZE, NULL);
  //    av_log(NULL, AV_LOG_DEBUG, "score: %d, dvhs_score: %d, fec_score: %d \n", score, dvhs_score, fec_score);

  if (score > fec_score && score > dvhs_score) return TS_PACKET_SIZE;
  else if (dvhs_score > score && dvhs_score > fec_score) return TS_DVHS_PACKET_SIZE;
  else if (score < fec_score && dvhs_score < fec_score) return TS_FEC_PACKET_SIZE;
  else                       return -1;
}

typedef struct SectionHeader {
  uint8_t tid;
  uint16_t id;
  uint8_t version;
  uint8_t sec_num;
  uint8_t last_sec_num;
} SectionHeader;

static inline int get8(const uint8_t **pp, const uint8_t *p_end) {
  const uint8_t *p;
  int c;

  p = *pp;
  if (p >= p_end)
    return -1;
  c = *p++;
  *pp = p;
  return c;
}

static inline int get16(const uint8_t **pp, const uint8_t *p_end) {
  const uint8_t *p;
  int c;

  p = *pp;
  if ((p + 1) >= p_end)
    return -1;
  c = lives_rb16(p);
  p += 2;
  *pp = p;
  return c;
}

/* read and allocate a DVB string preceded by its length */
static char *getstr8(const uint8_t **pp, const uint8_t *p_end) {
  int len;
  const uint8_t *p;
  char *str;

  p = *pp;
  len = get8(&p, p_end);
  if (len < 0)
    return NULL;
  if ((p + len) > p_end)
    return NULL;
  str = av_malloc(len + 1);
  if (!str)
    return NULL;
  memcpy(str, p, len);
  str[len] = '\0';
  p += len;
  *pp = p;
  return str;
}

static int parse_section_header(SectionHeader *h,
                                const uint8_t **pp, const uint8_t *p_end) {
  int val;

  val = get8(pp, p_end);
  if (val < 0)
    return -1;
  h->tid = val;
  *pp += 2;
  val = get16(pp, p_end);
  if (val < 0)
    return -1;
  h->id = val;
  val = get8(pp, p_end);
  if (val < 0)
    return -1;
  h->version = (val >> 1) & 0x1f;
  val = get8(pp, p_end);
  if (val < 0)
    return -1;
  h->sec_num = val;
  val = get8(pp, p_end);
  if (val < 0)
    return -1;
  h->last_sec_num = val;
  return 0;
}

typedef struct {
  uint32_t stream_type;
  enum AVMediaType codec_type;
  enum CodecID codec_id;
} StreamType;

static const StreamType ISO_types[] = {
  { 0x01, AVMEDIA_TYPE_VIDEO, CODEC_ID_MPEG2VIDEO },
  { 0x02, AVMEDIA_TYPE_VIDEO, CODEC_ID_MPEG2VIDEO },
  { 0x03, AVMEDIA_TYPE_AUDIO,        CODEC_ID_MP3 },
  { 0x04, AVMEDIA_TYPE_AUDIO,        CODEC_ID_MP3 },
  { 0x0f, AVMEDIA_TYPE_AUDIO,        CODEC_ID_AAC },
  { 0x10, AVMEDIA_TYPE_VIDEO,      CODEC_ID_MPEG4 },
  /* Makito encoder sets stream type 0x11 for AAC,
   * so auto-detect LOAS/LATM instead of hardcoding it. */
  //  { 0x11, AVMEDIA_TYPE_AUDIO,   CODEC_ID_AAC_LATM }, /* LATM syntax */
  { 0x1b, AVMEDIA_TYPE_VIDEO,       CODEC_ID_H264 },
  { 0xd1, AVMEDIA_TYPE_VIDEO,      CODEC_ID_DIRAC },
  { 0xea, AVMEDIA_TYPE_VIDEO,        CODEC_ID_VC1 },
  { 0 },
};

static const StreamType HDMV_types[] = {
  { 0x80, AVMEDIA_TYPE_AUDIO, CODEC_ID_PCM_BLURAY },
  { 0x81, AVMEDIA_TYPE_AUDIO, CODEC_ID_AC3 },
  { 0x82, AVMEDIA_TYPE_AUDIO, CODEC_ID_DTS },
  { 0x83, AVMEDIA_TYPE_AUDIO, CODEC_ID_TRUEHD },
  { 0x84, AVMEDIA_TYPE_AUDIO, CODEC_ID_EAC3 },
  { 0x85, AVMEDIA_TYPE_AUDIO, CODEC_ID_DTS }, /* DTS HD */
  { 0x86, AVMEDIA_TYPE_AUDIO, CODEC_ID_DTS }, /* DTS HD MASTER*/
  { 0xa1, AVMEDIA_TYPE_AUDIO, CODEC_ID_EAC3 }, /* E-AC3 Secondary Audio */
  { 0xa2, AVMEDIA_TYPE_AUDIO, CODEC_ID_DTS },  /* DTS Express Secondary Audio */
  { 0x90, AVMEDIA_TYPE_SUBTITLE, CODEC_ID_HDMV_PGS_SUBTITLE },
  { 0 },
};

/* ATSC ? */
static const StreamType MISC_types[] = {
  { 0x81, AVMEDIA_TYPE_AUDIO,   CODEC_ID_AC3 },
  { 0x8a, AVMEDIA_TYPE_AUDIO,   CODEC_ID_DTS },
  { 0 },
};

static const StreamType REGD_types[] = {
  { MKTAG('d','r','a','c'), AVMEDIA_TYPE_VIDEO, CODEC_ID_DIRAC },
  { MKTAG('A','C','-','3'), AVMEDIA_TYPE_AUDIO,   CODEC_ID_AC3 },
  { MKTAG('B','S','S','D'), AVMEDIA_TYPE_AUDIO, CODEC_ID_S302M },
  { MKTAG('D','T','S','1'), AVMEDIA_TYPE_AUDIO,   CODEC_ID_DTS },
  { MKTAG('D','T','S','2'), AVMEDIA_TYPE_AUDIO,   CODEC_ID_DTS },
  { MKTAG('D','T','S','3'), AVMEDIA_TYPE_AUDIO,   CODEC_ID_DTS },
  { MKTAG('V','C','-','1'), AVMEDIA_TYPE_VIDEO,   CODEC_ID_VC1 },
  { 0 },
};

/* descriptor present */
static const StreamType DESC_types[] = {
  { 0x6a, AVMEDIA_TYPE_AUDIO,             CODEC_ID_AC3 }, /* AC-3 descriptor */
  { 0x7a, AVMEDIA_TYPE_AUDIO,            CODEC_ID_EAC3 }, /* E-AC-3 descriptor */
  { 0x7b, AVMEDIA_TYPE_AUDIO,             CODEC_ID_DTS },
  { 0x56, AVMEDIA_TYPE_SUBTITLE, CODEC_ID_DVB_TELETEXT },
  { 0x59, AVMEDIA_TYPE_SUBTITLE, CODEC_ID_DVB_SUBTITLE }, /* subtitling descriptor */
  { 0 },
};

static void mpegts_find_stream_type(AVStream *st,
                                    uint32_t stream_type, const StreamType *types) {
  for (; types->stream_type; types++) {
    if (stream_type == types->stream_type) {
      st->codec->codec_type = types->codec_type;
      st->codec->codec_id   = types->codec_id;
      return;
    }
  }
}

static int mpegts_set_stream_info(lives_clip_data_t *cdata, AVStream *st, PESContext *pes,
                                  uint32_t stream_type, uint32_t prog_reg_desc) {
  lives_mpegts_priv_t *priv=cdata->priv;
  int old_codec_type= st->codec->codec_type;
  int old_codec_id  = st->codec->codec_id;
  av_set_pts_info(st, 33, 1, 90000);
  st->priv_data = pes;
  st->codec->codec_type = AVMEDIA_TYPE_DATA;
  st->codec->codec_id   = CODEC_ID_NONE;
  st->need_parsing = AVSTREAM_PARSE_FULL;
  pes->st = st;
  pes->stream_type = stream_type;

  /*  fprintf(stderr,
      "stream=%d stream_type=%x pid=%x prog_reg_desc=%.4s\n",
      st->index, pes->stream_type, pes->pid, (char*)&prog_reg_desc);*/

  st->codec->codec_tag = pes->stream_type;

  if (st->codec->codec_tag==STREAM_TYPE_VIDEO_MPEG1||
      st->codec->codec_tag==STREAM_TYPE_VIDEO_MPEG2||
      st->codec->codec_tag==STREAM_TYPE_VIDEO_MPEG4||
      st->codec->codec_tag==STREAM_TYPE_VIDEO_H264||
      st->codec->codec_tag==STREAM_TYPE_VIDEO_VC1||
      st->codec->codec_tag==STREAM_TYPE_VIDEO_DIRAC) {
    //    fprintf(stderr,"got our vidst %d %p\n",st->codec->codec_tag,st);
    priv->vidst=st;
  }

  if (st->codec->codec_tag==STREAM_TYPE_AUDIO_MPEG1) sprintf(cdata->audio_name,"%s","mpeg1");
  else if (st->codec->codec_tag==STREAM_TYPE_AUDIO_MPEG2) sprintf(cdata->audio_name,"%s","mpeg2");
  else if (st->codec->codec_tag==STREAM_TYPE_AUDIO_AAC) sprintf(cdata->audio_name,"%s","aac");
  else if (st->codec->codec_tag==STREAM_TYPE_AUDIO_AAC_LATM) sprintf(cdata->audio_name,"%s","aac_latm");
  else if (st->codec->codec_tag==STREAM_TYPE_AUDIO_AC3) sprintf(cdata->audio_name,"%s","ac3");
  else if (st->codec->codec_tag==STREAM_TYPE_AUDIO_DTS) sprintf(cdata->audio_name,"%s","dts");

  mpegts_find_stream_type(st, pes->stream_type, ISO_types);


  if ((prog_reg_desc == lives_rl32("HDMV") ||
       prog_reg_desc == lives_rl32("HDPR")) &&
      st->codec->codec_id == CODEC_ID_NONE) {
    mpegts_find_stream_type(st, pes->stream_type, HDMV_types);
    if (pes->stream_type == 0x83) {
      // HDMV TrueHD streams also contain an AC3 coded version of the
      // audio track - add a second stream for this
      AVStream *sub_st;
      // priv_data cannot be shared between streams
      PESContext *sub_pes = av_malloc(sizeof(*sub_pes));
      if (!sub_pes)
        return AVERROR(ENOMEM);
      memcpy(sub_pes, pes, sizeof(*sub_pes));

      sub_st = av_new_stream(pes->stream, pes->pid);

      if (!sub_st) {
        av_free(sub_pes);
        return AVERROR(ENOMEM);
      }

      sub_st->id = pes->pid;
      av_set_pts_info(sub_st, 33, 1, 90000);
      sub_st->priv_data = sub_pes;
      sub_st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
      sub_st->codec->codec_id   = CODEC_ID_AC3;
      sub_st->need_parsing = AVSTREAM_PARSE_FULL;
      sub_pes->sub_st = pes->sub_st = sub_st;
    }
  }
  if (st->codec->codec_id == CODEC_ID_NONE)
    mpegts_find_stream_type(st, pes->stream_type, MISC_types);
  if (st->codec->codec_id == CODEC_ID_NONE) {
    st->codec->codec_id  = old_codec_id;
    st->codec->codec_type= old_codec_type;
  }

  return 0;
}

static void new_pes_packet(PESContext *pes, AVPacket *pkt) {
  av_init_packet(pkt);

  pkt->destruct = av_destruct_packet;
  pkt->data = pes->buffer;
  pkt->size = pes->data_index;

  if (pes->total_size != MAX_PES_PAYLOAD &&
      pes->pes_header_size + pes->data_index != pes->total_size + 6) {
    fprintf(stderr, "mpegts_decoder: PES packet size mismatch\n");
    //pes->flags |= AV_PKT_FLAG_CORRUPT;
  }
  memset(pkt->data+pkt->size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

  // Separate out the AC3 substream from an HDMV combined TrueHD/AC3 PID
  if (pes->sub_st && pes->stream_type == 0x83 && pes->extended_stream_id == 0x76)
    pkt->stream_index = pes->sub_st->index;
  else
    pkt->stream_index = pes->st->index;
  pkt->pts = pes->pts;
  pkt->dts = pes->dts;
  /* store position of first TS packet of this PES packet */
  pkt->pos = pes->ts_packet_pos;
  pkt->flags = pes->flags;

  /* reset pts values */
  pes->pts = AV_NOPTS_VALUE;
  pes->dts = AV_NOPTS_VALUE;
  pes->buffer = NULL;
  pes->data_index = 0;
  pes->flags = 0;
}

static uint64_t get_bits64(GetBitContext *gb, int bits) {
  uint64_t ret = 0;
  while (bits > 17) {
    ret <<= 17;
    ret |= get_bits(gb, 17);
    bits -= 17;
  }
  ret <<= bits;
  ret |= get_bits(gb, bits);
  return ret;
}

static int read_sl_header(PESContext *pes, SLConfigDescr *sl, const uint8_t *buf, int buf_size) {
  GetBitContext gb;
  int au_start_flag = 0, au_end_flag = 0, ocr_flag = 0, idle_flag = 0;
  int padding_flag = 0, padding_bits = 0, inst_bitrate_flag = 0;
  int dts_flag = -1, cts_flag = -1;
  int64_t dts = AV_NOPTS_VALUE, cts = AV_NOPTS_VALUE;
  init_get_bits(&gb, buf, buf_size*8);

  if (sl->use_au_start)
    au_start_flag = get_bits1(&gb);
  if (sl->use_au_end)
    au_end_flag = get_bits1(&gb);
  if (!sl->use_au_start && !sl->use_au_end)
    au_start_flag = au_end_flag = 1;
  if (sl->ocr_len > 0)
    ocr_flag = get_bits1(&gb);
  if (sl->use_idle)
    idle_flag = get_bits1(&gb);
  if (sl->use_padding)
    padding_flag = get_bits1(&gb);
  if (padding_flag)
    padding_bits = get_bits(&gb, 3);

  if (!idle_flag && (!padding_flag || padding_bits != 0)) {
    if (sl->packet_seq_num_len)
      skip_bits_long(&gb, sl->packet_seq_num_len);
    if (sl->degr_prior_len)
      if (get_bits1(&gb))
        skip_bits(&gb, sl->degr_prior_len);
    if (ocr_flag)
      skip_bits_long(&gb, sl->ocr_len);
    if (au_start_flag) {
      if (sl->use_rand_acc_pt)
        get_bits1(&gb);
      if (sl->au_seq_num_len > 0)
        skip_bits_long(&gb, sl->au_seq_num_len);
      if (sl->use_timestamps) {
        dts_flag = get_bits1(&gb);
        cts_flag = get_bits1(&gb);
      }
    }
    if (sl->inst_bitrate_len)
      inst_bitrate_flag = get_bits1(&gb);
    if (dts_flag == 1)
      dts = get_bits64(&gb, sl->timestamp_len);
    if (cts_flag == 1)
      cts = get_bits64(&gb, sl->timestamp_len);
    if (sl->au_len > 0)
      skip_bits_long(&gb, sl->au_len);
    if (inst_bitrate_flag)
      skip_bits_long(&gb, sl->inst_bitrate_len);
  }

  if (dts != AV_NOPTS_VALUE)
    pes->dts = dts;
  if (cts != AV_NOPTS_VALUE)
    pes->pts = cts;

  av_set_pts_info(pes->st, sl->timestamp_len, 1, sl->timestamp_res);

  return (get_bits_count(&gb) + 7) >> 3;
}

/* return non zero if a packet could be constructed */
static int mpegts_push_data(lives_clip_data_t *cdata, MpegTSFilter *filter,
                            const uint8_t *buf, int buf_size, int is_start,
                            int64_t pos) {
  PESContext *pes = filter->u.pes_filter.opaque;
  MpegTSContext *ts = pes->ts;
  const uint8_t *p;
  int len, code;

  if (!ts->pkt)
    return 0;

  if (is_start) {
    if (pes->state == MPEGTS_PAYLOAD && pes->data_index > 0) {
      new_pes_packet(pes, ts->pkt);
      ts->stop_parse = 1;
    }
    pes->state = MPEGTS_HEADER;
    pes->data_index = 0;
    pes->ts_packet_pos = pos;
  }
  p = buf;
  while (buf_size > 0) {
    switch (pes->state) {
    case MPEGTS_HEADER:
      len = PES_START_SIZE - pes->data_index;
      if (len > buf_size)
        len = buf_size;
      memcpy(pes->header + pes->data_index, p, len);
      pes->data_index += len;
      p += len;
      buf_size -= len;
      if (pes->data_index == PES_START_SIZE) {
        /* we got all the PES or section header. We can now
           decide */
        if (pes->header[0] == 0x00 && pes->header[1] == 0x00 &&
            pes->header[2] == 0x01) {
          /* it must be an mpeg2 PES stream */
          code = pes->header[3] | 0x100;
          //av_dlog(pes->stream, "pid=%x pes_code=%#x\n", pes->pid, code);

          if ((pes->st && pes->st->discard == AVDISCARD_ALL) ||
              code == 0x1be) /* padding_stream */
            goto skip;

          /* stream not present in PMT */
          if (!pes->st) {
            pes->st = av_new_stream(ts->stream, pes->pid);
            if (!pes->st)
              return AVERROR(ENOMEM);
            pes->st->id = pes->pid;
            mpegts_set_stream_info(cdata, pes->st, pes, 0, 0);
          }

          pes->total_size = lives_rb16(pes->header + 4);
          /* NOTE: a zero total size means the PES size is
             unbounded */
          if (!pes->total_size)
            pes->total_size = MAX_PES_PAYLOAD;

          /* allocate pes buffer */
          pes->buffer = av_malloc(pes->total_size+FF_INPUT_BUFFER_PADDING_SIZE);
          if (!pes->buffer)
            return AVERROR(ENOMEM);

          if (code != 0x1bc && code != 0x1bf && /* program_stream_map, private_stream_2 */
              code != 0x1f0 && code != 0x1f1 && /* ECM, EMM */
              code != 0x1ff && code != 0x1f2 && /* program_stream_directory, DSMCC_stream */
              code != 0x1f8) {                  /* ITU-T Rec. H.222.1 type E stream */
            pes->state = MPEGTS_PESHEADER;
          } else {
            pes->state = MPEGTS_PAYLOAD;
            pes->data_index = 0;
          }
        } else {
          /* otherwise, it should be a table */
          /* skip packet */
skip:
          pes->state = MPEGTS_SKIP;
          continue;
        }
      }
      break;
      /**********************************************/
      /* PES packing parsing */
    case MPEGTS_PESHEADER:
      len = PES_HEADER_SIZE - pes->data_index;
      if (len < 0)
        return -1;
      if (len > buf_size)
        len = buf_size;
      memcpy(pes->header + pes->data_index, p, len);
      pes->data_index += len;
      p += len;
      buf_size -= len;
      if (pes->data_index == PES_HEADER_SIZE) {
        pes->pes_header_size = pes->header[8] + 9;
        pes->state = MPEGTS_PESHEADER_FILL;
      }
      break;
    case MPEGTS_PESHEADER_FILL:
      len = pes->pes_header_size - pes->data_index;
      if (len < 0)
        return -1;
      if (len > buf_size)
        len = buf_size;
      memcpy(pes->header + pes->data_index, p, len);
      pes->data_index += len;
      p += len;
      buf_size -= len;
      if (pes->data_index == pes->pes_header_size) {
        const uint8_t *r;
        unsigned int flags, pes_ext, skip;

        flags = pes->header[7];
        r = pes->header + 9;
        pes->pts = AV_NOPTS_VALUE;
        pes->dts = AV_NOPTS_VALUE;
        if ((flags & 0xc0) == 0x80) {
          pes->dts = pes->pts = ff_parse_pes_pts(r);
          r += 5;
        } else if ((flags & 0xc0) == 0xc0) {
          pes->pts = ff_parse_pes_pts(r);
          r += 5;
          pes->dts = ff_parse_pes_pts(r);
          r += 5;
        }
        pes->extended_stream_id = -1;
        if (flags & 0x01) { /* PES extension */
          pes_ext = *r++;
          /* Skip PES private data, program packet sequence counter and P-STD buffer */
          skip = (pes_ext >> 4) & 0xb;
          skip += skip & 0x9;
          r += skip;
          if ((pes_ext & 0x41) == 0x01 &&
              (r + 2) <= (pes->header + pes->pes_header_size)) {
            /* PES extension 2 */
            if ((r[0] & 0x7f) > 0 && (r[1] & 0x80) == 0)
              pes->extended_stream_id = r[1];
          }
        }

        /* we got the full header. We parse it and get the payload */
        pes->state = MPEGTS_PAYLOAD;
        pes->data_index = 0;
        if (pes->stream_type == 0x12) {
          int sl_header_bytes = read_sl_header(pes, &pes->sl, p, buf_size);
          pes->pes_header_size += sl_header_bytes;
          p += sl_header_bytes;
          buf_size -= sl_header_bytes;
        }
      }
      break;
    case MPEGTS_PAYLOAD:
      if (buf_size > 0 && pes->buffer) {
        if (pes->data_index > 0 && pes->data_index+buf_size > pes->total_size) {
          new_pes_packet(pes, ts->pkt);
          pes->total_size = MAX_PES_PAYLOAD;
          pes->buffer = av_malloc(pes->total_size+FF_INPUT_BUFFER_PADDING_SIZE);
          if (!pes->buffer)
            return AVERROR(ENOMEM);
          ts->stop_parse = 1;
        } else if (pes->data_index == 0 && buf_size > pes->total_size) {
          // pes packet size is < ts size packet and pes data is padded with 0xff
          // not sure if this is legal in ts but see issue #2392
          buf_size = pes->total_size;
        }
        memcpy(pes->buffer+pes->data_index, p, buf_size);
        pes->data_index += buf_size;
      }
      buf_size = 0;
      /* emit complete packets with known packet size
       * decreases demuxer delay for infrequent packets like subtitles from
       * a couple of seconds to milliseconds for properly muxed files.
       * total_size is the number of bytes following pes_packet_length
       * in the pes header, i.e. not counting the first 6 bytes */
      if (!ts->stop_parse && pes->total_size < MAX_PES_PAYLOAD &&
          pes->pes_header_size + pes->data_index == pes->total_size + 6) {
        ts->stop_parse = 1;
        new_pes_packet(pes, ts->pkt);
      }
      break;
    case MPEGTS_SKIP:
      buf_size = 0;
      break;
    }
  }

  return 0;
}

static PESContext *add_pes_stream(MpegTSContext *ts, int pid, int pcr_pid) {
  MpegTSFilter *tss;
  PESContext *pes;

  /* if no pid found, then add a pid context */
  pes = av_mallocz(sizeof(PESContext));
  if (!pes)
    return 0;
  pes->ts = ts;
  pes->stream = ts->stream;
  pes->pid = pid;
  pes->pcr_pid = pcr_pid;
  pes->state = MPEGTS_SKIP;
  pes->pts = AV_NOPTS_VALUE;
  pes->dts = AV_NOPTS_VALUE;
  tss = mpegts_open_pes_filter(ts, pid, mpegts_push_data, pes);
  if (!tss) {
    av_free(pes);
    return 0;
  }
  return pes;
}

#define MAX_LEVEL 4
typedef struct {
  AVFormatContext *s;
  int fd;
  Mp4Descr *descr;
  Mp4Descr *active_descr;
  int descr_count;
  int max_descr_count;
  int level;
} MP4DescrParseContext;


static int init_MP4DescrParseContext(lives_clip_data_t *cdata,
                                     MP4DescrParseContext *d, AVFormatContext *s, const uint8_t *buf,
                                     unsigned size, Mp4Descr *descr, int max_descr_count) {
  //    int ret;
  if (size > (1<<30))
    return -2;

  if ((d->fd=open(cdata->URI,O_RDONLY))==-1) {
    return d->fd;
  }
  d->s = s;
  d->level = 0;
  d->descr_count = 0;
  d->descr = descr;
  d->active_descr = NULL;
  d->max_descr_count = max_descr_count;

  return 0;
}


int ff_mp4_read_descr_len(uint8_t *p) {
  int len = 0;
  int count = 4;
  while (count--) {
    int c = lives_r8(p);
    p++;
    len = (len << 7) | (c & 0x7f);
    if (!(c & 0x80))
      break;
  }
  return len;
}


int ff_mp4_read_descr(lives_clip_data_t *cdata, AVFormatContext *fc, uint8_t *p, int *tag) {
  int len;
  *tag = lives_r8(p);
  p++;
  len = ff_mp4_read_descr_len(p);
  //av_dlog(fc, "MPEG4 description: tag=0x%02x len=%d\n", *tag, len);
  return len;
}




int ff_mp4_read_descr_lenf(lives_clip_data_t *cdata, int fd) {
  int len = 0;
  int count = 4;
  while (count--) {
    int c = lives_rf8(cdata,fd);
    len = (len << 7) | (c & 0x7f);
    if (!(c & 0x80))
      break;
  }
  return len;
}


int ff_mp4_read_descrf(lives_clip_data_t *cdata, AVFormatContext *fc, int fd, int *tag) {
  int len;
  *tag = lives_rf8(cdata,fd);
  len = ff_mp4_read_descr_lenf(cdata,fd);
  //av_dlog(fc, "MPEG4 description: tag=0x%02x len=%d\n", *tag, len);
  return len;
}







int ff_mp4_read_dec_config_descr(lives_clip_data_t *cdata, AVFormatContext *fc, AVStream *st, uint8_t *p) {
  int len, tag;
  int object_type_id = lives_r8(p);
  p++;
  lives_r8(p); /* stream type */
  p++;
  lives_rb24(p); /* buffer size db */
  p+=3;
  lives_rb32(p); /* max bitrate */
  p+=4;
  lives_rb32(p); /* avg bitrate */
  p+=4;

  st->codec->codec_id= ff_codec_get_id(ff_mp4_obj_type, object_type_id);
  //fprintf(stderr, "esds object type id 0x%02x\n", object_type_id);

  len = ff_mp4_read_descr(cdata, fc, p, &tag);

  if (tag == MP4DecSpecificDescrTag) {
    //av_dlog(fc, "Specific MPEG4 header len=%d\n", len);
    if (!len || (uint64_t)len > (1<<30))
      return -1;
    av_free(st->codec->extradata);
    st->codec->extradata = av_mallocz(len + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!st->codec->extradata)
      return AVERROR(ENOMEM);
    memcpy(st->codec->extradata, p, len);
    st->codec->extradata_size = len;

    // audio

  }
  return 0;
}


static void update_offsets(int fd, int64_t *off, int *len) {
  int64_t new_off = lseek(fd,0,SEEK_CUR);
  (*len) -= new_off - *off;
  *off = new_off;
}

static int parse_mp4_descr(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len, int target_tag);


static int parse_mp4_descr_arr(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len) {
  while (len > 0) {
    if (parse_mp4_descr(cdata, d, off, len, 0) < 0)
      return -1;
    update_offsets(d->fd, &off, &len);
  }
  return 0;
}

static int parse_MP4IODescrTag(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len) {
  lives_rbf16(cdata,d->fd); // ID
  lives_rf8(cdata,d->fd);
  lives_rf8(cdata,d->fd);
  lives_rf8(cdata,d->fd);
  lives_rf8(cdata,d->fd);
  lives_rf8(cdata,d->fd);
  update_offsets(d->fd, &off, &len);
  return parse_mp4_descr_arr(cdata, d, off, len);
}


static int parse_MP4ODescrTag(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len) {
  int id_flags;
  if (len < 2)
    return 0;
  id_flags = lives_rbf16(cdata,d->fd);
  if (!(id_flags & 0x0020)) { //URL_Flag
    update_offsets(d->fd, &off, &len);
    return parse_mp4_descr_arr(cdata, d, off, len); //ES_Descriptor[]
  } else {
    return 0;
  }
}

static int parse_MP4ESDescrTag(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len) {
  int es_id = 0;
  if (d->descr_count >= d->max_descr_count)
    return -1;
  ff_mp4_parse_es_descr(cdata, d->fd, &es_id);
  d->active_descr = d->descr + (d->descr_count++);

  d->active_descr->es_id = es_id;
  update_offsets(d->fd, &off, &len);
  parse_mp4_descr(cdata, d, off, len, MP4DecConfigDescrTag);
  update_offsets(d->fd, &off, &len);
  if (len > 0)
    parse_mp4_descr(cdata, d, off, len, MP4SLDescrTag);
  d->active_descr = NULL;
  return 0;
}

static int parse_MP4DecConfigDescrTag(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len) {
  Mp4Descr *descr = d->active_descr;
  if (!descr)
    return -1;
  d->active_descr->dec_config_descr = av_malloc(len);
  if (!descr->dec_config_descr)
    return AVERROR(ENOMEM);
  descr->dec_config_descr_len = len;

  lives_read(cdata, d->fd, descr->dec_config_descr, len);

  return 0;
}

static int parse_MP4SLDescrTag(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len) {
  Mp4Descr *descr = d->active_descr;
  int predefined;
  if (!descr)
    return -1;

  predefined = lives_rf8(cdata,d->fd);
  if (!predefined) {
    int lengths;
    int flags = lives_rf8(cdata,d->fd);
    descr->sl.use_au_start       = !!(flags & 0x80);
    descr->sl.use_au_end         = !!(flags & 0x40);
    descr->sl.use_rand_acc_pt    = !!(flags & 0x20);
    descr->sl.use_padding        = !!(flags & 0x08);
    descr->sl.use_timestamps     = !!(flags & 0x04);
    descr->sl.use_idle           = !!(flags & 0x02);
    descr->sl.timestamp_res      = lives_rbf32(cdata,d->fd);
    lives_rbf32(cdata,d->fd);
    descr->sl.timestamp_len      = lives_rf8(cdata,d->fd);
    descr->sl.ocr_len            = lives_rf8(cdata,d->fd);
    descr->sl.au_len             = lives_rf8(cdata,d->fd);
    descr->sl.inst_bitrate_len   = lives_rf8(cdata,d->fd);
    lengths                      = lives_rbf16(cdata,d->fd);
    descr->sl.degr_prior_len     = lengths >> 12;
    descr->sl.au_seq_num_len     = (lengths >> 7) & 0x1f;
    descr->sl.packet_seq_num_len = (lengths >> 2) & 0x1f;
  } //else {

  //av_log_missing_feature(d->s, "Predefined SLConfigDescriptor\n", 0);
  //}
  return 0;
}





static int parse_mp4_descr(lives_clip_data_t *cdata, MP4DescrParseContext *d, int64_t off, int len,
                           int target_tag) {
  int tag;

  int len1 = ff_mp4_read_descrf(cdata, d->s, d->fd, &tag);

  update_offsets(d->fd, &off, &len);
  if (len < 0 || len1 > len || len1 <= 0) {
    fprintf(stderr, "mpegts_decoder: Tag %x length violation new length %d bytes remaining %d\n", tag, len1, len);
    return -1;
  }

  if (d->level++ >= MAX_LEVEL) {
    fprintf(stderr, "mpegts_decoder: Maximum MP4 descriptor level exceeded\n");
    goto done;
  }

  if (target_tag && tag != target_tag) {
    fprintf(stderr, "mpegts_decoder: Found tag %x expected %x\n", tag, target_tag);
    goto done;
  }

  switch (tag) {
  case MP4IODescrTag:
    parse_MP4IODescrTag(cdata, d, off, len1);
    break;
  case MP4ODescrTag:
    parse_MP4ODescrTag(cdata, d, off, len1);
    break;
  case MP4ESDescrTag:
    parse_MP4ESDescrTag(cdata, d, off, len1);
    break;
  case MP4DecConfigDescrTag:
    parse_MP4DecConfigDescrTag(cdata, d, off, len1);
    break;
  case MP4SLDescrTag:
    parse_MP4SLDescrTag(cdata, d, off, len1);
    break;
  }

done:
  d->level--;
  lives_seek(cdata, d->fd, off + len1);
  return 0;
}



static int mp4_read_iods(lives_clip_data_t *cdata, AVFormatContext *s, const uint8_t *buf, unsigned size,
                         Mp4Descr *descr, int *descr_count, int max_descr_count) {
  MP4DescrParseContext d;
  if (init_MP4DescrParseContext(cdata, &d, s, buf, size, descr, max_descr_count) < 0)
    return -1;

  parse_mp4_descr(cdata, &d, lseek(d.fd,0,SEEK_CUR), size, MP4IODescrTag);

  *descr_count = d.descr_count;

  close(d.fd);

  return 0;
}

static int mp4_read_od(lives_clip_data_t *cdata, AVFormatContext *s, const uint8_t *buf, unsigned size,
                       Mp4Descr *descr, int *descr_count, int max_descr_count) {
  MP4DescrParseContext d;
  if (init_MP4DescrParseContext(cdata, &d, s, buf, size, descr, max_descr_count) < 0)
    return -1;

  parse_mp4_descr_arr(cdata, &d, lseek(d.fd,0,SEEK_CUR), size);

  *descr_count = d.descr_count;

  close(d.fd);

  return 0;
}

static void m4sl_cb(lives_clip_data_t *cdata, MpegTSFilter *filter, const uint8_t *section, int section_len) {
  MpegTSContext *ts = filter->u.section_filter.opaque;
  SectionHeader h;
  const uint8_t *p, *p_end;
  Mp4Descr mp4_descr[MAX_MP4_DESCR_COUNT] = {{ 0 }};
  int mp4_descr_count = 0;
  int i, pid;
  AVFormatContext *s = ts->stream;

  p_end = section + section_len - 4;
  p = section;
  if (parse_section_header(&h, &p, p_end) < 0)
    return;
  if (h.tid != M4OD_TID)
    return;

  mp4_read_od(cdata, s, p, (unsigned)(p_end - p), mp4_descr, &mp4_descr_count, MAX_MP4_DESCR_COUNT);

  for (pid = 0; pid < NB_PID_MAX; pid++) {
    if (!ts->pids[pid])
      continue;
    for (i = 0; i < mp4_descr_count; i++) {
      PESContext *pes;
      AVStream *st;
      if (ts->pids[pid]->es_id != mp4_descr[i].es_id)
        continue;
      if (!(ts->pids[pid] && ts->pids[pid]->type == MPEGTS_PES)) {
        //av_log(s, AV_LOG_ERROR, "pid %x is not PES\n", pid);
        continue;
      }
      pes = ts->pids[pid]->u.pes_filter.opaque;
      st = pes->st;
      if (!st) {
        continue;
      }

      pes->sl = mp4_descr[i].sl;

      ff_mp4_read_dec_config_descr(cdata, s, st, mp4_descr[i].dec_config_descr);

      if (st->codec->codec_id == CODEC_ID_AAC &&
          st->codec->extradata_size > 0)
        st->need_parsing = 0;
      if (st->codec->codec_id == CODEC_ID_H264 &&
          st->codec->extradata_size > 0)
        st->need_parsing = 0;

      if (st->codec->codec_id <= CODEC_ID_NONE) {
      } else if (st->codec->codec_id < CODEC_ID_FIRST_AUDIO) {
        st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
      } else if (st->codec->codec_id < CODEC_ID_FIRST_SUBTITLE) {
        st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
      } else if (st->codec->codec_id < CODEC_ID_FIRST_UNKNOWN) {
        st->codec->codec_type = AVMEDIA_TYPE_SUBTITLE;
      }
    }
  }
  for (i = 0; i < mp4_descr_count; i++)
    av_free(mp4_descr[i].dec_config_descr);
}




int ff_parse_mpeg2_descriptor(lives_clip_data_t *cdata, AVFormatContext *fc, AVStream *st, int stream_type,
                              const uint8_t **pp, const uint8_t *desc_list_end,
                              Mp4Descr *mp4_descr, int mp4_descr_count, int pid,
                              MpegTSContext *ts) {
  const uint8_t *desc_end;
  int desc_len, desc_tag, desc_es_id;
  char language[252];
  int i;

  desc_tag = get8(pp, desc_list_end);
  if (desc_tag < 0)
    return -1;
  desc_len = get8(pp, desc_list_end);
  if (desc_len < 0)
    return -1;
  desc_end = *pp + desc_len;
  if (desc_end > desc_list_end)
    return -1;

  //fprintf(stderr, "tag: 0x%02x len=%d\n", desc_tag, desc_len);

  if (st->codec->codec_id == CODEC_ID_NONE &&
      stream_type == STREAM_TYPE_PRIVATE_DATA)
    mpegts_find_stream_type(st, desc_tag, DESC_types);

  switch (desc_tag) {
  case 0x1E: /* SL descriptor */
    desc_es_id = get16(pp, desc_end);
    if (ts && ts->pids[pid])
      ts->pids[pid]->es_id = desc_es_id;
    for (i = 0; i < mp4_descr_count; i++)
      if (mp4_descr[i].dec_config_descr_len &&
          mp4_descr[i].es_id == desc_es_id) {

        ff_mp4_read_dec_config_descr(cdata, fc, st, mp4_descr[i].dec_config_descr);

        if (st->codec->codec_id == CODEC_ID_AAC &&
            st->codec->extradata_size > 0)
          st->need_parsing = 0;
        if (st->codec->codec_id == CODEC_ID_MPEG4SYSTEMS)
          mpegts_open_section_filter(ts, pid, m4sl_cb, ts, 1);
      }
    break;
  case 0x1F: /* FMC descriptor */
    get16(pp, desc_end);
    if (mp4_descr_count > 0 && (st->codec->codec_id == CODEC_ID_AAC_LATM) &&
        mp4_descr->dec_config_descr_len && mp4_descr->es_id == pid) {
      ff_mp4_read_dec_config_descr(cdata, fc, st, mp4_descr->dec_config_descr);
      if (st->codec->codec_id == CODEC_ID_AAC &&
          st->codec->extradata_size > 0) {
        st->need_parsing = 0;
        st->codec->codec_type= AVMEDIA_TYPE_AUDIO;
      }
    }
    break;
  case 0x56: /* DVB teletext descriptor */
    language[0] = get8(pp, desc_end);
    language[1] = get8(pp, desc_end);
    language[2] = get8(pp, desc_end);
    language[3] = 0;
    av_dict_set(&st->metadata, "language", language, 0);
    break;
  case 0x59: /* subtitling descriptor */
    language[0] = get8(pp, desc_end);
    language[1] = get8(pp, desc_end);
    language[2] = get8(pp, desc_end);
    language[3] = 0;
    /* hearing impaired subtitles detection */
    switch (get8(pp, desc_end)) {
    case 0x20: /* DVB subtitles (for the hard of hearing) with no monitor aspect ratio criticality */
    case 0x21: /* DVB subtitles (for the hard of hearing) for display on 4:3 aspect ratio monitor */
    case 0x22: /* DVB subtitles (for the hard of hearing) for display on 16:9 aspect ratio monitor */
    case 0x23: /* DVB subtitles (for the hard of hearing) for display on 2.21:1 aspect ratio monitor */
    case 0x24: /* DVB subtitles (for the hard of hearing) for display on a high definition monitor */
    case 0x25: /* DVB subtitles (for the hard of hearing) with plano-stereoscopic disparity for display on a high definition monitor */
      st->disposition |= AV_DISPOSITION_HEARING_IMPAIRED;
      break;
    }
    if (st->codec->extradata) {
      //if (st->codec->extradata_size == 4 && memcmp(st->codec->extradata, *pp, 4))
      //av_log_ask_for_sample(fc, "DVB sub with multiple IDs\n");
      //} else {
      st->codec->extradata = av_malloc(4 + FF_INPUT_BUFFER_PADDING_SIZE);
      if (st->codec->extradata) {
        st->codec->extradata_size = 4;
        memcpy(st->codec->extradata, *pp, 4);
      }
    }
    *pp += 4;
    av_dict_set(&st->metadata, "language", language, 0);
    break;
  case 0x0a: /* ISO 639 language descriptor */
    for (i = 0; i + 4 <= desc_len; i += 4) {
      language[i + 0] = get8(pp, desc_end);
      language[i + 1] = get8(pp, desc_end);
      language[i + 2] = get8(pp, desc_end);
      language[i + 3] = ',';
      switch (get8(pp, desc_end)) {
      case 0x01:
        st->disposition |= AV_DISPOSITION_CLEAN_EFFECTS;
        break;
      case 0x02:
        st->disposition |= AV_DISPOSITION_HEARING_IMPAIRED;
        break;
      case 0x03:
        st->disposition |= AV_DISPOSITION_VISUAL_IMPAIRED;
        break;
      }
    }
    if (i) {
      language[i - 1] = 0;
      av_dict_set(&st->metadata, "language", language, 0);
    }
    break;
  case 0x05: /* registration descriptor */
    st->codec->codec_tag = lives_rl32((const char *)*pp);
    *pp+=4;
    //av_dlog(fc, "reg_desc=%.4s\n", (char*)&st->codec->codec_tag);
    if (st->codec->codec_id == CODEC_ID_NONE &&
        stream_type == STREAM_TYPE_PRIVATE_DATA)
      mpegts_find_stream_type(st, st->codec->codec_tag, REGD_types);
    break;
  case 0x52: /* stream identifier descriptor */
    //    st->stream_identifier = 1 + get8(pp, desc_end);
    break;
  default:
    break;
  }
  *pp = desc_end;
  return 0;
}


void ff_program_add_stream_index(AVFormatContext *ac, int progid, unsigned int idx) {
  int i, j;
  AVProgram *program=NULL;
  void *tmp;

  if (idx >= ac->nb_streams) {
    fprintf(stderr, "mpegts_decoder: stream index %d is not valid\n", idx);
    return;
  }

  for (i=0; i<ac->nb_programs; i++) {
    if (ac->programs[i]->id != progid)
      continue;
    program = ac->programs[i];
    for (j=0; j<program->nb_stream_indexes; j++)
      if (program->stream_index[j] == idx)
        return;

    tmp = av_realloc(program->stream_index, sizeof(unsigned int)*(program->nb_stream_indexes+1));
    if (!tmp)
      return;
    program->stream_index = tmp;
    program->stream_index[program->nb_stream_indexes++] = idx;
    return;
  }
}











static void pmt_cb(lives_clip_data_t *cdata, MpegTSFilter *filter, const uint8_t *section, int section_len) {
  MpegTSContext *ts = filter->u.section_filter.opaque;
  SectionHeader h1, *h = &h1;
  PESContext *pes;
  AVStream *st;
  const uint8_t *p, *p_end, *desc_list_end;
  int program_info_length, pcr_pid, pid, stream_type;
  int desc_list_len;
  uint32_t prog_reg_desc = 0; /* registration descriptor */

  Mp4Descr mp4_descr[MAX_MP4_DESCR_COUNT] = {{ 0 }};
  int mp4_descr_count = 0;
  int i;

  //fprintf(stderr, "PMT: len %i\n", section_len);
  //hex_dump_debug(ts->stream, (uint8_t *)section, section_len);

  p_end = section + section_len - 4;
  p = section;
  if (parse_section_header(h, &p, p_end) < 0)
    return;

  // av_dlog(ts->stream, "sid=0x%x sec_num=%d/%d\n",
  //	  h->id, h->sec_num, h->last_sec_num);

  if (h->tid != PMT_TID)
    return;

  clear_program(ts, h->id);
  pcr_pid = get16(&p, p_end) & 0x1fff;
  if (pcr_pid < 0)
    return;
  add_pid_to_pmt(ts, h->id, pcr_pid);
  set_pcr_pid(ts->stream, h->id, pcr_pid);

  // av_dlog(ts->stream, "pcr_pid=0x%x\n", pcr_pid);

  program_info_length = get16(&p, p_end) & 0xfff;
  if (program_info_length < 0)
    return;
  while (program_info_length >= 2) {
    uint8_t tag, len;
    tag = get8(&p, p_end);
    len = get8(&p, p_end);

    //av_dlog(ts->stream, "program tag: 0x%02x len=%d\n", tag, len);

    if (len > program_info_length - 2)
      //something else is broken, exit the program_descriptors_loop
      break;
    program_info_length -= len + 2;
    if (tag == 0x1d) { // IOD descriptor
      get8(&p, p_end); // scope
      get8(&p, p_end); // label
      len -= 2;
      mp4_read_iods(cdata, ts->stream, p, len, mp4_descr + mp4_descr_count,
                    &mp4_descr_count, MAX_MP4_DESCR_COUNT);
    } else if (tag == 0x05 && len >= 4) { // registration descriptor
      prog_reg_desc = lives_rl32((const char *)p);
      p+=4;
      len -= 4;
    }
    p += len;
  }
  p += program_info_length;
  if (p >= p_end)
    goto out;

  // stop parsing after pmt, we found header
  if (!ts->stream->nb_streams)
    ts->stop_parse = 2;

  for (;;) {
    st = 0;
    pes = NULL;
    stream_type = get8(&p, p_end);
    if (stream_type < 0)
      break;
    pid = get16(&p, p_end) & 0x1fff;
    if (pid < 0)
      break;

    /* now create stream */
    if (ts->pids[pid] && ts->pids[pid]->type == MPEGTS_PES) {
      pes = ts->pids[pid]->u.pes_filter.opaque;
      if (!pes->st) {
        pes->st = av_new_stream(pes->stream, pes->pid);
        pes->st->id = pes->pid;
      }
      st = pes->st;
    } else if (stream_type != 0x13) {
      if (ts->pids[pid]) mpegts_close_filter(ts, ts->pids[pid]); //wrongly added sdt filter probably
      pes = add_pes_stream(ts, pid, pcr_pid);
      if (pes) {
        st = av_new_stream(pes->stream, pes->pid);
        st->id = pes->pid;
      }
    } else {
      int idx = ff_find_stream_index(ts->stream, pid);
      if (idx >= 0) {
        st = ts->stream->streams[idx];
      } else {
        st = av_new_stream(pes->stream, pid);
        st->id = pid;
        st->codec->codec_type = AVMEDIA_TYPE_DATA;
      }
    }

    if (!st)
      goto out;

    if (pes && !pes->stream_type)
      mpegts_set_stream_info(cdata, st, pes, stream_type, prog_reg_desc);

    add_pid_to_pmt(ts, h->id, pid);

    ff_program_add_stream_index(ts->stream, h->id, st->index);

    desc_list_len = get16(&p, p_end) & 0xfff;
    if (desc_list_len < 0)
      break;
    desc_list_end = p + desc_list_len;
    if (desc_list_end > p_end)
      break;
    for (;;) {
      if (ff_parse_mpeg2_descriptor(cdata, ts->stream, st, stream_type, &p, desc_list_end,
                                    mp4_descr, mp4_descr_count, pid, ts) < 0)
        break;

      if (pes && prog_reg_desc == lives_rl32("HDMV") && stream_type == 0x83 && pes->sub_st) {
        ff_program_add_stream_index(ts->stream, h->id, pes->sub_st->index);
        pes->sub_st->codec->codec_tag = st->codec->codec_tag;
      }
    }
    p = desc_list_end;
  }

out:
  for (i = 0; i < mp4_descr_count; i++)
    av_free(mp4_descr[i].dec_config_descr);
}

static void pat_cb(lives_clip_data_t *cdata, MpegTSFilter *filter, const uint8_t *section, int section_len) {
  MpegTSContext *ts = filter->u.section_filter.opaque;
  SectionHeader h1, *h = &h1;
  const uint8_t *p, *p_end;
  int sid, pmt_pid;
  //  AVProgram *program;

  //fprintf(stderr, "PAT:\n");
  //hex_dump_debug(ts->stream, (uint8_t *)section, section_len);

  p_end = section + section_len - 4;
  p = section;
  if (parse_section_header(h, &p, p_end) < 0)
    return;
  if (h->tid != PAT_TID)
    return;

  //  ts->stream->ts_id = h->id;

  clear_programs(ts);
  for (;;) {
    sid = get16(&p, p_end);
    if (sid < 0)
      break;
    pmt_pid = get16(&p, p_end) & 0x1fff;
    if (pmt_pid < 0)
      break;

    //av_dlog(ts->stream, "sid=0x%x pid=0x%x\n", sid, pmt_pid);

    if (sid == 0x0000) {
      /* NIT info */
    } else {
      //      program = av_new_program(ts->stream, sid);
      //      program->program_num = sid;
      //program->pmt_pid = pmt_pid;
      if (ts->pids[pmt_pid])
        mpegts_close_filter(ts, ts->pids[pmt_pid]);
      mpegts_open_section_filter(ts, pmt_pid, pmt_cb, ts, 1);
      add_pat_entry(ts, sid);
      add_pid_to_pmt(ts, sid, 0); //add pat pid to program
      add_pid_to_pmt(ts, sid, pmt_pid);
    }
  }
}

static void sdt_cb(lives_clip_data_t *cdata, MpegTSFilter *filter, const uint8_t *section, int section_len) {
  MpegTSContext *ts = filter->u.section_filter.opaque;
  SectionHeader h1, *h = &h1;
  const uint8_t *p, *p_end, *desc_list_end, *desc_end;
  int onid, val, sid, desc_list_len, desc_tag, desc_len, service_type;
  char *name, *provider_name;

  //av_dlog(ts->stream, "SDT:\n");
  //hex_dump_debug(ts->stream, (uint8_t *)section, section_len);

  p_end = section + section_len - 4;
  p = section;
  if (parse_section_header(h, &p, p_end) < 0)
    return;
  if (h->tid != SDT_TID)
    return;
  onid = get16(&p, p_end);
  if (onid < 0)
    return;
  val = get8(&p, p_end);
  if (val < 0)
    return;
  for (;;) {
    sid = get16(&p, p_end);
    if (sid < 0)
      break;
    val = get8(&p, p_end);
    if (val < 0)
      break;
    desc_list_len = get16(&p, p_end) & 0xfff;
    if (desc_list_len < 0)
      break;
    desc_list_end = p + desc_list_len;
    if (desc_list_end > p_end)
      break;
    for (;;) {
      desc_tag = get8(&p, desc_list_end);
      if (desc_tag < 0)
        break;
      desc_len = get8(&p, desc_list_end);
      desc_end = p + desc_len;
      if (desc_end > desc_list_end)
        break;

      //av_dlog(ts->stream, "tag: 0x%02x len=%d\n",
      //      desc_tag, desc_len);

      switch (desc_tag) {
      case 0x48:
        service_type = get8(&p, p_end);
        if (service_type < 0)
          break;
        provider_name = getstr8(&p, p_end);
        if (!provider_name)
          break;
        name = getstr8(&p, p_end);
        if (name) {
          AVProgram *program = av_new_program(ts->stream, sid);
          if (program) {
            av_dict_set(&program->metadata, "service_name", name, 0);
            av_dict_set(&program->metadata, "service_provider", provider_name, 0);
          }
        }
        av_free(name);
        av_free(provider_name);
        break;
      default:
        break;
      }
      p = desc_end;
    }
    p = desc_list_end;
  }
}



////////////////////////////////////////////////////////////////////////////////////////////




/* handle one TS packet */
static int handle_packet(lives_clip_data_t *cdata, const uint8_t *packet) {
  lives_mpegts_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;
  MpegTSContext *ts = s->priv_data;

  MpegTSFilter *tss;
  int len, pid, cc, expected_cc, cc_ok, afc, is_start, is_discontinuity,
      has_adaptation, has_payload;
  const uint8_t *p, *p_end;
  int64_t pos;

  PESContext *pes;
  AVStream *st;

  pid = lives_rb16(packet + 1) & 0x1fff;
  if (pid && discard_pid(ts, pid))
    return 0;
  is_start = packet[1] & 0x40;
  tss = ts->pids[pid];

  // TODO - if tss is NULL,  most likely these are invalid after EOF frames - drop them

  if (ts->auto_guess && tss == NULL && is_start) {
    add_pes_stream(ts, pid, -1);
    tss = ts->pids[pid];
  }
  if (!tss) {
    return 0;
  }

  pes = tss->u.pes_filter.opaque;
  if (pes!=NULL) {
    st = pes->st;
    if (priv->vidst!=NULL&&st!=priv->vidst) return 0;
  }

  afc = (packet[3] >> 4) & 3;
  if (afc == 0) /* reserved value */
    return 0;

  has_adaptation = afc & 2;
  has_payload = afc & 1;
  is_discontinuity = has_adaptation
                     && packet[4] != 0 /* with length > 0 */
                     && (packet[5] & 0x80); /* and discontinuity indicated */

  /* continuity check (currently not used) */
  cc = (packet[3] & 0xf);
  expected_cc = has_payload ? (tss->last_cc + 1) & 0x0f : tss->last_cc;
  cc_ok = pid == 0x1FFF // null packet PID
          || is_discontinuity
          || tss->last_cc < 0
          || expected_cc == cc;

  tss->last_cc = cc;
  if (!cc_ok) {
    fprintf(stderr,
            "mpegts_decoder: Continuity check failed for pid %d expected %d got %d\n",
            pid, expected_cc, cc);
    if (tss->type == MPEGTS_PES) {
      //PESContext *pc = tss->u.pes_filter.opaque;
      //pc->flags |= AV_PKT_FLAG_CORRUPT;
    }
  }

  if (!has_payload)
    return 0;

  // skip stream packet
  p = packet + 4;
  if (has_adaptation) {
    uint8_t xflags,spc;
    if ((xflags=p[1])&4) {
      // get splice distance
      size_t sploffs=1; // skip pcr
      if (xflags & 8) sploffs+=48; // skip opcr

      spc=p[sploffs];
      if (spc==0) {
        //fprintf (stderr,"XXX:  Keyframe ?");
      }

    }
    /* skip adaptation field */
    p += p[0] + 1;
  }
  /* if past the end of packet, ignore */
  p_end = packet + TS_PACKET_SIZE;
  if (p >= p_end)
    return 0;

  pos = priv->input_position;
  ts->pos47= pos % ts->raw_packet_size;


  if (tss->type == MPEGTS_SECTION) {
    if (is_start) {
      /* pointer field present */
      len = *p++;
      if (p + len > p_end)
        return 0;
      if (len && cc_ok) {
        /* write remaining section bytes */
        write_section_data(cdata, s, tss,
                           p, len, 0);
        /* check whether filter has been closed */
        if (!ts->pids[pid])
          return 0;
      }
      p += len;
      if (p < p_end) {
        write_section_data(cdata, s, tss,
                           p, p_end - p, 1);
      }
    } else {
      if (cc_ok) {
        write_section_data(cdata, s, tss,
                           p, p_end - p, 0);
      }
    }
  } else {
    int ret;
    // Note: The position here points actually behind the current packet.
    if ((ret = tss->u.pes_filter.pes_cb(cdata, tss, p, p_end - p, is_start,
                                        pos - ts->raw_packet_size)) < 0)
      return ret;
  }


  return 0;
}


/* XXX: try to find a better synchro over several packets (use
   get_packet_size() ?) */
static int mpegts_resync(lives_clip_data_t *cdata) {
  lives_mpegts_priv_t *priv=cdata->priv;
  int pb=priv->fd;
  int c, i;

  for (i = 0; i < MAX_RESYNC_SIZE; i++) {
    c = lives_rf8(cdata,pb);
    if (check_for_eof(cdata))
      return -1;
    if (c == 0x47) {
      priv->input_position--;
      lseek(pb, priv->input_position, SEEK_SET);
      return 0;
    }
  }
  fprintf(stderr, "mpegts_Decoder: max resync size reached, could not find sync byte\n");
  /* no sync found */
  return -1;
}


/* return -1 if error or EOF. Return 0 if OK. */
static int read_packet(lives_clip_data_t *cdata, uint8_t *buf, int raw_packet_size) {
  lives_mpegts_priv_t *priv=cdata->priv;
  int pb = priv->fd;

  int skip, len;

  for (;;) {
    len = read(pb, buf, TS_PACKET_SIZE);
    if (len != TS_PACKET_SIZE) {
      priv->input_position+=len;
      return len < 0 ? len : -1;
    }
    priv->input_position+=len;

    /* check packet sync byte */
    if (buf[0] != 0x47) {
      /* find a new packet start */
      priv->input_position-=TS_PACKET_SIZE;
      lseek(pb, priv->input_position, SEEK_SET);
      if (mpegts_resync(cdata) < 0)
        return -1;
      else
        continue;
    } else {
      skip = raw_packet_size - TS_PACKET_SIZE;
      if (skip > 0) {
        lives_seek(cdata, pb, priv->input_position+skip);
      }
      break;
    }
  }
  return 0;
}




static int handle_packets(lives_clip_data_t *cdata, int nb_packets) {
  lives_mpegts_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;
  MpegTSContext *ts = s->priv_data;

  uint8_t packet[TS_PACKET_SIZE];
  int packet_num, ret = 0;

  if (priv->input_position != ts->last_pos) {
    int i;
#ifdef DEBUG
    fprintf(stderr, "mpegts_decoder: Skipping after seek\n");
#endif
    /* seek detected, flush pes buffer */
    for (i = 0; i < NB_PID_MAX; i++) {
      if (ts->pids[i]) {

        if (ts->pids[i]->type == MPEGTS_PES) {

          PESContext *pes = ts->pids[i]->u.pes_filter.opaque;
          av_freep(&pes->buffer);
          pes->data_index = 0;
          pes->state = MPEGTS_SKIP; /* skip until pes header */
        }
        ts->pids[i]->last_cc = -1;
      }
    }
  }

  ts->stop_parse = 0;
  packet_num = 0;
  for (;;) {
    packet_num++;
    if ((nb_packets != 0 && packet_num >= nb_packets) ||
        ts->stop_parse > 1) {
      ret = -1;
      break;
    }
    if (ts->stop_parse > 0)
      break;

    ret = read_packet(cdata, packet, ts->raw_packet_size);
    if (ret != 0)
      break;

    ret = handle_packet(cdata, packet);

    if (ret != 0)
      break;
  }
  ts->last_pos = priv->input_position;
  return ret;
}


#define MPEGTS_PROBE_SIZE 16384

static boolean lives_mpegts_probe(const lives_clip_data_t *cdata, unsigned char *p) {

  const int size= MPEGTS_PROBE_SIZE;
  int score, fec_score, dvhs_score;
  int check_count= size / TS_FEC_PACKET_SIZE;
#define CHECK_COUNT 10

  if (check_count < CHECK_COUNT) return FALSE;

  score      = analyze(p, TS_PACKET_SIZE     *check_count, TS_PACKET_SIZE     , NULL)*CHECK_COUNT/check_count;
  dvhs_score = analyze(p, TS_DVHS_PACKET_SIZE*check_count, TS_DVHS_PACKET_SIZE, NULL)*CHECK_COUNT/check_count;
  fec_score  = analyze(p, TS_FEC_PACKET_SIZE *check_count, TS_FEC_PACKET_SIZE , NULL)*CHECK_COUNT/check_count;

  if (score > fec_score && score > dvhs_score && score > 6) return TRUE;
  else if (dvhs_score > score && dvhs_score > fec_score && dvhs_score > 6) return TRUE;
  else if (fec_score > 6) return TRUE;
  else return FALSE;
}


/* return the 90kHz PCR and the extension for the 27MHz PCR. return
   (-1) if not available */
static int parse_pcr(int64_t *ppcr_high, int *ppcr_low, const uint8_t *packet) {
  int afc, len, flags;
  const uint8_t *p;
  unsigned int v;

  afc = (packet[3] >> 4) & 3;
  if (afc <= 1)
    return -1;
  p = packet + 4;
  len = p[0];
  p++;
  if (len == 0)
    return -1;
  flags = *p++;
  len--;
  if (!(flags & 0x10))
    return -1;
  if (len < 6)
    return -1;
  v = lives_rb32(p);
  *ppcr_high = ((int64_t)v << 1) | (p[4] >> 7);
  *ppcr_low = ((p[4] & 1) << 8) | p[5];
  return 0;
}


static int lives_mpegts_read_header(lives_clip_data_t *cdata) {
  lives_mpegts_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;
  MpegTSContext *ts = s->priv_data;
  int pb = priv->fd;
  uint8_t buf[8*1024];
  int len;
  int64_t pos;

  /* read the first 8192 bytes to get packet size */
  pos = priv->input_position;
  len = read(pb, buf, sizeof(buf));

  if (len != sizeof(buf)) {
    if (len>0) priv->input_position+=len;
    goto fail;
  }

  priv->input_position+=len;

  ts->raw_packet_size = get_packet_size(buf, sizeof(buf));
  if (ts->raw_packet_size <= 0) {
    fprintf(stderr, "mpegts_decoder: Could not detect TS packet size, defaulting to non-FEC/DVHS\n");
    ts->raw_packet_size = TS_PACKET_SIZE;
  }

  ts->stream = s;
  ts->auto_guess = 0;

  if (1) {//s->iformat == &ff_mpegts_demuxer) {
    /* normal demux */

    /* first do a scanning to get all the services */
    /* NOTE: We attempt to seek on non-seekable files as well, as the
     * probe buffer usually is big enough. Only warn if the seek failed
     * on files where the seek should work. */
    if (lseek(pb, pos, SEEK_SET) < 0)
      fprintf(stderr, "mpegts_decoder: Unable to seek back to the start\n");

    mpegts_open_section_filter(ts, SDT_PID, sdt_cb, ts, 1);

    mpegts_open_section_filter(ts, PAT_PID, pat_cb, ts, 1);

    handle_packets(cdata, s->probesize / ts->raw_packet_size);
    /* if could not find service, enable auto_guess */

    ts->auto_guess = 1;

    //av_dlog(ts->stream, "tuning done\n");

    s->ctx_flags |= AVFMTCTX_NOHEADER;
  } else {
    AVStream *st;
    int pcr_pid, pid, nb_packets, nb_pcrs, ret, pcr_l;
    int64_t pcrs[2], pcr_h;
    int packet_count[2];
    uint8_t packet[TS_PACKET_SIZE];

    /* only read packets */

    st = av_new_stream(s, 0);
    if (!st)
      goto fail;
    av_set_pts_info(st, 60, 1, 27000000);
    st->codec->codec_type = AVMEDIA_TYPE_DATA;
    st->codec->codec_id = CODEC_ID_MPEG2TS;

    if (priv->vidst==NULL) {
      priv->vidst=st;
      //fprintf(stderr,"mpegts_decoder: got video stream\n");
    }


    /* we iterate until we find two PCRs to estimate the bitrate */
    pcr_pid = -1;
    nb_pcrs = 0;
    nb_packets = 0;
    for (;;) {
      ret = read_packet(cdata, packet, ts->raw_packet_size);
      if (ret < 0)
        return -1;
      pid = lives_rb16(packet + 1) & 0x1fff;
      if ((pcr_pid == -1 || pcr_pid == pid) &&
          parse_pcr(&pcr_h, &pcr_l, packet) == 0) {
        pcr_pid = pid;
        packet_count[nb_pcrs] = nb_packets;
        pcrs[nb_pcrs] = pcr_h * 300 + pcr_l;
        nb_pcrs++;
        if (nb_pcrs >= 2)
          break;
      }
      nb_packets++;
    }

    /* NOTE1: the bitrate is computed without the FEC */
    /* NOTE2: it is only the bitrate of the start of the stream */
    ts->pcr_incr = (pcrs[1] - pcrs[0]) / (packet_count[1] - packet_count[0]);
    ts->cur_pcr = pcrs[0] - ts->pcr_incr * packet_count[0];
    s->bit_rate = (TS_PACKET_SIZE * 8) * 27e6 / ts->pcr_incr;
    st->codec->bit_rate = s->bit_rate;
    st->start_time = ts->cur_pcr;
    //av_dlog(ts->stream, "start=%0.3f pcr=%0.3f incr=%d\n",
    //	    st->start_time / 1000000.0, pcrs[0] / 27e6, ts->pcr_incr);
  }

  lseek(pb, pos, SEEK_SET);
  return 0;
fail:
  return -1;
}


static boolean mpegts_read_packet(lives_clip_data_t *cdata, AVPacket *pkt) {
  lives_mpegts_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;
  MpegTSContext *ts = s->priv_data;
  int ret, i;

  ts->pkt = pkt;
  ret = handle_packets(cdata, 0);
  if (ret < 0) {
    /* flush pes data left */
    for (i = 0; i < NB_PID_MAX; i++) {
      if (ts->pids[i] && ts->pids[i]->type == MPEGTS_PES) {
        PESContext *pes = ts->pids[i]->u.pes_filter.opaque;
        if (pes->state == MPEGTS_PAYLOAD && pes->data_index > 0) {
          new_pes_packet(pes, pkt);
          pes->state = MPEGTS_SKIP;
          ret = 0;
          break;
        }
      }
    }
  }

  return ret;
}



static int mpegts_read_close(lives_clip_data_t *cdata) {
  lives_mpegts_priv_t *priv=cdata->priv;
  AVFormatContext *s=priv->s;

  MpegTSContext *ts = s->priv_data;
  int i;

  clear_programs(ts);

  for (i=0; i<NB_PID_MAX; i++)
    if (ts->pids[i]) mpegts_close_filter(ts, ts->pids[i]);

  return 0;
}



///////////////////////////////////////////////////////////////////////////




static int64_t dts_to_frame(const lives_clip_data_t *cdata, int64_t dts) {
  // use ADJUSTED dts (subtract priv->start_dts from it)
  return (int64_t)((double)dts/90000.*cdata->fps+.5);
}

static int64_t frame_to_dts(const lives_clip_data_t *cdata, int64_t frame) {
  // returns UNADJUSTED dts : add priv->start_dts to it
  return (int64_t)((double)frame*90000./cdata->fps+.5);
}








const char *module_check_init(void) {
  avcodec_register_all();
  av_log_set_level(0);
  indices=NULL;
  nidxc=0;
  pthread_mutex_init(&indices_mutex,NULL);
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata(void) {
  lives_mpegts_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));

  cdata->URI=NULL;

  cdata->priv=priv=malloc(sizeof(lives_mpegts_priv_t));

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

  cdata->fps=0.;

  cdata->video_start_time=0.;

  memset(cdata->author,0,1);
  memset(cdata->title,0,1);
  memset(cdata->comment,0,1);

  return cdata;
}

static index_entry *mpegts_read_seek(const lives_clip_data_t *cdata, uint32_t timestamp) {
  // use unadj timestamp

  lives_mpegts_priv_t *priv=cdata->priv;

  index_entry *idx;

  if (!priv->idxc->idxhh) return NULL;

  pthread_mutex_lock(&priv->idxc->mutex);
  timestamp = FFMIN(timestamp, frame_to_dts(cdata,cdata->nframes));
  timestamp = FFMAX(timestamp, priv->idxc->idxhh->dts);

  idx=get_idx_for_pts(cdata,timestamp);

  priv->input_position=idx->offs;
  pthread_mutex_unlock(&priv->idxc->mutex);

  lseek(priv->fd,priv->input_position,SEEK_SET);

  if (priv->avpkt.data!=NULL) {
    free(priv->avpkt.data);
    priv->avpkt.data=NULL;
    priv->avpkt.size=0;
  }

  avcodec_flush_buffers(priv->ctx);

  return idx;
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
  lives_mpegts_priv_t *priv=cdata->priv;
  index_container_t *idxc=priv->idxc;
  register int i,j;

  if (idxc==NULL) return;

  pthread_mutex_lock(&indices_mutex);

  if (idxc->nclients==1) {
    mpegts_save_index(idxc->clients[0]);

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
  lives_mpegts_priv_t *priv=cdata->priv;

  cdata->seek_flag=0;

  if (priv->s) mpegts_read_close(cdata);

  if (priv->ctx!=NULL) {
    avcodec_close(priv->ctx);
    av_free(priv->ctx);
  }

  if (priv->s!=NULL) {
    av_free(priv->s);
  }

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

  close(priv->fd);
}

static int64_t mpegts_load_index(lives_clip_data_t *cdata);

int64_t get_last_video_dts(lives_clip_data_t *cdata) {
  lives_mpegts_priv_t *priv=cdata->priv;
  boolean got_picture=FALSE;
  int len;
  int64_t dts,last_dts=-1;
  int64_t idxpos,idxpos_data=0;

  // see if we have a file from previous open
  pthread_mutex_lock(&priv->idxc->mutex);
  if ((dts=mpegts_load_index(cdata))>0) {
    pthread_mutex_unlock(&priv->idxc->mutex);
    return dts+priv->start_dts;
  }
  pthread_mutex_unlock(&priv->idxc->mutex);

  priv->input_position=priv->data_start;
  lseek(priv->fd,priv->input_position,SEEK_SET);
  avcodec_flush_buffers(priv->ctx);
  mpegts_read_packet(cdata,&priv->avpkt);
  priv->got_eof=FALSE;

  idxpos=priv->input_position;

  // get each packet and decode just the first frame

  while (1) {
    got_picture=FALSE;

    while (!got_picture) {

#if LIBAVCODEC_VERSION_MAJOR >= 52
      len=avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
      len=avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

      if (got_picture) {
        idxpos_data=idxpos;
        dts=priv->avpkt.dts-priv->start_dts;
        pthread_mutex_lock(&priv->idxc->mutex);
        lives_add_idx(cdata,idxpos,dts);
        pthread_mutex_unlock(&priv->idxc->mutex);
        avcodec_flush_buffers(priv->ctx);
        idxpos=priv->input_position;
      }

      if (len<0||len==priv->avpkt.size||got_picture) {
        if (priv->avpkt.data!=NULL) {
          free(priv->avpkt.data);
          priv->avpkt.data=NULL;
          priv->avpkt.size=0;
        }
        if (priv->input_position==priv->filesize) goto vts_next;
        mpegts_read_packet((lives_clip_data_t *)cdata,&priv->avpkt);
        if (priv->got_eof) goto vts_next;
      }

    }
  }

vts_next:

  // rewind back to last pos, and decode up to end now

  priv->input_position=idxpos_data;
  lseek(priv->fd,priv->input_position,SEEK_SET);
  priv->got_eof=FALSE;
  avcodec_flush_buffers(priv->ctx);
  mpegts_read_packet(cdata,&priv->avpkt);

  while (1) {

#if LIBAVCODEC_VERSION_MAJOR >= 52
    len=avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    len=avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

    if (got_picture) last_dts=priv->avpkt.dts;

    if (len==priv->avpkt.size) {
      if (priv->avpkt.data!=NULL) {
        free(priv->avpkt.data);
        priv->avpkt.data=NULL;
        priv->avpkt.size=0;
      }
      if (priv->input_position==priv->filesize) goto vts_done;
      mpegts_read_packet((lives_clip_data_t *)cdata,&priv->avpkt);
      if (priv->got_eof) goto vts_done;
    }

  }

vts_done:

  priv->got_eof=FALSE;
  return last_dts;

}



static boolean attach_stream(lives_clip_data_t *cdata, boolean isclone) {
  // open the file and get a handle
  lives_mpegts_priv_t *priv=cdata->priv;
  unsigned char header[MPEGTS_PROBE_SIZE];
  int64_t ldts,dts,pts;
  double fps;

  int len;


  AVCodec *codec=NULL;
  AVCodecContext *ctx;

  boolean got_picture=FALSE;
  boolean is_partial_clone=FALSE;

  struct stat sb;

  //#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"\n");
#endif

  if (isclone&&!priv->inited) {
    isclone=FALSE;
    if (cdata->fps>0.&&cdata->nframes>0)
      is_partial_clone=TRUE;
  }

  priv->has_audio=priv->has_video=FALSE;
  priv->vidst=NULL;
  priv->vididx=-1;

  priv->got_eof=FALSE;

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "mpegts_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

#ifdef IS_MINGW
  setmode(priv->fd,O_BINARY);
#endif

  if (isclone) goto seek_skip;

  fstat(priv->fd,&sb);
  priv->filesize=sb.st_size;

  if (read(priv->fd, header, MPEGTS_PROBE_SIZE) < MPEGTS_PROBE_SIZE) {
    // for example, might be a directory
#ifdef DEBUG
    fprintf(stderr, "mpegts_decoder: unable to read header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }

  priv->input_position+=MPEGTS_PROBE_SIZE;

  if (!lives_mpegts_probe(cdata, header)) {
#ifdef DEBUG
    fprintf(stderr, "mpegts_decoder: unable to parse header for %s\n",cdata->URI);
#endif
    close(priv->fd);
    return FALSE;
  }

  priv->input_position=0;
  lseek(priv->fd,priv->input_position,SEEK_SET);

  if (!is_partial_clone) cdata->fps=0.;
  cdata->width=cdata->frame_width=cdata->height=cdata->frame_height=0;
  cdata->offs_x=cdata->offs_y=0;

  cdata->arate=0;
  cdata->achans=0;
  cdata->asamps=16;

  cdata->sync_hint=SYNC_HINT_AUDIO_TRIM_START;

  sprintf(cdata->audio_name,"%s","");


seek_skip:
  priv->idxc=idxc_for(cdata);
  priv->inited=TRUE;

  priv->s = avformat_alloc_context();

  priv->ts=malloc(sizeof(MpegTSContext));
  memset(priv->ts,0,sizeof(MpegTSContext));

  priv->ts->raw_packet_size = TS_PACKET_SIZE;
  //ts->stream = s;
  priv->ts->auto_guess = 1;
  priv->s->priv_data = priv->ts;

  av_init_packet(&priv->avpkt);
  priv->avpkt.data=NULL;
  priv->ctx=NULL;

  if (lives_mpegts_read_header(cdata)) {
    close(priv->fd);
    return FALSE;
  }

  if (priv->vidst==NULL) {
    fprintf(stderr, "mpegts_decoder: Got no video stream !\n");
    detach_stream(cdata);
    return FALSE;
  }

  priv->data_start=priv->input_position;

  if (isclone) goto skip_det;

  cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;

  cdata->offs_x=0;
  cdata->offs_y=0;

  switch (priv->vidst->codec->codec_id) {
  case CODEC_ID_DIRAC:
    sprintf(cdata->video_name,"%s","dirac");
    break;
  case CODEC_ID_H264:
    sprintf(cdata->video_name,"%s","h264");
    break;
  case CODEC_ID_MPEG1VIDEO:
    sprintf(cdata->video_name,"%s","mpeg1");
    break;
  case CODEC_ID_MPEG2VIDEO:
    sprintf(cdata->video_name,"%s","mpeg2");
    break;
  case CODEC_ID_MPEG4:
    sprintf(cdata->video_name,"%s","mpeg2");
    break;
  case CODEC_ID_VC1:
    sprintf(cdata->video_name,"%s","vc1");
    break;
  default:
    sprintf(cdata->video_name,"%s","unknown");
    break;
  }

  //#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"video type is %f %s %d x %d (%d x %d +%d +%d)\n",duration,cdata->video_name,
          cdata->width,cdata->height,cdata->frame_width,cdata->frame_height,cdata->offs_x,cdata->offs_y);
#endif

skip_det:

  codec = avcodec_find_decoder(priv->vidst->codec->codec_id);

  if (!codec) {
    if (strlen(cdata->video_name)>0)
      fprintf(stderr, "mpegts_decoder: Could not find avcodec codec for video type %s\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }

  priv->ctx = ctx = avcodec_alloc_context3(codec);

  if (avcodec_open2(ctx, codec, NULL) < 0) {
    fprintf(stderr, "mpegts_decoder: Could not open avcodec context\n");
    detach_stream(cdata);
    return FALSE;
  }

  priv->codec=codec;

  if (codec->capabilities&CODEC_CAP_TRUNCATED)
    ctx->flags|= CODEC_FLAG_TRUNCATED;

  // re-scan with avcodec; priv->data_start holds video data start position

  av_init_packet(&priv->avpkt);
  if (priv->avpkt.data!=NULL) free(priv->avpkt.data);
  priv->avpkt.data=NULL;

  priv->input_position=priv->data_start;
  lseek(priv->fd,priv->input_position,SEEK_SET);
  avcodec_flush_buffers(priv->ctx);

  priv->picture = av_frame_alloc();

  mpegts_read_packet(cdata,&priv->avpkt);

  while (!got_picture&&!priv->got_eof) {


#if LIBAVCODEC_VERSION_MAJOR >= 52
    len=avcodec_decode_video2(ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    len=avcodec_decode_video(ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

    if (len<0||len==priv->avpkt.size) {
      if (priv->avpkt.data!=NULL) {
        free(priv->avpkt.data);
        priv->avpkt.data=NULL;
        priv->avpkt.size=0;
      }
      mpegts_read_packet(cdata,&priv->avpkt);
    }


  }

  priv->last_frame=-1;

  if (isclone) {
    if (priv->picture!=NULL) av_frame_free(&priv->picture);
    priv->picture=NULL;
    return TRUE;
  }


  if (!got_picture) {
    fprintf(stderr,"mpegts_decoder: could not get picture.\n PLEASE SEND A PATCH FOR %s FORMAT.\n",cdata->video_name);
    detach_stream(cdata);
    return FALSE;
  }

  pts=priv->avpkt.pts;
  dts=priv->avpkt.dts;

  priv->start_dts=dts;


  //fprintf(stderr,"got dts %ld pts %ld\n",dts,pts);

  got_picture=0;

  while (!got_picture&&!priv->got_eof) {

#if LIBAVCODEC_VERSION_MAJOR >= 52
    len=avcodec_decode_video2(ctx, priv->picture, &got_picture, &priv->avpkt);
#else
    len=avcodec_decode_video(ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

    if (len==priv->avpkt.size) {
      if (priv->avpkt.data!=NULL) {
        free(priv->avpkt.data);
        priv->avpkt.data=NULL;
        priv->avpkt.size=0;
      }
      mpegts_read_packet(cdata,&priv->avpkt);
    }

  }

  if (got_picture) {
    pts=priv->avpkt.pts-pts;
    dts=priv->avpkt.dts-dts;
    //fprintf(stderr,"got second picture %ld %ld\n",pts,dts);
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
    fprintf(stderr, "mpegts_decoder: Could not find a usable palette for (%d) %s\n",ctx->pix_fmt,cdata->URI);
    detach_stream(cdata);
    return FALSE;
  }

  cdata->current_palette=cdata->palettes[0];

  // re-get fps, width, height, nframes - actually avcodec is pretty useless at getting this
  // so we fall back on the values we obtained ourselves

  if (cdata->width==0) cdata->width=ctx->width-cdata->offs_x*2;
  if (cdata->height==0) cdata->height=ctx->height-cdata->offs_y*2;

  if (cdata->width*cdata->height==0) {
    fprintf(stderr, "mpegts_decoder: invalid width and height (%d X %d)\n",cdata->width,cdata->height);
    detach_stream(cdata);
    return FALSE;
  }
  //#define DEBUG
#ifdef DEBUG
  fprintf(stderr,"using palette %d, size %d x %d\n",
          cdata->current_palette,cdata->width,cdata->height);
#endif

  cdata->par=(double)ctx->sample_aspect_ratio.num/(double)ctx->sample_aspect_ratio.den;
  if (cdata->par==0.) cdata->par=1.;

  if (cdata->fps==0.&&ctx->time_base.den>0&&ctx->time_base.num>0) {
    fps=(double)ctx->time_base.den/(double)ctx->time_base.num;
    if (fps!=1000.) cdata->fps=fps;
  }

  if (cdata->fps==0.) {
    if (pts!=0) cdata->fps=180000./(double)pts;
    else if (dts!=0) cdata->fps=180000./(double)dts;
  }

#ifndef IS_MINGW

  if (cdata->fps==0.||cdata->fps==1000.) {
    int res = get_fps(cdata->URI);
    if (res >= 0) cdata->fps = res;
  }

#endif

  if (cdata->fps==0.||cdata->fps==1000.) {
    // if mplayer fails, count the frames between index entries
    //dts=calc_dts_delta(cdata);
    //if (dts!=0) cdata->fps=1000./(double)dts;
  }

  if (cdata->fps==0.||cdata->fps==1000.) {
    fprintf(stderr, "mpegts_decoder: invalid framerate %.4f (%d / %d)\n",
            cdata->fps,ctx->time_base.den,ctx->time_base.num);
    detach_stream(cdata);
    return FALSE;
  }


  if (ctx->ticks_per_frame==2) {
    // TODO - needs checking
    cdata->fps/=2.;
    cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;
  }

  if (is_partial_clone) {
    // see if we have a file from previous open
    pthread_mutex_lock(&priv->idxc->mutex);
    mpegts_load_index(cdata);
    pthread_mutex_unlock(&priv->idxc->mutex);
    return TRUE;
  }

  ldts=get_last_video_dts(cdata);

  if (ldts==-1) {
    fprintf(stderr, "mpegts_decoder: could not read last dts\n");
    detach_stream(cdata);
    return FALSE;
  }

  ldts-=priv->start_dts;

  cdata->nframes=dts_to_frame(cdata,ldts)+2;

  //fprintf(stderr,"check for %ld frames\n",cdata->nframes);

  // double check, sometimes we can be out by one or two frames
  while (1) {
    priv->expect_eof=TRUE;
    priv->got_eof=FALSE;
    if (get_frame(cdata,cdata->nframes-1,NULL,0,NULL)) {
      if (!priv->got_eof) break;
    }
    cdata->nframes--;
  }
  priv->expect_eof=FALSE;


#ifdef DEBUG
  fprintf(stderr,"fps is %.4f %ld %ld %ld\n",cdata->fps,cdata->nframes,ldts,priv->start_dts);
#endif

  if (priv->picture!=NULL) av_frame_free(&priv->picture);
  priv->picture=NULL;

  return TRUE;
}


static lives_clip_data_t *mpegts_clone(lives_clip_data_t *cdata) {
  lives_clip_data_t *clone=init_cdata();
  lives_mpegts_priv_t *dpriv,*spriv;

  // copy from cdata to clone, with a new context for clone
  clone->URI=strdup(cdata->URI);

  // create "priv" elements
  dpriv=clone->priv;
  spriv=cdata->priv;

  if (spriv!=NULL) dpriv->filesize=spriv->filesize;

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

  snprintf(clone->author,256,"%s",cdata->author);
  snprintf(clone->title,256,"%s",cdata->title);
  snprintf(clone->comment,256,"%s",cdata->comment);

  if (spriv!=NULL) dpriv->inited=TRUE;

  if (!attach_stream(clone,TRUE)) {
    free(clone->URI);
    clone->URI=NULL;
    clip_data_free(clone);
    return NULL;
  }

  if (spriv!=NULL) {
    clone->nclips=cdata->nclips;
    snprintf(clone->container_name,512,"%s",cdata->container_name);
    snprintf(clone->video_name,512,"%s",cdata->video_name);
    clone->arate=cdata->arate;
    clone->achans=cdata->achans;
    clone->asamps=cdata->asamps;
    clone->asigned=cdata->asigned;
    clone->ainterleaf=cdata->ainterleaf;
    snprintf(clone->audio_name,512,"%s",cdata->audio_name);
    clone->seek_flag=cdata->seek_flag;
    clone->sync_hint=cdata->sync_hint;

    dpriv->data_start=spriv->data_start;
    dpriv->start_dts=spriv->start_dts;
  }

  else {
    clone->nclips=1;

    ///////////////////////////////////////////////////////////

    sprintf(clone->container_name,"%s","mpegts");

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

  dpriv->last_frame=-1;
  dpriv->expect_eof=FALSE;
  dpriv->got_eof=FALSE;

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

  lives_mpegts_priv_t *priv;

  if (URI==NULL&&cdata!=NULL) {
    // create a clone of cdata - we also need to be able to handle a "fake" clone with only URI, nframes and fps set (priv == NULL)
    return mpegts_clone(cdata);
  }

  if (cdata!=NULL&&cdata->current_clip>0) {
    // currently we only support one clip per container

    //clip_data_free(cdata);
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
      //clip_data_free(cdata);
      return NULL;
    }
    cdata->current_palette=cdata->palettes[0];
    cdata->current_clip=0;
  }

  cdata->nclips=1;

  ///////////////////////////////////////////////////////////

  sprintf(cdata->container_name,"%s","mpegts");

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
  int len;
  int64_t target_pts=frame_to_dts(cdata,tframe);
  int64_t nextframe=0;
  lives_mpegts_priv_t *priv=cdata->priv;

  int xheight=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;
  int rescan_limit=16;  // pick some arbitrary value
  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;

  boolean got_picture=FALSE;
  unsigned char *dst,*src;//,flags;
  unsigned char black[4]= {0,0,0,255};

  index_entry *idx;
  register int i,p;


  //#define DEBUG_KFRAMES
#ifdef DEBUG_KFRAMES
  fprintf(stderr,"vals %ld %ld\n",tframe,priv->last_frame);
#endif

  priv->got_eof=FALSE;

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

    if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||
        pal==WEED_PALETTE_UYVY8888||pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV888||
        pal==WEED_PALETTE_YUVA8888) psize=4;

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
      idx=mpegts_read_seek(cdata,target_pts);

      nextframe=dts_to_frame(cdata,idx->dts);

      if (priv->input_position==priv->filesize) return FALSE;
      mpegts_read_packet((lives_clip_data_t *)cdata,&priv->avpkt);
      if (priv->got_eof) return FALSE;

      //#define DEBUG_KFRAMES
#ifdef DEBUG_KFRAMES
      if (idx!=NULL) printf("got kframe %ld for frame %ld\n",nextframe,tframe);
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

#if LIBAVCODEC_VERSION_MAJOR >= 52
        len=avcodec_decode_video2(priv->ctx, priv->picture, &got_picture, &priv->avpkt);
#else
        len=avcodec_decode_video(priv->ctx, priv->picture, &got_picture, priv->avpkt.data, priv->avpkt.size);
#endif

        if (len==priv->avpkt.size) {
          if (priv->avpkt.data!=NULL) {
            free(priv->avpkt.data);
            priv->avpkt.data=NULL;
            priv->avpkt.size=0;
          }

          if (priv->input_position==priv->filesize) return FALSE;
          mpegts_read_packet((lives_clip_data_t *)cdata,&priv->avpkt);
          if (priv->got_eof) return FALSE;

        }
      }

      nextframe++;
      if (nextframe>cdata->nframes) return FALSE;
    } while (nextframe<=tframe);

    /////////////////////////////////////////////////////

  }

  if (pixel_data==NULL||priv->picture==NULL) return TRUE;

  for (p=0; p<nplanes; p++) {
    dst=pixel_data[p];
    src=priv->picture->data[p];

    for (i=0; i<xheight; i++) {
      if (i<btop||i>bbot) {
        // top or bottom border, copy black row
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P
            ||pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24
            ||pal==WEED_PALETTE_BGR24) {
          memset(dst,black[p],dstwidth+(bleft+bright)*psize);
          dst+=dstwidth+(bleft+bright)*psize;
        } else dst+=write_black_pixel(dst,pal,dstwidth/psize+bleft+bright,y_black);
        continue;
      }

      if (bleft>0) {
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||
            pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24
            ||pal==WEED_PALETTE_BGR24) {
          memset(dst,black[p],bleft*psize);
          dst+=bleft*psize;
        } else dst+=write_black_pixel(dst,pal,bleft,y_black);
      }

      memcpy(dst,src,dstwidth);
      dst+=dstwidth;

      if (bright>0) {
        if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P
            ||pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||
            pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
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

#if !defined (IS_MINGW) && !defined (IS_SOLARIS) && !defined (__FreeBSD__)

# if __BYTE_ORDER == __BIG_ENDIAN

static void reverse_bytes(uint8_t *out, const uint8_t *in, size_t count) {
  register int i;
  for (i=0; i<count; i++) {
    out[i]=in[count-i-1];
  }
}

#endif

#endif

static ssize_t lives_write_le(int fd, const void *buf, size_t count) {
#if !defined (IS_MINGW) && !defined (IS_SOLARIS) && !defined (__FreeBSD__)
# if __BYTE_ORDER == __BIG_ENDIAN
  uint8_t xbuf[count];
  reverse_bytes(xbuf,(const uint8_t *)buf,count);
  return write(fd,xbuf,count);
# else
  return write(fd,buf,count);
# endif
#else
  return write(fd,buf,count);
#endif

}


ssize_t lives_read_le(int fd, void *buf, size_t count) {
#if !defined (IS_MINGW) && !defined (IS_SOLARIS) && !defined (__FreeBSD__)
# if __BYTE_ORDER == __BIG_ENDIAN
  uint8_t xbuf[count];
  ssize_t retval=read(fd,buf,count);
  if (retval<count) return retval;
  reverse_bytes((uint8_t *)buf,(const uint8_t *)xbuf,count);
  return retval;
#else
  return read(fd,buf,count);
#endif
#else
  return read(fd,buf,count);
#endif
}





static void mpegts_save_index(lives_clip_data_t *cdata) {

  lives_mpegts_priv_t *priv=cdata->priv;
  index_entry *idx=priv->idxc->idxhh;

  int fd;


  int64_t max_dts=frame_to_dts(cdata,cdata->nframes);

  const char ver[4]="V1.0";

  if (idx==NULL) return;

  if ((fd=open("sync_index",O_CREAT|O_TRUNC|O_WRONLY,S_IRUSR|S_IWUSR))==-1) return;

  if (write(fd,ver,4)<4) goto donewr;

  if (lives_write_le(fd,&max_dts,8)<8) goto failwr;


  // dump index to file in le format

  while (idx!=NULL) {
    if (lives_write_le(fd,&idx->dts,8)<8) goto failwr;
    if (lives_write_le(fd,&idx->offs,8)<8) goto failwr;
    idx=idx->next;
  }

donewr:
  close(fd);
  return;

failwr:
  close(fd);
  unlink("sync_index");
  return;

}




static int64_t mpegts_load_index(lives_clip_data_t *cdata) {
  // returns max_dts
  char hdr[4];

  lives_mpegts_priv_t *priv=cdata->priv;

  int64_t dts,last_dts=0,max_dts=0;
  uint64_t offs,last_offs=0;

  int fd;
  int count=0;

  ssize_t bytes;

  if ((fd=open("sync_index",O_RDONLY))<0) return 0;

  if (read(fd,hdr,4)<4) goto donerd;

  if (strncmp(hdr,"V1.0",4)) goto donerd;

  bytes=lives_read_le(fd,&max_dts,8);
  if (bytes<8) goto failrd;

  if (max_dts<last_dts) goto failrd;

  while (1) {
    bytes=lives_read_le(fd,&dts,8);
    if (bytes<8) break;

    if (dts<last_dts) goto failrd;
    if (dts>max_dts) goto failrd;

    bytes=lives_read_le(fd,&offs,8);
    if (bytes<8) break;

    if (offs<last_offs) goto failrd;
    if (offs>=priv->filesize) goto failrd;

    lives_add_idx(cdata,offs,dts);

    last_dts=dts;
    last_offs=offs;

    count++;
  }

donerd:
  close(fd);
  return max_dts;

failrd:
  if (priv->idxc->idxhh!=NULL) idxc_release(cdata);

  close(fd);
  return 0;

}


void clip_data_free(lives_clip_data_t *cdata) {
  lives_mpegts_priv_t *priv=cdata->priv;

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

