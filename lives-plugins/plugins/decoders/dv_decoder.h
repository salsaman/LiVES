// LiVES - dv decoder plugin
// (c) G. Finch 2008 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define DV_HEADER_SIZE (6*80) /* 6 DIF blocks */

typedef struct  {
  // input
  int fd;
  boolean inited;
  dv_decoder_t *dv_dec;
  int frame_size;
  boolean is_pal;

  // audio output
  int16_t *audio_buffers[4];
  int16_t *audio;
  int audio_fd;
} lives_dv_priv_t;

