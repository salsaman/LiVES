// LiVES - avformat decoder plugin
// (c) G. Finch 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


typedef struct  {
    // input
    int fd;
    AVInputFormat *fmt;
    AVFormatContext *ic;
    AVCodecContext *ctx;
    AVFrame *pFrame;
    AVPacket packet;
    boolean packet_valid;
    
    int astream;
    int vstream;

    int last_frame;
} lives_av_priv_t;

