// LiVES - ogg/theora/vorbis decoder plugin
// (c) G. Finch 2008 <salsaman@xs4all.nl>
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
  int64_t prev_granulepos;      /* Granulepos of the previous page */

  int keyframe_granule_shift;

  int64_t frame_counter;

  /* Set this during seeking */
  int do_sync;

  /* Position of the current and previous page */
  int64_t page_pos;
  int64_t prev_page_pos;
} stream_priv_t;




// theora
#define FOURCC_THEORA    MK_FOURCC('T','H','R','A')

#ifdef HAVE_THEORA
typedef struct
{
  theora_info    ti;
  theora_comment tc;
  theora_state   ts;
} theora_priv_t;
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


typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next;
  index_entry *prev;
  int64_t granulepos;
  int64_t pagepos;
};





typedef struct {
  ogg_t *opriv;
  lives_in_stream *astream;
  lives_in_stream *vstream;

#ifdef HAVE_THEORA
  theora_priv_t *tpriv;
#endif

  int64_t data_start;

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
  index_entry *idx;
} lives_ogg_priv_t;



// little-endian
#define get_uint32(p) (*p<<24|*(p+1)<<16|*(p+2)<<8|*(p+3))
