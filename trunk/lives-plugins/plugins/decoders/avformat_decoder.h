// LiVES - avformat decoder plugin
// (c) G. Finch 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


typedef struct  {
  int fd;

  AVInputFormat *fmt;
  AVFormatContext *ic;
  AVCodecContext *ctx;
  AVFrame *pFrame;
  AVPacket packet;

  boolean fps_avg;
  boolean black_fill;
  boolean inited;
  boolean longer_seek;

  int astream;
  int vstream;

  int64_t found_pts;

  int last_frame;

  size_t pkt_offs;
} lives_av_priv_t;

