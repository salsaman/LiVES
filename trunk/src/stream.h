// stream.h
// LiVES
// (c) G. Finch 2008 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _HAS_STREAM_H
#define _HAS_STREAM_H


typedef struct {
  guint32 stream_id;
  guint32 flags;

  int64_t timecode;
  int hsize;
  int vsize;
  double fps;
  int palette;
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  int compression_type;

  // TODO - use lives_stream_control_t for these
  size_t dsize;
  gboolean data_ready;
  void *handle;

  volatile gboolean reading;
  void *buffer;
  volatile size_t bufoffs;
} lives_vstream_t;

// stream packet tpyes
#define LIVES_STREAM_TYPE_VIDEO 1

// video stream flags
#define LIVES_VSTREAM_FLAGS_IS_CONTINUATION 1<<0

// video compression types
#define LIVES_VSTREAM_COMPRESSION_NONE 0


void lives2lives_read_stream (const gchar *host, int port);
void weed_layer_set_from_lives2lives (weed_plant_t *layer, gint clip, lives_vstream_t *lstream);
void on_open_lives2lives_activate (GtkMenuItem *, gpointer);
void on_send_lives2lives_activate (GtkMenuItem *, gpointer);

typedef struct {
  GtkWidget *dialog;
  GtkWidget *entry1;
  GtkWidget *entry2;
  GtkWidget *entry3;
  GtkWidget *entry4;
  GtkWidget *port_spin;
  GtkWidget *rb_anyhost;
} lives_pandh_w;

lives_pandh_w* create_pandh_dialog (gint type);




#endif // _HAS_STREAM_H
