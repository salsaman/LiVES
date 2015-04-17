// LiVES - flv decoder plugin
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

// based on code

/*
 * FLV demuxer
 * Copyright (c) 2003 The FFmpeg Project
 *
 */


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



// TODO - this is a lazy implementation - for speed we should use bi-directional skip-lists

typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next; ///< ptr to next entry
  int32_t dts; ///< dts of keyframe
  int32_t dts_max;    ///< max dts for this keyframe
  uint64_t offs;  ///< offset in file
};


typedef struct {
  index_entry *idxhh;  ///< head of head list
  index_entry *idxht; ///< tail of head list
  index_entry *idxth; ///< head of tail list

  int nclients;
  lives_clip_data_t **clients;
  pthread_mutex_t mutex;
} index_container_t;



typedef struct {
  int fd;
  int pack_offset;
  boolean inited;
  int64_t input_position;
  int64_t data_start;
  AVCodec *codec;
  AVCodecContext *ctx;
  AVFrame *picture;
  AVPacket avpkt;
  int64_t last_frame; ///< last frame displayed
  index_container_t *idxc;
} lives_flv_priv_t;

index_entry *index_upto(const lives_clip_data_t *, int pts);
index_entry *index_downto(const lives_clip_data_t *, int pts);

