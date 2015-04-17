// yuv4mpeg.h
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2013
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#ifndef YUV4MPEG_H
#define YUV4MPEG_H

#include <yuv4mpeg.h>

#define YUV4_TYPE_GENERIC 0
#define YUV4_TYPE_FW 1
#define YUV4_TYPE_TV 2


typedef struct {
  int type;
  int cardno;
  y4m_stream_info_t streaminfo;
  y4m_frame_info_t frameinfo;
  y4m_ratio_t sar;
  y4m_ratio_t dar;
  char *name;
  char *filename;
  int fd;
  int hsize;
  int vsize;
  void **pixel_data;
  boolean ready;
} lives_yuv4m_t;

void weed_layer_set_from_yuv4m(weed_plant_t *layer, lives_clip_t *);

// callbacks
void on_open_yuv4m_activate(LiVESMenuItem *, livespointer);
void on_live_tvcard_activate(LiVESMenuItem *, livespointer);
void on_live_fw_activate(LiVESMenuItem *, livespointer);

void lives_yuv_stream_stop_read(lives_yuv4m_t *);


/// not used
boolean lives_yuv_stream_start_write(lives_yuv4m_t *, const char *filename, int hsize, int vsize, double fps);
boolean lives_yuv_stream_write_frame(lives_yuv4m_t *, void *pixel_data);
void lives_yuv_stream_stop_write(lives_yuv4m_t *);


typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *card_spin;
  LiVESWidget *channel_spin;
} lives_card_w;




#endif


