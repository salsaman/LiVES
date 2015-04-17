/*
 * MPEG2 transport stream defines
 * Copyright (c) 2003 Fabrice Bellard
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef LIVES_MPEGTS_H
#define LIVES_MPEGTS_H

#define TS_FEC_PACKET_SIZE 204
#define TS_DVHS_PACKET_SIZE 192
#define TS_PACKET_SIZE 188
#define TS_MAX_PACKET_SIZE 204

#define NB_PID_MAX 8192
#define MAX_SECTION_SIZE 4096

/* pids */
#define PAT_PID                 0x0000
#define SDT_PID                 0x0011

/* table ids */
#define PAT_TID   0x00
#define PMT_TID   0x02
#define M4OD_TID  0x05
#define SDT_TID   0x42

#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_AUDIO_AAC_LATM  0x11
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_VC1       0xea
#define STREAM_TYPE_VIDEO_DIRAC     0xd1

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a

typedef struct MpegTSContext MpegTSContext;

MpegTSContext *ff_mpegts_parse_open(AVFormatContext *s);
void ff_mpegts_parse_close(MpegTSContext *ts);

int ff_mpegts_parse_packet(lives_clip_data_t *cdata, MpegTSContext *ts, AVPacket *pkt,
                           const uint8_t *buf, int len);

typedef struct {
  int use_au_start;
  int use_au_end;
  int use_rand_acc_pt;
  int use_padding;
  int use_timestamps;
  int use_idle;
  int timestamp_res;
  int timestamp_len;
  int ocr_len;
  int au_len;
  int inst_bitrate_len;
  int degr_prior_len;
  int au_seq_num_len;
  int packet_seq_num_len;
} SLConfigDescr;

typedef struct {
  int es_id;
  int dec_config_descr_len;
  uint8_t *dec_config_descr;
  SLConfigDescr sl;
} Mp4Descr;

/**
 * Parse an MPEG-2 descriptor
 * @param[in] fc                    Format context (used for logging only)
 * @param st                        Stream
 * @param stream_type               STREAM_TYPE_xxx
 * @param pp                        Descriptor buffer pointer
 * @param desc_list_end             End of buffer
 * @param mp4_dec_config_descr_len  Length of 'mp4_dec_config_descr', or zero if not present
 * @param mp4_es_id
 * @param pid
 * @param mp4_dec_config_descr
 * @return <0 to stop processing
 */
int ff_parse_mpeg2_descriptor(lives_clip_data_t *cdata, AVFormatContext *fc, AVStream *st, int stream_type,
                              const uint8_t **pp, const uint8_t *desc_list_end,
                              Mp4Descr *mp4_descr, int mp4_descr_count, int pid,
                              MpegTSContext *ts);



// get_bits.h



#if defined(ALT_BITSTREAM_READER_LE) && !defined(ALT_BITSTREAM_READER)
#   define ALT_BITSTREAM_READER
#endif

#if !defined(A32_BITSTREAM_READER) && !defined(ALT_BITSTREAM_READER)
#   if ARCH_ARM && !HAVE_FAST_UNALIGNED
#       define A32_BITSTREAM_READER
#   else
#       define ALT_BITSTREAM_READER
//#define A32_BITSTREAM_READER
#   endif
#endif




/* bit input */
/* buffer, buffer_end and size_in_bits must be present and used by every reader */
typedef struct GetBitContext {
  const uint8_t *buffer, *buffer_end;
#ifdef ALT_BITSTREAM_READER
  int index;
#elif defined A32_BITSTREAM_READER
  uint32_t *buffer_ptr;
  uint32_t cache0;
  uint32_t cache1;
  int bit_count;
#endif
  int size_in_bits;
} GetBitContext;


static uint16_t lives_rb32(const uint8_t *x);



#ifdef ALT_BITSTREAM_READER
#   define MIN_CACHE_BITS 25

#   define OPEN_READER(name, gb)                \
    unsigned int name##_index = (gb)->index;    \
    av_unused unsigned int name##_cache

#   define CLOSE_READER(name, gb) (gb)->index = name##_index

# ifdef ALT_BITSTREAM_READER_LE
#   define UPDATE_CACHE(name, gb) \
    name##_cache = AV_RL32(((const uint8_t *)(gb)->buffer)+(name##_index>>3)) >> (name##_index&0x07)

