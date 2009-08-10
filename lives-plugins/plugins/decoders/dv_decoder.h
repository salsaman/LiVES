// LiVES - dv decoder plugin
// (c) G. Finch 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define DV_HEADER_SIZE (6*80) /* 6 DIF blocks */

typedef struct  {
  int fd;
  dv_decoder_t *dv_dec;
  int frame_size;
  boolean is_pal;
} dv_priv_t;

