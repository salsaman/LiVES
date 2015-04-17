// LiVES - ogg/theora/vorbis decoder plugin
// (c) G. Finch 2008 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


// ogg

#define PAGE_HEADER_BYTES 27


typedef struct {
  int fd;

  off64_t total_bytes;

  ogg_sync_state  oy;
  ogg_page        current_page;
  ogg_packet      op;

  char *buffer;

  /* current_page is valid */
  int page_valid;

  int y_width;
  int y_height;
  int uv_width;

} ogg_t;



typedef struct {
  uint32_t fourcc_priv;
  ogg_stream_state os;

  int header_packets_read;
  int header_packets_needed;

  int64_t last_granulepos;

  int keyframe_granule_shift;

} stream_priv_t;




// theora
#define FOURCC_THEORA    MK_FOURCC('T','H','R','A')

#ifdef HAVE_THEORA
typedef struct {
  theora_info    ti;
  theora_comment tc;
  theora_state   ts;
} theora_priv_t;
#endif


#ifdef HAVE_DIRAC
typedef struct {
  SchroDecoder *schrodec;
  SchroFrame *schroframe;
} dirac_priv_t;
#endif

#define LIVES_TIMESTAMP_UNDEFINED 0x8000000000000000LL


// vorbis
#define FOURCC_VORBIS    MK_FOURCC('V','B','I','S')



// dirac
#define FOURCC_DIRAC    MK_FOURCC('D','R','A','C')


typedef struct lives_stream_s {
  int type;
  uint32_t fourcc;
  uint64_t version;  // major * 1000000 + minor * 1000 + subminor
  int64_t data_start;

  stream_priv_t *stpriv;
  int stream_id;
  int samplerate;
  int fps_num;
  int fps_denom;
  int64_t nframes;
  double duration;

  void *ext_data;
  size_t ext_size;

} lives_in_stream;


#define LIVES_STREAM_AUDIO 1
#define LIVES_STREAM_VIDEO 2


#define BYTES_TO_READ 8500

#define DIRAC_EXTRA_FRAMES 2

typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next;
  index_entry *prev;
  int64_t value;    // granulepos for theora, frame for dirac
  int64_t pagepos;
  int64_t pagepos_end; // only used for dirac
};


typedef struct {
  index_entry *idx;

  int nclients;
  lives_clip_data_t **clients;
  pthread_mutex_t mutex;
} index_container_t;



typedef struct {
  ogg_t *opriv;
  lives_in_stream *astream;
  lives_in_stream *vstream;

  boolean inited;

#ifdef HAVE_THEORA
  theora_priv_t *tpriv;
#endif

#ifdef HAVE_DIRAC
  dirac_priv_t *dpriv;
#endif

  // seeking
  off64_t input_position;
  off64_t current_pos;
  int skip;
  int64_t last_kframe;
  int64_t last_frame;
  int64_t cframe;
  int64_t kframe_offset;
  int64_t cpagepos;
  boolean ignore_packets;
  boolean frame_out;

  // indexing
  index_container_t *idxc;
} lives_ogg_priv_t;




// bitstream functions from vlc



typedef struct bs_s {
  uint8_t *p_start;
  uint8_t *p;
  uint8_t *p_end;

  ssize_t  i_left;    /**< i_count number of available bits */
} bs_t;

static inline void bs_init(bs_t *s, const void *p_data, size_t i_data) {
  s->p_start = (void *)p_data;
  s->p       = s->p_start;
  s->p_end   = s->p_start + i_data;
  s->i_left  = 8;
}

static inline int bs_pos(const bs_t *s) {
  return (8 * (s->p - s->p_start) + 8 - s->i_left);
}

static inline int bs_eof(const bs_t *s) {
  return (s->p >= s->p_end ? 1: 0);
}

static inline uint32_t bs_read(bs_t *s, int i_count) {
  static const uint32_t i_mask[33] = {
    0x00,
    0x01,      0x03,      0x07,      0x0f,
    0x1f,      0x3f,      0x7f,      0xff,
    0x1ff,     0x3ff,     0x7ff,     0xfff,
    0x1fff,    0x3fff,    0x7fff,    0xffff,
    0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
    0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
    0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
    0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff
  };
  int      i_shr;
  uint32_t i_result = 0;

  while (i_count > 0) {
    if (s->p >= s->p_end) {
      break;
    }

    if ((i_shr = s->i_left - i_count) >= 0) {
      /* more in the buffer than requested */
      i_result |= (*s->p >> i_shr)&i_mask[i_count];
      s->i_left -= i_count;
      if (s->i_left == 0) {
        s->p++;
        s->i_left = 8;
      }
      return (i_result);
    } else {
      /* less in the buffer than requested */
      i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
      i_count  -= s->i_left;
      s->p++;
      s->i_left = 8;
    }
  }

  return (i_result);
}

static inline uint32_t bs_read1(bs_t *s) {
  if (s->p < s->p_end) {
    unsigned int i_result;

    s->i_left--;
    i_result = (*s->p >> s->i_left)&0x01;
    if (s->i_left == 0) {
      s->p++;
      s->i_left = 8;
    }
    return i_result;
  }

  return 0;
}

static inline uint32_t bs_show(bs_t *s, int i_count) {
  bs_t     s_tmp = *s;
  return bs_read(&s_tmp, i_count);
}

static inline void bs_skip(bs_t *s, ssize_t i_count) {
  s->i_left -= i_count;

  if (s->i_left <= 0) {
    const int i_bytes = (-s->i_left + 8) / 8;

    s->p += i_bytes;
    s->i_left += 8 * i_bytes;
  }
}

static inline void bs_write(bs_t *s, int i_count, uint32_t i_bits) {
  while (i_count > 0) {
    if (s->p >= s->p_end) {
      break;
    }

    i_count--;

    if ((i_bits >> i_count)&0x01) {
      *s->p |= 1 << (s->i_left - 1);
    } else {
      *s->p &= ~(1 << (s->i_left - 1));
    }
    s->i_left--;
    if (s->i_left == 0) {
      s->p++;
      s->i_left = 8;
    }
  }
}

static inline void bs_align(bs_t *s) {
  if (s->i_left != 8) {
    s->i_left = 8;
    s->p++;
  }
}

static inline void bs_align_0(bs_t *s) {
  if (s->i_left != 8) {
    bs_write(s, s->i_left, 0);
  }
}

static inline void bs_align_1(bs_t *s) {
  while (s->i_left != 8) {
    bs_write(s, 1, 1);
  }
}