#   define SKIP_CACHE(name, gb, num) name##_cache >>= (num)
# else
#   define UPDATE_CACHE(name, gb) \
    name##_cache = lives_rb32(((const uint8_t *)(gb)->buffer)+(name##_index>>3)) << (name##_index&0x07)

#   define SKIP_CACHE(name, gb, num) name##_cache <<= (num)
# endif

// FIXME name?
#   define SKIP_COUNTER(name, gb, num) name##_index += (num)

#   define SKIP_BITS(name, gb, num) do {        \
        SKIP_CACHE(name, gb, num);              \
        SKIP_COUNTER(name, gb, num);            \
    } while (0)

#   define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)
#   define LAST_SKIP_CACHE(name, gb, num)

# ifdef ALT_BITSTREAM_READER_LE
#   define SHOW_UBITS(name, gb, num) zero_extend(name##_cache, num)

#   define SHOW_SBITS(name, gb, num) sign_extend(name##_cache, num)
# else
#   define SHOW_UBITS(name, gb, num) NEG_USR32(name##_cache, num)

#   define SHOW_SBITS(name, gb, num) NEG_SSR32(name##_cache, num)
# endif

#   define GET_CACHE(name, gb) ((uint32_t)name##_cache)

static inline int get_bits_count(const GetBitContext *s) {
  return s->index;
}

static inline void skip_bits_long(GetBitContext *s, int n) {
  s->index += n;
}

#elif defined A32_BITSTREAM_READER

#   define MIN_CACHE_BITS 32

#   define OPEN_READER(name, gb)                        \
    int name##_bit_count        = (gb)->bit_count;      \
    uint32_t name##_cache0      = (gb)->cache0;         \
    uint32_t name##_cache1      = (gb)->cache1;         \
    uint32_t *name##_buffer_ptr = (gb)->buffer_ptr

#   define CLOSE_READER(name, gb) do {          \
        (gb)->bit_count  = name##_bit_count;    \
        (gb)->cache0     = name##_cache0;       \
        (gb)->cache1     = name##_cache1;       \
        (gb)->buffer_ptr = name##_buffer_ptr;   \
    } while (0)

#   define UPDATE_CACHE(name, gb) do {                                  \
        if(name##_bit_count > 0){                                       \
            const uint32_t next = av_be2ne32(*name##_buffer_ptr);       \
            name##_cache0 |= NEG_USR32(next, name##_bit_count);         \
            name##_cache1 |= next << name##_bit_count;                  \
            name##_buffer_ptr++;                                        \
            name##_bit_count -= 32;                                     \
        }                                                               \
    } while (0)

#if ARCH_X86
#   define SKIP_CACHE(name, gb, num)                            \
    __asm__("shldl %2, %1, %0          \n\t"                    \
            "shll  %2, %1              \n\t"                    \
            : "+r" (name##_cache0), "+r" (name##_cache1)        \
            : "Ic" ((uint8_t)(num)))
#else
#   define SKIP_CACHE(name, gb, num) do {               \
        name##_cache0 <<= (num);                        \
        name##_cache0 |= NEG_USR32(name##_cache1,num);  \
        name##_cache1 <<= (num);                        \
    } while (0)
#endif

#   define SKIP_COUNTER(name, gb, num) name##_bit_count += (num)

#   define SKIP_BITS(name, gb, num) do {        \
        SKIP_CACHE(name, gb, num);              \
        SKIP_COUNTER(name, gb, num);            \
    } while (0)

#   define LAST_SKIP_BITS(name, gb, num)  SKIP_BITS(name, gb, num)
#   define LAST_SKIP_CACHE(name, gb, num) SKIP_CACHE(name, gb, num)

#   define SHOW_UBITS(name, gb, num) NEG_USR32(name##_cache0, num)

#   define SHOW_SBITS(name, gb, num) NEG_SSR32(name##_cache0, num)

#   define GET_CACHE(name, gb) name##_cache0



#endif




#ifndef NEG_SSR32
#   define NEG_SSR32(a,s) ((( int32_t)(a))>>(32-(s)))
#endif

#ifndef NEG_USR32
#   define NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))
#endif





