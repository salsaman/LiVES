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

  /* File position of the current page */
  int64_t     current_page_pos;

  int64_t end_pos;
 
  /* Serial  number of the last page of the entire file */
  int last_page_serialno;
   
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

typedef struct
{
  theora_info    ti;
  theora_comment tc;
  theora_state   ts;
} theora_priv_t;



#define LIVES_TIMESTAMP_UNDEFINED 0x8000000000000000LL


// vorbis
#define FOURCC_VORBIS    MK_FOURCC('V','B','I','S') 


typedef struct lives_stream_s {
  int type;
  uint32_t fourcc;
  stream_priv_t *priv;
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


static int ogg_data_process(yuv_buffer *yuv, boolean cont);
