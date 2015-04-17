// stream.h
// LiVES
// (c) G. Finch 2008 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_STREAM_H
#define HAS_LIVES_STREAM_H


typedef struct {
  uint32_t stream_id;
  uint32_t flags;

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
  boolean data_ready;
  void *handle;

  volatile boolean reading;
  void *buffer;
  volatile size_t bufoffs;
} lives_vstream_t;

// stream packet tpyes
#define LIVES_STREAM_TYPE_VIDEO 1

// video stream flags
#define LIVES_VSTREAM_FLAGS_IS_CONTINUATION 1<<0

// video compression types
#define LIVES_VSTREAM_COMPRESSION_NONE 0


void lives2lives_read_stream(const char *host, int port);
void weed_layer_set_from_lives2lives(weed_plant_t *layer, int clip, lives_vstream_t *lstream);
void on_open_lives2lives_activate(LiVESMenuItem *, livespointer);
void on_send_lives2lives_activate(LiVESMenuItem *, livespointer);

typedef struct {
  LiVESWidget *dialog;
  LiVESWidget *entry1;
  LiVESWidget *entry2;
  LiVESWidget *entry3;
  LiVESWidget *entry4;
  LiVESWidget *port_spin;
  LiVESWidget *rb_anyhost;
} lives_pandh_w;

lives_pandh_w *create_pandh_dialog(int type);




#endif // HAS_LIVES_STREAM_H