#define MP4ODescrTag                    0x01
#define MP4IODescrTag                   0x02
#define MP4ESDescrTag                   0x03
#define MP4DecConfigDescrTag            0x04
#define MP4DecSpecificDescrTag          0x05
#define MP4SLDescrTag                   0x06


#define CODEC_ID_MPEG4SYSTEMS 0x20001

/* http://www.mp4ra.org */
/* ordered by muxing preference */
const AVCodecTag ff_mp4_obj_type[] = {
  { CODEC_ID_MOV_TEXT  , 0x08 },
  { CODEC_ID_MPEG4     , 0x20 },
  { CODEC_ID_H264      , 0x21 },
  { CODEC_ID_AAC       , 0x40 },
  { CODEC_ID_MP4ALS    , 0x40 }, /* 14496-3 ALS */
  { CODEC_ID_MPEG2VIDEO, 0x61 }, /* MPEG2 Main */
  { CODEC_ID_MPEG2VIDEO, 0x60 }, /* MPEG2 Simple */
  { CODEC_ID_MPEG2VIDEO, 0x62 }, /* MPEG2 SNR */
  { CODEC_ID_MPEG2VIDEO, 0x63 }, /* MPEG2 Spatial */
  { CODEC_ID_MPEG2VIDEO, 0x64 }, /* MPEG2 High */
  { CODEC_ID_MPEG2VIDEO, 0x65 }, /* MPEG2 422 */
  { CODEC_ID_AAC       , 0x66 }, /* MPEG2 AAC Main */
  { CODEC_ID_AAC       , 0x67 }, /* MPEG2 AAC Low */
  { CODEC_ID_AAC       , 0x68 }, /* MPEG2 AAC SSR */
  { CODEC_ID_MP3       , 0x69 }, /* 13818-3 */
  { CODEC_ID_MP2       , 0x69 }, /* 11172-3 */
  { CODEC_ID_MPEG1VIDEO, 0x6A }, /* 11172-2 */
  { CODEC_ID_MP3       , 0x6B }, /* 11172-3 */
  { CODEC_ID_MJPEG     , 0x6C }, /* 10918-1 */
  { CODEC_ID_PNG       , 0x6D },
  { CODEC_ID_JPEG2000  , 0x6E }, /* 15444-1 */
  { CODEC_ID_VC1       , 0xA3 },
  { CODEC_ID_DIRAC     , 0xA4 },
  { CODEC_ID_AC3       , 0xA5 },
  { CODEC_ID_DTS       , 0xA9 }, /* mp4ra.org */
  { CODEC_ID_VORBIS    , 0xDD }, /* non standard, gpac uses it */
  { CODEC_ID_DVD_SUBTITLE, 0xE0 }, /* non standard, see unsupported-embedded-subs-2.mp4 */
  { CODEC_ID_QCELP     , 0xE1 },
  { CODEC_ID_MPEG4SYSTEMS, 0x01 },
  { CODEC_ID_MPEG4SYSTEMS, 0x02 },
  { CODEC_ID_NONE      ,    0 },
};


#define CODEC_ID_FIRST_AUDIO 0x10000
#define CODEC_ID_FIRST_SUBTITLE 0x17000
#define CODEC_ID_FIRST_UNKNOWN 0x18000



typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next; ///< ptr to next entry
  int64_t dts; ///< dts of keyframe
  uint64_t offs;  ///< offset in file
};

typedef struct {
  index_entry *idxhh;  ///< head of head list
  index_entry *idxht; ///< tail of head list

  int nclients;
  lives_clip_data_t **clients;
  pthread_mutex_t mutex;
} index_container_t;


typedef struct {
  int fd;
  boolean inited;
  boolean has_video;
  boolean has_audio;
  boolean black_fill;
  int vididx;
  AVStream *vidst;
  int64_t input_position;
  int64_t data_start;
  off_t filesize;

  int64_t start_dts;

  MpegTSContext *ts;

  AVFormatContext *s;
  AVCodec *codec;
  AVCodecContext *ctx;
  AVFrame *picture;
  AVPacket avpkt;
  int64_t last_frame; ///< last frame displayed
  index_container_t *idxc;
  boolean got_eof;
  boolean expect_eof;
} lives_mpegts_priv_t;









#endif /* LIVES_MPEGTS_H */
