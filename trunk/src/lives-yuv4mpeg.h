// yuv4mpeg.h
// LiVES (lives-exe)
// (c) G. Finch 2004
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#ifndef YUV4MPEG_H
#define YUV4MPEG_H

#include <yuv4mpeg.h>

typedef struct {
  y4m_stream_info_t streaminfo;
  y4m_frame_info_t frameinfo;
  y4m_ratio_t sar;
  y4m_ratio_t dar;
  int fd;
  int hsize;
  int vsize;
} lives_yuv4m_t;

void weed_layer_set_from_yuv4m (weed_plant_t *layer, void *src);

// callback
void on_open_yuv4m_activate (GtkMenuItem *menuitem, gpointer user_data);

lives_yuv4m_t *lives_yuv4mpeg_alloc (void);
gboolean lives_yuv_stream_start_read (lives_yuv4m_t *, gchar *filename);
void lives_yuv_stream_stop_read (lives_yuv4m_t *);

gboolean lives_yuv_stream_start_write (lives_yuv4m_t * yuv4mpeg, gchar *filename, gint hsize, gint vsize, gdouble fps);
gboolean lives_yuv_stream_write_frame (lives_yuv4m_t *yuv4mpeg, void *pixel_data);
void lives_yuv_stream_stop_write (lives_yuv4m_t *yuv4mpeg);

#endif


