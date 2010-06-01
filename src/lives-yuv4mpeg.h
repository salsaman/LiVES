// yuv4mpeg.h
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2010
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#ifndef YUV4MPEG_H
#define YUV4MPEG_H

#include <yuv4mpeg.h>

#define YUV4_TYPE_GENERIC 0
#define YUV4_TYPE_FW 1
#define YUV4_TYPE_TV 2


typedef struct {
  gint type;
  gint cardno;
  y4m_stream_info_t streaminfo;
  y4m_frame_info_t frameinfo;
  y4m_ratio_t sar;
  y4m_ratio_t dar;
  gchar *name;
  gchar *filename;
  int fd;
  gint hsize;
  gint vsize;
  void **pixel_data;
  gboolean ready;
} lives_yuv4m_t;

void weed_layer_set_from_yuv4m (weed_plant_t *layer, file *);

// callbacks
void on_open_yuv4m_activate (GtkMenuItem *, gpointer);
void on_live_tvcard_activate (GtkMenuItem *, gpointer);
void on_live_fw_activate (GtkMenuItem *, gpointer);

void lives_yuv_stream_stop_read (lives_yuv4m_t *);


// not used
gboolean lives_yuv_stream_start_write (lives_yuv4m_t *, const gchar *filename, gint hsize, gint vsize, gdouble fps);
gboolean lives_yuv_stream_write_frame (lives_yuv4m_t *, void *pixel_data);
void lives_yuv_stream_stop_write (lives_yuv4m_t *);


typedef struct {
  GtkWidget *dialog;
  GtkWidget *card_spin;
  GtkWidget * channel_spin;
} lives_card_w;




#endif


