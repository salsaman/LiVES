// LiVES - avformat decoder plugin
// (c) G. Finch 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

//#define TEST_CACHING - not working, frames seem to jump around with reverse pb
#ifdef TEST_CACHING
typedef struct priv_cache_t priv_cache_t;
struct priv_cache_t {
  priv_cache_t *next;
  int64_t pts;
  AVFrame *frame;
};
#endif

typedef struct  {
  int fd;

  AVInputFormat *fmt;
  AVFormatContext *ic;
  AVCodecContext *ctx;
  AVFrame *pFrame;
  AVPacket packet;

  boolean fps_avg;
  boolean inited;
  boolean longer_seek;
  boolean needs_packet;
  boolean pkt_inited;

  int astream;
  int vstream;

  int64_t found_pts;

  int last_frame;

  size_t pkt_offs;
#ifdef TEST_CACHING
  int cachemax;
  priv_cache_t *cache;
#endif
} lives_av_priv_t;

